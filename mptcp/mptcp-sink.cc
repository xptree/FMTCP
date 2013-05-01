#include "ip.h"
#include "flags.h"
#include "tcp-sink.h"
#include "mptcp-sink.h"
#include "classifier.h"
#include "hdr_qs.h"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
using namespace std;

class MptcpSink;
class MptcpSubflowSink;

typedef unsigned int uint;
#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

static class MptcpSinkClass : public TclClass {
public:
	MptcpSinkClass() : TclClass("Agent/MPTCPSink") {}
	TclObject* create(int, const char* const*) {
	  return (new MptcpSink());
	}
} class_mptcp_sink_agent;

static class MptcpSubflowSinkClass : public TclClass {
public:
	MptcpSubflowSinkClass() : TclClass("Agent/TCPSink/MPTCPSubflowSink") {}
	TclObject* create(int, const char* const*) {
	  return (new MptcpSubflowSink(new Acker));
	}
} class_mptcp_subflow_sink;

void MptcpSink::totrace(string msg) {
  if (tracefile_ == NULL) tracefile_ = fopen("MptcpSink.tr", "w");
  fprintf(tracefile_, "At %lf, %s\n", (double)Scheduler::instance().clock(), msg.c_str());
}

void MptcpSink::totest(string msg) {
	if (testinfo_ == NULL) testinfo_ = fopen("MptcpSinkTest.tr", "w");
	fprintf(testinfo_, "At %lf, %s\n", (double)Scheduler::instance().clock(), msg.c_str());
}

void MptcpSubflowSink::totrace(string msg) {
  string tmp;
  tmp += subflow_id_ + '0';
  if (tracefile_ == NULL) tracefile_ = fopen(("MptcpSubflowSink" + tmp + ".tr").c_str(), "w");
  fprintf(tracefile_, "At time %lf, %s\n", (double)Scheduler::instance().clock(), msg.c_str());
}

MptcpSink::MptcpSink() : Agent(PT_ACK), next_(0) {
  memset(buffer_,0,sizeof(buffer_));
  tracefile_ = NULL;
  testinfo_ = NULL;
}

MptcpSink::~MptcpSink() {
	fclose(tracefile_);
	fclose(testinfo_);
  for(uint i = 0; i < subflow_.size(); i++)
    delete subflow_[i];
}

void MptcpSink::delay_bind_init_all(){
	delay_bind_init_one("window_");
	delay_bind_init_one("block_size_");
	Agent::delay_bind_init_all();
	//reset();
}

int MptcpSink::delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer){
	if (delay_bind(varName, localName, "window_", &wnd_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "block_size_", &block_packets, tracer)) return TCL_OK;
	return Agent::delay_bind_dispatch(varName, localName, tracer);
}

SubflowSink* MptcpSink::find_subflow(int addr) {
  for(uint i = 0; i < subflow_.size(); i++)
    if (subflow_[i] != NULL && subflow_[i]->addr_ == addr) return subflow_[i];
  return NULL;
}

SubflowSink* MptcpSink::find_subflow(int addr, int port) {
  for(uint i = 0; i < subflow_.size(); i++)
    if (subflow_[i] != NULL && subflow_[i]->addr_ == addr && subflow_[i]->port_ == port) return subflow_[i];
  return NULL;
}


//Add a new destination
void MptcpSink::add_destination(int addr, int port) {
  Dstinfor t;
  t.addr_ = addr;
  t.port_ = port;
  dstinfor_.push_back(t);
}

//Check whether we can route packet to (addr, port) using subflow sid
bool MptcpSink::check_routable(int sid, int addr, int port) {
  Packet *p = allocpkt();
  hdr_ip *iph = hdr_ip::access(p);
  iph->daddr() = addr;
  iph->dport() = port;
  bool result = (static_cast<Classifier*> (subflow_[sid]->target_) ->classify(p) > 0)? true : false;
  Packet::free(p);
  return result;
}

//forward the packet to subflow, but do nothing
void MptcpSink::recv(Packet *pkt, Handler *h) {
  hdr_ip *iph = hdr_ip::access(pkt);
  SubflowSink *sf = find_subflow(iph->daddr());
  if (sf == NULL) {
    cerr << "Fatal: MptcpSink::recv: cannot find subflow for the packet" << endl;
    abort();
  }
  
  sf->tcpsink_->recv(pkt, h);
  
}

