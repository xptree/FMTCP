#include "tcp.h"
#include "agent.h"
#include "packet.h"
#include "classifier.h"
#include "ip.h"
#include "flags.h"
#include "hdr_qs.h"
#include "random.h"
#include "tcp-sink.h"

#include "fmtcpconst.h"
#include "fmtcp-sink.h"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

using namespace std;


static class FmtcpSinkClass : public TclClass {
public:
	FmtcpSinkClass() : TclClass("Agent/FMTCPSink") {}
	TclObject* create(int, const char* const*) {
	  return (new FmtcpSink());
	}
} class_fmtcp_sink_agent;

static class FmtcpSubflowSinkClass : public TclClass {
public:
	FmtcpSubflowSinkClass() : TclClass("Agent/TCPSink/FMTCPSubflowSink") {}
	TclObject* create(int, const char* const*) {
	  return (new FmtcpSubflowSink());
	}
} class_fmtcp_subflow_sink;

FmtcpSink::FmtcpSink() : Agent(PT_ACK), tracefile_(NULL), next_(0) {
  tracefile_ = fopen("FmtcpSink.tr", "w");
  assert(tracefile_);
  testinfo_ = fopen("FmtcpSinkTest.tr", "w");
  assert(testinfo_);

  memset(buffer_,0,sizeof(buffer_));
}

FmtcpSink::~FmtcpSink() {
  fclose(tracefile_);
  fclose(testinfo_);
  for(uint i = 0; i < subflow_.size(); i++)
    delete subflow_[i];
}


void FmtcpSink::delay_bind_init_all(){
	delay_bind_init_one("window_");
	delay_bind_init_one("block_size_");
	Agent::delay_bind_init_all();
	//reset();
}

int FmtcpSink::delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer){
	if (delay_bind(varName, localName, "window_", &wnd_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "block_size_", &block_packets, tracer)) return TCL_OK;
	return Agent::delay_bind_dispatch(varName, localName, tracer);
}

void FmtcpSink::totrace(string msg) {
  fprintf(tracefile_, "At %lf, %s\n", Scheduler::instance().clock(), msg.c_str());
}

void FmtcpSink::totest(string msg) {
  fprintf(testinfo_, "At time %lf, %s\n", Scheduler::instance().clock(), msg.c_str());
}

SubflowSink* FmtcpSink::find_subflow(int addr) {
  for(uint i = 0; i < subflow_.size(); i++)
    if (subflow_[i] != NULL && subflow_[i]->addr_ == addr) return subflow_[i];
  return NULL;
}

SubflowSink* FmtcpSink::find_subflow(int addr, int port) {
  for(uint i = 0; i < subflow_.size(); i++)
    if (subflow_[i] != NULL && subflow_[i]->addr_ == addr && subflow_[i]->port_ == port) return subflow_[i];
  return NULL;
}

//Add a new destination
void FmtcpSink::add_destination(int addr, int port) {
  Dstinfor t;
  t.addr_ = addr;
  t.port_ = port;
  dstinfor_.push_back(t);
}

//Check whether we can route packet to (addr, port) using subflow sid
bool FmtcpSink::check_routable(int sid, int addr, int port) {
  Packet *p = allocpkt();
  hdr_ip *iph = hdr_ip::access(p);
  iph->daddr() = addr;
  iph->dport() = port;
  bool result = (static_cast<Classifier*> (subflow_[sid]->target_) ->classify(p) > 0)? true : false;
  Packet::free(p);
  return result;
}

//forward the packet to subflow, but do nothing
void FmtcpSink::recv(Packet* pkt, Handler* h) {
  hdr_ip *iph = hdr_ip::access(pkt);
  SubflowSink *sf = find_subflow(iph->daddr());
  if (sf == NULL) {
    cerr << "Fatal: MptcpSink::recv: cannot find subflow for the packet" << endl;
    abort();
  }
  sf->tcpsink_->recv(pkt, h);
}