int MptcpSink::command(int argc, const char* const* argv) {

 if (strcmp(argv[1], "reset") == 0) {
  
    //reset everything
    next_ = 0;
    memset(buffer_,0,sizeof(buffer_));
    tracefile_ = NULL;
    
    //Reset every subflow
    vector<bool> used;
    for(uint i = 0; i < dstinfor_.size(); i++) 
      used.push_back(false);
    for(uint i = 0; i < subflow_.size(); i++) 
      for(uint j = 0; j < dstinfor_.size(); j++) {
      
        if (used[j]) continue; //this destination has already been assigned
        
        if (check_routable(i, dstinfor_[j].addr_, dstinfor_[j].port_)) {
          subflow_[i]->daddr_ = dstinfor_[j].addr_;
          subflow_[i]->dport_ = dstinfor_[j].port_;
          subflow_[i]->tcpsink_->daddr() = dstinfor_[j].addr_;
          subflow_[i]->tcpsink_->dport() = dstinfor_[j].port_;
          used[j] = true;
          break;
        }
      
      }
    
    return TCL_OK;
  }
  
  else if (strcmp(argv[1], "attach-tcpsink") == 0) {
    //attach a mptcp subflow (sending agent)
    MptcpSubflowSink *subflow_agent = (MptcpSubflowSink*) TclObject::lookup(argv[2]);
    subflow_agent->mptcpsink_setdata(subflow_.size(), this);
    SubflowSink *sf = new SubflowSink;
    sf->tcpsink_ = subflow_agent;
    sf->id_ = subflow_.size();
    sf->addr_ = subflow_agent->addr(); 
    sf->port_ = subflow_agent->port();
    subflow_.push_back(sf);   
    
    return TCL_OK;
  }
  
  else if (strcmp(argv[1], "set-multihome-core") == 0) {
    core_ = (Classifier*) TclObject::lookup(argv[2]);
    if (core_ == NULL) {
      return TCL_ERROR;
    }
    return TCL_OK;
  }
  
  else if (strcmp(argv[1], "add-multihome-destination") == 0) {
    add_destination(atoi(argv[2]), atoi(argv[3]));
    return TCL_OK;
  }
  
  else if (strcmp(argv[1], "add-multihome-interface") == 0) {
    SubflowSink *sf = find_subflow(atoi(argv[2]));
    if (sf == NULL) {
      cerr << "Fatal: MptcpAgent::command, cannot find subflow in add-multihome-interface" << endl;
      abort();
    }
    sf->tcpsink_->port() = atoi(argv[3]);
    sf->port_ = atoi(argv[3]);
    sf->target_ = (NsObject*) TclObject::lookup(argv[4]);
    sf->link_ = (NsObject*) TclObject::lookup(argv[5]);
    if (sf->target_ == NULL || sf->link_ == NULL) {
      return TCL_ERROR;
    }
    return TCL_OK;
  }
  
  return (Agent::command(argc, argv));

}

//update the receving buffer/window and seqno mapping, possibly reject if the receiving buffer is already full
bool MptcpSink::update(int mptcp_seqno, int subflow_id) {

  if (mptcp_seqno < next_) return true;
  int winsize = (wnd_ < recv_buffer_size ? wnd_ : recv_buffer_size);
  if (mptcp_seqno - (next_-1) > winsize) return false; //rejected
  
  buffer_[mptcp_seqno % recv_buffer_size] = true;
  
  //cerr << "MPTCP sink: next_ = " << next_ << endl;
  
  int next_offset = next_ % recv_buffer_size;
  while (buffer_[next_offset]) {
    char tmp[100]; 	
	if (next_ >= (block_packets - 1) && next_ % block_packets == (block_packets - 1)) {
		memset(tmp,0,sizeof(tmp));
		sprintf(tmp, "block seqno = %d", (next_+1) / block_packets - 1);
		totrace(tmp);
		double now = Scheduler::instance().clock();
		double throughput = (next_+1)*MSS*8/(now*1000);
		memset(tmp,0,sizeof(tmp));
		sprintf(tmp, "throughput = %d kbps", (int)throughput);
		totest(tmp);
	}
    //sprintf(tmp, "mptcp seqno = %d", next_);
    //totrace(tmp);
    buffer_[next_offset] = false;
    next_ ++;
    next_offset = (next_offset + 1) % recv_buffer_size;
  }
  
  return true;

}

MptcpSubflowSink::MptcpSubflowSink(Acker *acker) : TcpSink(acker), next_(0) {
  for(int i = 0; i < recv_buffer_size; i++)
    buffer_[i] = -1;
}

void MptcpSubflowSink::delay_bind_init_all(){
	delay_bind_init_one("window_");	
	Agent::delay_bind_init_all();
	//reset();
}

int MptcpSubflowSink::delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer){
	if (delay_bind(varName, localName, "window_", &wnd_, tracer)) return TCL_OK;	
	return Agent::delay_bind_dispatch(varName, localName, tracer);
}

MptcpSubflowSink::~MptcpSubflowSink() {
	fclose(tracefile_);	
}

//the main reception path for MptcpSubflowSink

void MptcpSubflowSink::recv(Packet *pkt, Handler*) {
  

  hdr_tcp *th = hdr_tcp::access(pkt);
  //check if packet is from previous incarnation
  if (th->ts() < lastreset_) {
    Packet::free(pkt);
    return;
  }
  
  //try to update this packet to FmtcpSink
  if (core_ == NULL) {
    cerr << "Error: MptcpSubflowSink: core not set" << endl;
    abort();
  }
  
  char tmp[100]; 
  //memset(tmp,0,sizeof(tmp));
  //sprintf(tmp, "recving packet, mptcp_seqno = %d, seqno = %d, last_update_ = %d, next_ = %d", th->mptcp_seqno(), th->seqno(), last_update_, next_);
  //totrace(tmp);
  
  memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "subflow %d : mptcp_seqno = %d, seqno = %d", subflow_id_, th->mptcp_seqno(), th->seqno());
  totrace(tmp);
  core_->totest(tmp);
  
  int winsize = (wnd_ < recv_buffer_size ? wnd_ : recv_buffer_size);
  if (th->seqno() >= next_ && th->seqno() - next_ + 1 <= winsize && th->mptcp_seqno() > core_->get_last_ack() && (th->mptcp_seqno() - core_->get_last_ack()) <= winsize) {
    
    buffer_[th->seqno() % recv_buffer_size] = th->mptcp_seqno();
    
    /*while (next_ - last_update_ <= winsize && buffer_[next_ % recv_buffer_size] != -1) {
      char tmp[100]; memset(tmp,0,sizeof(tmp));
      sprintf(tmp, "next_ = %d, buffer_[%d] = %d", next_, next_ % recv_buffer_size, buffer_[next_ % recv_buffer_size]);
      totrace(tmp);
      next_ ++;
    }
      
    int last_offset = (last_update_ + 1) % recv_buffer_size;
    while (buffer_[last_offset] != -1) {
      bool flag = core_->update(buffer_[last_offset], subflow_id_);
      if (flag) {
        buffer_[last_offset] = -1;
        last_update_ ++;
        last_offset = (last_offset + 1) % recv_buffer_size;
      }
      else break;
    }*/
	int last_offset = next_ % recv_buffer_size;
    while (buffer_[last_offset] != -1) {
		bool flag = core_->update(buffer_[last_offset], subflow_id_);
		buffer_[last_offset] = -1;
		next_++;
		last_offset = next_ % recv_buffer_size;		
    }
    
  }
  
  //Send ack back
  
  Packet *npkt = allocpkt();
  Packet *opkt = pkt;
  //opkt: the old packet
  //nptk: the ack packet constructed
  double now = Scheduler::instance().clock();
  
  hdr_tcp *otcp = hdr_tcp::access(opkt);
  hdr_tcp *ntcp = hdr_tcp::access(npkt);
  
  ntcp->seqno() = next_ - 1;  
  ntcp->mptcp_seqno() = core_->get_last_ack();  // seqno 
  ntcp->ts() = now;
  ntcp->ts_echo() = otcp->ts();
  
  send(npkt, 0);
  
  Packet::free(pkt);


}