int FmtcpSink::command(int argc, const char* const* argv) {

 if (strcmp(argv[1], "reset") == 0) {
  
    //reset everything
    next_ = 0;
    memset(buffer_,0,sizeof(buffer_));
    
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
    FmtcpSubflowSink *subflow_agent = (FmtcpSubflowSink*) TclObject::lookup(argv[2]);
    subflow_agent->fmtcpsink_setdata(subflow_.size(), this);
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


pair<int,int> FmtcpSink::update(int fmtcp_seqno) {

  //char tmp[100]; memset(tmp,0,sizeof(tmp));
  //string str = "";
  //sprintf(tmp, "recved fmtcp_seqno = %d, next_ = %d", fmtcp_seqno, next_);
  //totrace(tmp);
  /*for(int i = 0; i < recv_buffer_block; i++) {
    memset(tmp,0,sizeof(tmp));
    sprintf(tmp, "%d ", buffer_[i]);
    str += tmp;
  }*/
  //totrace(str.c_str());
  
  pair<int,int> ret;
  
  if (fmtcp_seqno < next_) return make_pair(fmtcp_seqno, block_packets);
  if (fmtcp_seqno > next_-1 + (int)(wnd_/block_packets)) return make_pair(fmtcp_seqno, 0);
  int offset = fmtcp_seqno % recv_buffer_block;
  char tmp[100]; 
  memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "block seqno = %d num = %d", fmtcp_seqno, buffer_[offset]);      
  totest(tmp);
  if (++ buffer_[offset] >= block_packets) {
    ret = make_pair(fmtcp_seqno, buffer_[offset]);
    int next_offset = next_ % recv_buffer_block;
    //while (buffer_[next_offset] >= recv_buffer_block) {// error correction
    while (buffer_[next_offset] >= block_packets) { 
	  
      //char tmp[100]; 
	  memset(tmp,0,sizeof(tmp));
      sprintf(tmp, "block seqno = %d", next_);      
      totrace(tmp);
	  double now = Scheduler::instance().clock();
	  double throughput = (next_+1)*block_packets*MSS*8/(1000*now);
	  memset(tmp,0,sizeof(tmp));
	  sprintf(tmp, "throughput = %d kbps", (int)throughput);
	  totest(tmp);
	  
      buffer_[next_offset] = 0;
      next_offset = (next_offset + 1) % recv_buffer_block;
      next_ ++;
    }
  }
  else ret = make_pair(fmtcp_seqno, buffer_[offset]);
  
  return ret;

}

FmtcpSubflowSink::FmtcpSubflowSink() : TcpSink(new Acker) {
  next_ = 0;
}

FmtcpSubflowSink::~FmtcpSubflowSink() {
    fclose(tracefile_);
    fclose(testinfo_);
}

void FmtcpSubflowSink::totrace(string msg) {
  fprintf(tracefile_, "At %lf, %s\n", Scheduler::instance().clock(), msg.c_str());
}

void FmtcpSubflowSink::totest(string msg) {
  fprintf(testinfo_, "At time %lf, %s\n", Scheduler::instance().clock(), msg.c_str());
}

void FmtcpSubflowSink::fmtcpsink_setdata(int id, FmtcpSink *core) {
  subflow_id_ = id;
  core_ = core;
  char tmp[100]; memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "FmtcpSubflowSink%d.tr", id);
  tracefile_ = fopen(tmp, "w");
  assert(tracefile_);
  memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "FmtcpSubflowSink%dTest.tr", id);  
  testinfo_ = fopen(tmp, "w");
  assert(testinfo_);
}

void FmtcpSubflowSink::recv(Packet *pkt, Handler* h) {

  hdr_tcp *th = hdr_tcp::access(pkt);
  //check if packet is from previous incarnation
  if (th->ts() < lastreset_) {
    Packet::free(pkt);
    return;
  }
  
  //try to update this packet to FmtcpSink
  if (core_ == NULL) {
    cerr << "Error: FmtcpSubflowSink: core not set" << endl;
    abort();
  }
  
  char tmp[100]; memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "subflow %d : recv fmtcp_seqno = %d, seqno = %d", subflow_id_, th->fmtcp_seqno(), th->seqno());
  core_->totest(tmp);
  totest(tmp);
  
  if (th->seqno() >= next_) buffer_.insert(th->seqno());
  while (1) {
    set<int>::iterator iter = buffer_.find(next_);
    if (iter == buffer_.end()) break;
    next_ ++;
    buffer_.erase(iter);
  }
  
  pair<int,int> t = core_->update(th->fmtcp_seqno());
  
  ack(pkt, t.first, t.second);
  
  Packet::free(pkt);

}

void FmtcpSubflowSink::ack(Packet *opkt, int fmtcp_seqno_ack, int symbolnum) {

  Packet *npkt = allocpkt();
  //opkt: the old packet
  //nptk: the ack packet constructed
  double now = Scheduler::instance().clock();
  
  hdr_tcp *otcp = hdr_tcp::access(opkt);
  hdr_tcp *ntcp = hdr_tcp::access(npkt);
  
  ntcp->seqno() = next_ - 1;
  ntcp->fmtcp_seqno() = core_->get_last_ack();
  ntcp->fmtcp_seqno_ack() = fmtcp_seqno_ack;
  ntcp->fmtcp_symbolnum_ack() = symbolnum;
  
  ntcp->ts() = now;
  ntcp->ts_echo() = otcp->ts();
  
  send(npkt, 0);

}


