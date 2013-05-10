#include "tcp.h"
#include "agent.h"
#include "packet.h"
#include "classifier.h"
#include "ip.h"
#include "flags.h"
#include "random.h"
#include "hdr_qs.h"

#include "fmtcpconst.h"
#include "fmtcp.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cassert>
#include <cmath>
using namespace std;

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))
#define sqr(x) ((x)*(x))


class FmtcpAgent;
class FmtcpSubflow;

static class FmtcpAgentClass : public TclClass {
public:
	FmtcpAgentClass() : TclClass("Agent/FMTCP") {}
	TclObject* create(int, const char* const*) {
	  return (new FmtcpAgent());
	}
} class_fmtcp_agent;

static class FmtcpSubflowClass : public TclClass {
public:
	FmtcpSubflowClass() : TclClass("Agent/TCP/FMTCPSubflow") {}
	TclObject* create(int, const char* const*) {
	  return (new FmtcpSubflow());
	}
} class_fmtcp_subflow;


FmtcpAgent::FmtcpAgent() : Agent(PT_TCP), tracefile_(NULL), fortelnet_(NULL), testinfo_(NULL), curseq_(-1), t_seqno_(0), last_ack_(-1), t_data_seqno_(0), last_data_ack_(-1), unacked_pkts_(0), unack_symbols_(0) {
  tracefile_ = fopen("FmtcpAgent.tr", "w");
  assert(tracefile_);
  fortelnet_ = fopen("FmtcpTelnet.tr", "w");
  assert(fortelnet_);
  testinfo_ = fopen("FmtcpAgentTest.tr", "w");
  assert(testinfo_);
  lostinfo_ = fopen("FmtcpLost.tr", "w");
  assert(lostinfo_);

  pending_block_ = 0;
  memset(scheduled_, 0, sizeof(scheduled_));
  memset(symbols_sent_, 0, sizeof(symbols_sent_));
  memset(symbols_out_, 0, sizeof(symbols_out_));
  memset(symbols_actualsent_, 0, sizeof(symbols_actualsent_));
  memset(symbols_acked_, 0, sizeof(symbols_acked_));
  memset(symbols_lost_, 0, sizeof(symbols_lost_));
  memset(send_ok_time_, 0, sizeof(send_ok_time_));
  memset(send_start_time_, 0, sizeof(send_start_time_));

  loss_rate_ = 0;
  append_packets_ = compute_appendpackets(0);

  arvtop_ = sdtop_ = 0;

}

FmtcpAgent::~FmtcpAgent() {
  fclose(tracefile_);
  fclose(fortelnet_);
  fclose(testinfo_);
  fclose(lostinfo_);
  for(uint i = 0; i < subflow_.size(); i++)
    delete subflow_[i];
}

void FmtcpAgent::delay_bind_init_all(){
	delay_bind_init_one("window_");
	delay_bind_init_one("block_size_");
	delay_bind_init_one("append_pkts_");
	Agent::delay_bind_init_all();
	//reset();
}

int FmtcpAgent::delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer){
	if (delay_bind(varName, localName, "window_", &wnd_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "block_size_", &block_packets, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "append_pkts_", &extra_packets, tracer)) return TCL_OK;
	return Agent::delay_bind_dispatch(varName, localName, tracer);
}

int FmtcpAgent::compute_appendpackets(double loss_rate) {
	//return 1;
	cerr<<"append_pkts_: "<<extra_packets<<endl;
	return extra_packets;
}

void FmtcpAgent::totrace(string msg) {
  fprintf(tracefile_, "At time %lf, %s\n", Scheduler::instance().clock(), msg.c_str());
}

void FmtcpAgent::totelnet(string msg) {
	fprintf(fortelnet_, "At time %lf, %s\n", Scheduler::instance().clock(), msg.c_str());
}

void FmtcpAgent::totest(string msg) {
  fprintf(testinfo_, "At time %lf, %s\n", Scheduler::instance().clock(), msg.c_str());
}

void FmtcpAgent::tolost(string msg) {
  fprintf(lostinfo_, "%s\n", msg.c_str());
}

Subflow* FmtcpAgent::find_subflow(int addr, int port) {
  for(uint i = 0; i < subflow_.size(); i++)
    if (subflow_[i] != NULL && subflow_[i]->addr_ == addr && subflow_[i]->port_ == port) return subflow_[i];
  return NULL;
}

Subflow* FmtcpAgent::find_subflow(int addr) {
  for(uint i = 0; i < subflow_.size(); i++)
    if (subflow_[i] != NULL && subflow_[i]->addr_ == addr) return subflow_[i];
  return NULL;
}

//Add a new destination
void FmtcpAgent::add_destination(int addr, int port) {
  Dstinfor t;
  t.addr_ = addr;
  t.port_ = port;
  dstinfor_.push_back(t);
}

//Check whether we can route packet to (addr, port) using subflow sid
bool FmtcpAgent::check_routable(int sid, int addr, int port) {
  Packet *p = allocpkt();
  hdr_ip *iph = hdr_ip::access(p);
  iph->daddr() = addr;
  iph->dport() = port;
  bool result = (static_cast<Classifier*> (subflow_[sid]->target_) ->classify(p) > 0)? true : false;
  Packet::free(p);
  return result;
}

//Only forward the packet to subflows but do nothing at mptcpagent
//The subflow should return this packet back by calling "update", that's where the update takes place
void FmtcpAgent::recv(Packet *pkt, Handler* h) {
  hdr_ip *iph = hdr_ip::access(pkt);
  Subflow *sf = find_subflow(iph->daddr());
  if (sf == NULL) {
    cerr << "Fatal: FmtcpAgent::recv cannot find destination of a packet" << endl;
    abort();
  }

  sf->tcp_->recv(pkt, h);
}

int FmtcpAgent::command(int argc, const char* const* argv) {

  bool used[max_subflow];

  memset(used,0,sizeof(used));

  if (strcmp(argv[1], "reset") == 0) {

    curseq_ = -1;
    last_ack_ = -1;
    t_seqno_ = 0;
    unack_symbols_ = 0;
    current_databytes_ = 0;

	last_data_ack_ = -1;
    t_data_seqno_ = 0;
	unacked_pkts_ = 0;

    loss_rate_ = 0;
    append_packets_ = compute_appendpackets(0);

    arvtop_ = sdtop_ = 0;

	pending_block_ = 0;
	memset(scheduled_, 0, sizeof(scheduled_));
    memset(symbols_sent_, 0, sizeof(symbols_sent_));
	memset(symbols_out_, 0, sizeof(symbols_out_));
	memset(symbols_actualsent_, 0, sizeof(symbols_actualsent_));
    memset(symbols_acked_, 0, sizeof(symbols_acked_));
    memset(symbols_lost_, 0, sizeof(symbols_lost_));
    memset(send_ok_time_, 0, sizeof(send_ok_time_));
    memset(send_start_time_, 0, sizeof(send_start_time_));

	memset(lost_count_, 0, sizeof(lost_count_));

    for(uint i = 0; i < subflow_.size(); i++)
      for(uint j = 0; j < dstinfor_.size(); j++) {

        if (used[j]) continue; //this destination has already been assigned

        if (check_routable(i, dstinfor_[j].addr_, dstinfor_[j].port_)) {
          subflow_[i]->daddr_ = dstinfor_[j].addr_;
          subflow_[i]->dport_ = dstinfor_[j].port_;
          subflow_[i]->tcp_->daddr() = dstinfor_[j].addr_;
          subflow_[i]->tcp_->dport() = dstinfor_[j].port_;
          used[j] = true;
          break;
        }

      }

    return TCL_OK;
  }

  else if (strcmp(argv[1], "attach-tcp") == 0) {
    //attach a mptcp subflow (sending agent)
    FmtcpSubflow *subflow_agent = (FmtcpSubflow*) TclObject::lookup(argv[2]);
    subflow_agent->fmtcp_setdata(subflow_.size(), this);
    Subflow *sf = new Subflow;
    sf->tcp_ = subflow_agent;
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
    Subflow *sf = find_subflow(atoi(argv[2]));
    if (sf == NULL) {
      cerr << "Fatal: MptcpAgent::command, cannot find subflow in add-multihome-interface" << endl;
      abort();
    }
    sf->tcp_->port() = atoi(argv[3]);
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

//Try to send nbytes message (if nbytes = -1, send message infinitely) by raising curseq_
void FmtcpAgent::sendmsg(int nbytes, const char* flags) {

	//char tmp[100]; memset(tmp,0,sizeof(tmp));
	//sprintf(tmp, "sendmsg nbytes = %d", nbytes);
	//totest(tmp);

  if (nbytes == 0) nbytes = MSS;  // ???

  //cout << "time = " << Scheduler::instance().clock() << "; nbytes = " << nbytes << endl;

  if (nbytes == -1 && curseq_ <= TCP_MAXSEQ)
    curseq_ = TCP_MAXSEQ;
  else {
    current_databytes_ += nbytes;
    if (current_databytes_ >= block_packets * MSS) {

      int last_curseq = curseq_;
      curseq_ += current_databytes_ / (block_packets * MSS);
      double current_time = (double)Scheduler::instance().clock();
      for(int i = last_curseq+1; i <= curseq_; i++) {
        trace_arrive_time_[arvtop_++] = current_time;
        char tmp[100]; memset(tmp,0,sizeof(tmp));
        sprintf(tmp, "arriving block %d", i);
        totelnet(tmp);
      }
      current_databytes_ %= block_packets * MSS;

      //cout << "curseq_ = " << curseq_ << "; t_seqno_ = " << t_seqno_ << endl;

    }
  }
  send_much(-1, 0);
}

//Advance curseq_ by delta  (but do not send them immediately)
void FmtcpAgent::advanceby(int delta) {

  if (delta > 0) {
    int nbytes = delta * MSS; // advance window @wang
    current_databytes_ += nbytes;
    if (current_databytes_ >= block_packets * MSS) {
      curseq_ += current_databytes_ / (block_packets * MSS);
      current_databytes_ %= block_packets * MSS;
    }
	send_much(-1, 0);
  }
}

// @wang
double FmtcpAgent::get_wnd(){
	return wnd_;
}

int FmtcpAgent::remaining_wnd(int sender_wnd) {
	int receiver_wnd = (int)wnd_ - unacked_pkts_;
	return  (sender_wnd < receiver_wnd ? sender_wnd : receiver_wnd);
}


void FmtcpAgent::update_ack(int lastack_seqno, int fmtcp_seqno_ack, int symbolnum, int subflow_id, bool force) {

  //char tmp[100]; memset(tmp,0,sizeof(tmp));
  //sprintf(tmp, "lastack_seqno = %d, fmtcp_seqno_ack = %d, symbolnum = %d", lastack_seqno, fmtcp_seqno_ack, symbolnum);
  //totest(tmp);

  int offset = fmtcp_seqno_ack % send_buffer_block;
  symbols_acked_[offset] = symbolnum;

  // for unrealized lost
  if(fmtcp_seqno_ack > (last_ack_+1) && lastack_seqno == last_ack_){
	  int cur_block_offset = (last_ack_+1) % send_buffer_block;
	  symbols_actualsent_[cur_block_offset] = symbols_acked_[cur_block_offset];
  }

  while (last_ack_ < lastack_seqno) {
    last_ack_ ++;
    int last_offset = last_ack_ % send_buffer_block;
	unacked_pkts_ -= symbols_actualsent_[last_offset];
	if(unacked_pkts_ < 0)
		unacked_pkts_ = 0;

	int temp = symbols_out_[last_offset]-(block_packets + append_packets_);

	char tmp[100];
	memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "%d", temp);
	tolost(tmp);

    symbols_acked_[last_offset] = 0;
    symbols_lost_[last_offset] = 0;
    send_start_time_[last_offset] = 0;
	symbols_actualsent_[last_offset] = 0;
	symbols_out_[last_offset] = 0;

	//char tmp[100];
	memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "ack block = %d unacked_pkts_ = %d last_offset = %d symbols_acked = %d ", last_ack_, unacked_pkts_, last_offset, symbols_acked_[last_offset]);
	totest(tmp);

	memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "scheduled symbols:");
	totest(tmp);
	for(int i=0; i<wnd_/block_packets; i++){
		memset(tmp,0,sizeof(tmp));
		sprintf(tmp, "%d : %d", i, scheduled_[i]);
		totest(tmp);
	}
  }

  /*
  if (fmtcp_seqno_ack <= last_ack_ || fmtcp_seqno_ack > last_ack_ + send_buffer_block) {
    if (force) send_much(subflow_id, 0);  //
	return;
  }*/

  if (force) send_much(-1, 0);  // attention

}

void FmtcpAgent::update_timeout(int fmtcp_seqno_ack, int subflow_id, bool force) {

  /*char tmp[100]; memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "TD/TO, fmtcp_seqno_ack = %d, last_ack = %d, force = %d", fmtcp_seqno_ack, last_ack_, (int)force);*/
  //totrace(tmp);

  if (fmtcp_seqno_ack <= last_ack_ || fmtcp_seqno_ack > last_ack_ + send_buffer_block) {
    if (force) send_much(subflow_id, 1);
	//cerr<<"block     disorder!!"<<endl;
	//char tmp[100];
	//memset(tmp,0,sizeof(tmp));
	//sprintf(tmp, "Disorder: fmtcp_seqno %d last_ack_ %d", fmtcp_seqno_ack, last_ack_);
	//totest(tmp);
    return;
  }
  int offset = fmtcp_seqno_ack % send_buffer_block;
  //symbols_sent_[offset] --;
  symbols_actualsent_[offset]--;
  unacked_pkts_ --;
  symbols_lost_[offset] ++;

  if (force) send_much(subflow_id, 1);

}

// smallest root heap
int FmtcpAgent::get_first_subflow(){
	int length = rank_.size();
	for (int i = length / 2 - 1; i >= 0; --i){
		double nTemp;
		int nChild, idTemp;
		int j = i;
		for (nTemp = rank_[j].EAT; 2 * j + 1 < length; j = nChild) {
			idTemp = rank_[j].ID;
			// get child
			nChild = 2 * j + 1;
			// get the smaller child
			if (nChild != length - 1 && rank_[nChild + 1].EAT < rank_[nChild].EAT)
				++nChild;

			// let father node to be smallest
			if (nTemp >= rank_[nChild].EAT){
				rank_[j].EAT = rank_[nChild].EAT;
				rank_[j].ID = rank_[nChild].ID;
			} else  {
				break;
			}
			rank_[nChild].EAT = nTemp;
			rank_[nChild].ID = idTemp;
		}
	}
	return rank_[0].ID;
}

int FmtcpAgent::get_fmtcp_seqno(int subflow_id) {

  //if force == true, this packet will be actually transmitted; otherwise it will not
  //if FmtcpAgent cannot allocate a packet, it will return -1

  int first_id = get_first_subflow(); // subflow_id;//

  if(first_id != subflow_id) {
	  char tmp[100];
	  memset(tmp,0,sizeof(tmp));
	  sprintf(tmp, "check: subflow_id_ = %d, first_id_ = %d, first_win = %d EAT %f %f", subflow_id, first_id, rank_[0].WIN, rank_[0].EAT, rank_[1].EAT);
	  //totest(tmp);
	  int cur_fmtcp_seqno = last_ack_ + 1;
	  int offset = cur_fmtcp_seqno % send_buffer_block;
	  while (cur_fmtcp_seqno <= last_ack_ + (int)(wnd_/block_packets) && cur_fmtcp_seqno <= curseq_) {
		  if (symbols_sent_[offset] < block_packets + append_packets_) {
		    // update sent_
			symbols_sent_[offset]++;  // check window_remaining of first_id ??
			// update EAT
			FmtcpSubflow *sf_agent = subflow_[first_id]->tcp_;
			int count = subflow_[first_id]->virt_num_;
			count++;
			subflow_[first_id]->virt_num_++;
			if(count >= rank_[0].WIN){
				rank_[0].EAT = sf_agent->get_EAT(count-rank_[0].WIN+1);
			} else {
				rank_[0].EAT = sf_agent->get_EAT(0);
			}
			memset(tmp,0,sizeof(tmp));
			sprintf(tmp, "count %d update EAT %lf  %lf", count, rank_[0].EAT, rank_[1].EAT);
			//totest(tmp);
			//sprintf(tmp, "cur_fmtcp_seqno = %d symbols = %d", cur_fmtcp_seqno, symbols_sent_[offset]);
			//totest(tmp);
			int fmtcp_seqno = get_fmtcp_seqno(subflow_id);
			// recover EAT etc.
			symbols_sent_[offset]--;
			count--;
			subflow_[first_id]->virt_num_--;
			return fmtcp_seqno;
		 }
		 cur_fmtcp_seqno ++;
		 offset = (offset + 1) % send_buffer_block;
	  }
  } else {
	  int cur_fmtcp_seqno = last_ack_ + 1;
	  int offset = cur_fmtcp_seqno % send_buffer_block;
	  int flag = false;
	  while (cur_fmtcp_seqno <= last_ack_ + (int)(wnd_/block_packets) && cur_fmtcp_seqno <= curseq_) {
		  if(!flag && (symbols_actualsent_[offset] < block_packets + append_packets_)){
			  flag = true;
			  pending_block_ = cur_fmtcp_seqno;
			  //char tmp[100];
			  //memset(tmp,0,sizeof(tmp));
			  //sprintf(tmp, "appending block %d", pending_block_);
			  //totrace(tmp);
		  }
		  if (symbols_sent_[offset] < block_packets + append_packets_) {
			  //this packet is actually sent
			  double cur_time = (double)Scheduler::instance().clock();
			  int symbol_seqno = symbols_actualsent_[offset];
			  if (symbol_seqno == 0) {
				send_start_time_[offset] = cur_time;
				char tmp[100];
				memset(tmp,0,sizeof(tmp));
				sprintf(tmp, "sending block %d", cur_fmtcp_seqno);
				totrace(tmp);
				//totest(tmp);
			  }
			  //memset(tmp,0,sizeof(tmp));
			  //sprintf(tmp, "point2: block  %d symbols %d", cur_fmtcp_seqno, symbols_actualsent_[offset]);
			  //totest(tmp);

			  //symbols_[offset][symbol_seqno] = SymbolInfo(cur_time, subflow_id);

			  unacked_pkts_ ++;

			  if(cur_fmtcp_seqno >= pending_block_ && (cur_fmtcp_seqno - pending_block_)<send_buffer_block){
				  scheduled_[cur_fmtcp_seqno - pending_block_]++;  // bound error!!!!
			  } else {
				  cerr<<"appending block error!";
			  }

		      return cur_fmtcp_seqno;
		  }
		  cur_fmtcp_seqno ++;
		  offset = (offset + 1) % send_buffer_block;
	}
  }

  return -1;

}

void FmtcpAgent::get_eat(){
	rank_.clear();
	for(uint i = 0; i < subflow_.size(); i++) {
		subflow_[i]->virt_num_ = 0;
		FmtcpSubflow *sf_agent = subflow_[i]->tcp_;
		EAT_ID t;
		t.EAT = sf_agent->get_EAT(0);
		int subflow_remaining = sf_agent->fmtcp_get_remaining_window();
		t.WIN = remaining_wnd(subflow_remaining);
		if(t.WIN < 0){
			t.WIN = 0;
		}
		t.ID = i;
		rank_.push_back(t);
		//char tmp[100]; memset(tmp,0,sizeof(tmp));
		//sprintf(tmp, "check ID = %d, EAT = %f", i, t.EAT);
		//totest(tmp);
	}
}

//Use a simple loop to allocate data
// @wang
void FmtcpAgent::send_much(int subflow_id, int maxburst) {

  if (subflow_.empty()) {
    cerr << "Fatal: FmtcpAgent::send_much: no subflows found." << endl;
    abort();
  }

	//char tmp[100]; memset(tmp,0,sizeof(tmp));
	//sprintf(tmp, "send: subflow id = %d maxburst = %d ", subflow_id, maxburst);
	//totest(tmp);

  int npackets = 0; //packets already sent
  t_seqno_ = last_ack_ + 1; //7-16

  char tmp[100]; memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "Try to send data, t_seqno_ = %d, curseq_ = %d", t_seqno_, curseq_);
  totest(tmp);


  if(subflow_id < 0){ // actived by sendmsg

	while ((maxburst==0||npackets<maxburst) && t_seqno_ < curseq_){
		//Try to find a subflow to send the current packet
		bool success = false;
		for(uint i = 0; i < subflow_.size(); i++) {

			if ((maxburst && npackets >= maxburst) || t_seqno_ >= curseq_ ) break;

		  FmtcpSubflow *sf_agent = subflow_[i]->tcp_;
		  // window @wang
		  int subflow_remaining = sf_agent->fmtcp_get_remaining_window();
		  int window_remaining = subflow_remaining; //remaining_wnd(subflow_remaining);//

		  //char tmp[100];
		  //memset(tmp,0,sizeof(tmp));
		  //sprintf(tmp, "subflow id: %d send wnd = %d remain_wnd = %d", sf_agent->fmtcp_get_subflow_id(), subflow_remaining, window_remaining);
		  //totest(tmp);

		  if (window_remaining > 0) {
		  //for (int k = 0; k < window_remaining; k++) {
			  get_eat();

			  for(int m=0; m<send_buffer_block; m++){
				  symbols_sent_[m] = symbols_actualsent_[m];
			  }
			  int fmtcp_seqno = get_fmtcp_seqno(i);	//

			  if (fmtcp_seqno > -1) {
				  int index = fmtcp_seqno%send_buffer_block;
				  int sent_t = symbols_actualsent_[index];
				  char tmp[100];
				  memset(tmp,0,sizeof(tmp));
				  sprintf(tmp, "subflow id: %d block_seqno = %d symbol_num = %d", i, fmtcp_seqno, sent_t);
				  //totest(tmp);
				sf_agent->send_much(fmtcp_seqno, 0, 1);  //
				npackets ++;
				symbols_actualsent_[index] ++;
				symbols_out_[index] ++;
				success = true;
			  }

			  if ((maxburst && npackets >= maxburst) || t_seqno_ >= curseq_ ) break;
		  }
		}
		if (!success) break; // to be considered
	}
  } else {  // actived by ack or timeout of a subflow
		// send a packet through subflow _id
		//allocate a packet (consisting of several symbols) and try to send it (only one packet is sent one time)
		bool success = false;
		FmtcpSubflow *sf_agent = subflow_[subflow_id]->tcp_;
		if(maxburst){
			get_eat();

			for(int m=0; m<send_buffer_block; m++){
				symbols_sent_[m] = symbols_actualsent_[m];
			}
			int fmtcp_seqno = get_fmtcp_seqno(subflow_id);	//

			if(fmtcp_seqno < 0){
				fmtcp_seqno = last_ack_ + 1;
			}

			if (fmtcp_seqno > -1) {
				//symbols_actualsent_[fmtcp_seqno%send_buffer_block]++;
				int index = fmtcp_seqno%send_buffer_block;
				int sent_t = symbols_actualsent_[index];
				char tmp[100];
				memset(tmp,0,sizeof(tmp));
				sprintf(tmp, "subflow id: %d block_seqno = %d symbol_num = %d", subflow_id, fmtcp_seqno, sent_t);
				//totest(tmp);
				sf_agent->send_much(fmtcp_seqno, 0, 1);  //
				npackets ++;
				symbols_actualsent_[index]++;
				symbols_out_[index] ++;
				success = true;
			}

		} else {
			// window @wang
			int subflow_remaining = sf_agent->fmtcp_get_remaining_window();
			int window_remaining = subflow_remaining; //remaining_wnd(subflow_remaining);//

			//char tmp[100];
			//memset(tmp,0,sizeof(tmp));
			//sprintf(tmp, "subflow id: %d send wnd = %d remain_wnd = %d", sf_agent->fmtcp_get_subflow_id(), subflow_remaining, window_remaining);
			//totest(tmp);

			for (int k = 0; k < window_remaining; k++) {
				get_eat();

				for(int m=0; m<send_buffer_block; m++){
					symbols_sent_[m] = symbols_actualsent_[m];
				}
				int fmtcp_seqno = get_fmtcp_seqno(subflow_id);	//

				if (fmtcp_seqno > -1) {
					int index = fmtcp_seqno%send_buffer_block;
					int sent_t = symbols_actualsent_[index];
					char tmp[100];
					memset(tmp,0,sizeof(tmp));
					sprintf(tmp, "subflow id: %d block_seqno = %d symbol_num = %d", subflow_id, fmtcp_seqno, sent_t);
					//totest(tmp);
					sf_agent->send_much(fmtcp_seqno, 0, 1);  //
					symbols_actualsent_[index]++;
					symbols_out_[index] ++;
					npackets ++;
					success = true;
				} else {
					success = false;
				}

				if ((maxburst && npackets >= maxburst) || t_seqno_ >= curseq_ )break;
				if (!success) break;
			}
		}
	}
}

bool cmp_(const Subflow* A, const Subflow* B)
{
	return A->tcp_->cwnd_ * sqr(B->tcp_->t_srtt_) < B->tcp_->cwnd_ * sqr(B->tcp_->t_srtt_);
}

bool id_cmp_(const Subflow* A, const Subflow* B)
{
        return A->id_ < B->id_;
}

double FmtcpAgent::get_increment(int subflow_id_)
{
	sort(subflow_.begin(), subflow_.end(), cmp_);
	int p = -1;
	double s = 0, ans = 1e100, res = 0;
	FmtcpSubflow *pflow = 0;
	for (uint i = 0; i < subflow_.size(); ++i) {
		if (subflow_[i]->id_ == subflow_id_) {
			p = i;
		}
		pflow = subflow_[i]->tcp_;
		s += pflow->cwnd_ / pflow->t_srtt_;
	}
	if (p < 0) {
		cerr << "Fatal: can't find subflow whose id is " << subflow_id_ << endl;
		abort();
	}
	for (uint i = p; i < subflow_.size(); ++i) {
		pflow = subflow_[i]->tcp_;
		s += pflow->cwnd_ / pflow->t_srtt_;
		res = pflow->cwnd_ / sqr(pflow->t_srtt_) / sqr(s);
		ans = MIN(res, ans);
	}
	sort(subflow_.begin(), subflow_.end(), id_cmp_);
	return ans;
}

//Class FmtcpSubflow

FmtcpSubflow::FmtcpSubflow() : TcpAgent(), dupwnd_(0), tracefile_(NULL), testinfo_(NULL), subflow_id_(-1), core_(NULL) {

  last_ack_ = -1;
  highest_seqno_ = -1;
  t_seqno_ = 0;
  cnt_dupack_ = 0;
  target_ack_ = -1;
  for(int i = 0; i < send_buffer_size; i++)
    sent_[i] = -1;

  lost_num_ = 0;
  lossrate_ = 0;

  syn_ = 0;
  frto_enabled_ = 0;
  sfrto_enabled_ = 0;
  ecn_ = 0;
  ect_ = 0;
  eln_ = 0;
  qs_enabled_ = 0;
}

FmtcpSubflow::~FmtcpSubflow() {}

void FmtcpSubflow::totrace(string msg) {
  fprintf(tracefile_, "At time %lf, %s\n", Scheduler::instance().clock(), msg.c_str());
}

void FmtcpSubflow::totest(string msg) {
  fprintf(testinfo_, "At time %lf, %s\n", Scheduler::instance().clock(), msg.c_str());
}

void FmtcpSubflow::fmtcp_setdata(int id, FmtcpAgent *core) {
  subflow_id_ = id;
  core_ = core;
  string t;
  t += id + '0';
  //wnd_ = core->get_wnd(); //
  block_cnt = 0;
  tracefile_ = fopen(("FmtcpSubflow" + t + ".tr").c_str(), "w");
  assert(tracefile_);
  testinfo_ = fopen(("FmtcpSubflow" + t + "Test.tr").c_str(), "w");
  assert(testinfo_);
}

//time out function, do congestion control, inform the FmtcpAgent but do not retransmit.
void FmtcpSubflow::timeout(int tno) {
  if (tno == TCP_TIMER_RTX) {

    if (sent_[(last_ack_ + 1) % send_buffer_size] == -1) return;

    if (core_->get_last_ack() >= core_->get_curseq() && last_ack_ >= t_seqno_ - 1) return;

	char tmp[100];

	if(last_ack_ >= t_seqno_ - 1)  {
		block_cnt++;
		memset(tmp,0,sizeof(tmp));
		sprintf(tmp, "Subflow %d Blocking time %d", subflow_id_, block_cnt);
		totest(tmp);
		//cerr << tmp << endl;
		//core_->totest(tmp);
		return;
	}

    trace_event("TIMEOUT");

    //cerr << "Timeout!" << endl;

	memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "subflow %d : Timeout! last_ack_ = %d", subflow_id_, last_ack_);
	totest(tmp);
	core_->totest(tmp);

	send_time_.clear();
	/*if(!send_time_.empty()){
		cerr << "vector error!" << endl;
	}*/

    if (cwnd_ < 1) cwnd_ = 1;
    slowdown(CLOSE_SSTHRESH_HALF | CLOSE_CWND_ONE);

    last_cwnd_action_ = CWND_ACTION_TIMEOUT;

	dupwnd_ = 0;

    for(int i = last_ack_+2; i < t_seqno_; i++) {
        int offset = i % send_buffer_size;
		if (sent_[offset] != -1) {
			core_->update_timeout(sent_[offset], subflow_id_, false);
			sent_[offset] = -1; //@wang
		}
    }
    cnt_dupack_ = 4;
    t_seqno_ = last_ack_ + 1;
    core_->update_timeout(sent_[t_seqno_ % send_buffer_size], subflow_id_, true);

	memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "Timeout! t_seqno_ = %d", t_seqno_);
	//totest(tmp);

    reset_rtx_timer(1, 0);

  }
  else {
	//cerr << "To TCP Timeout!" << endl;
	totest("To TCP Timeout!");
	TcpAgent::timeout(tno);
  }
}

//triple ack received, treated as if the packet was lost
void FmtcpSubflow::triple_ack() {

  //cerr << "Triple ACK!" << endl;

  if (cwnd_ < 1) cwnd_ = 1;
  slowdown(CLOSE_SSTHRESH_HALF | CLOSE_CWND_HALF);

  lost_num_++;
  lossrate_ = (double)lost_num_/t_seqno_;

  char tmp[100];
  memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "subflow %d : Triple ACK!! ", subflow_id_);
  totest(tmp);
  //core_->totest(tmp);

  last_cwnd_action_ = CWND_ACTION_DUPACK;

  int backup_t_seqno = t_seqno_;
  t_seqno_ = last_ack_ + 1;
  target_ack_ = t_seqno_;

  reset_rtx_timer(1, 0);

  core_->update_timeout(sent_[t_seqno_ % send_buffer_size], subflow_id_, true);

  t_seqno_ = backup_t_seqno;

  //core_->send_much(0);

}

//only remove the timer-setting part
void FmtcpSubflow::output(int block_seqno, int seqno, int reason)
{
	//both seqno and reason are dummy parameters here

	Packet* p = allocpkt();
	hdr_tcp *tcph = hdr_tcp::access(p);
	int databytes = hdr_cmn::access(p)->size();
	tcph->ts() = Scheduler::instance().clock();

	send_time_.push_back(tcph->ts()); // store sending time

	tcph->ts_echo() = ts_peer_;
	tcph->reason() = reason;
	tcph->last_rtt() = int(int(t_rtt_)*tcp_tick_*1000);
	tcph->seqno() = seqno;

	// @wang
	tcph->fmtcp_seqno() = block_seqno;
	//tcph->fmtcp_seqno() = core_->get_fmtcp_seqno(true, subflow_id_);

	char tmp[100];
	memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "subflow %d : send, fmtcp seqno = %d, seqno = %d t_seqno_ = %d ", subflow_id_, block_seqno, seqno, t_seqno_);
	//totest(tmp);
	//core_->totest(tmp);

	//count through-put
	//cout << "Send a packet" << endl;
	//end count through-put

	sent_[tcph->seqno() % send_buffer_size] = tcph->fmtcp_seqno();

        if (seqno - last_ack_ - 1 == 0) {
          set_rtx_timer();
        }

    ++ndatapack_;
    ndatabytes_ += databytes;

	send(p, 0);

	if(seqno > highest_seqno_){
		highest_seqno_ = seqno;
	}
}


void FmtcpSubflow::send_much(int block_seqno, int reason, int maxburst)
{

	int win = window();
	int npackets = 0;

	while ( t_seqno_ - last_ack_ - 1 < win ) {

		output(block_seqno, t_seqno_, reason);
		t_seqno_ ++;
		npackets++;

		win = window();

		if (maxburst && npackets == maxburst)
			break;
	}

	/*char tmp[100]; memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "maxburst = %d, t_seqno_ = %d, last_ack_ = %d", maxburst, t_seqno_, last_ack_);*/
	//totrace(tmp);

}


int FmtcpSubflow::window()
{
	//
	// reno: inflate the window by dupwnd_
	//	dupwnd_ will be non-zero during fast recovery,
	//	at which time it contains the number of dup acks
	//
	int win = int(cwnd_) + dupwnd_;
	if (frto_ == 2) {
		// First ack after RTO has arrived.
		// Open window to allow two new segments out with F-RTO.
		win = force_wnd(2);
	}
	if (win > int(wnd_))  // for shared use of receive buffer @wang
		win = int(wnd_);
	return (win);
}


double FmtcpSubflow::windowd()
{
	//
	// reno: inflate the window by dupwnd_
	//	dupwnd_ will be non-zero during fast recovery,
	//	at which time it contains the number of dup acks
	//
	double win = cwnd_ + dupwnd_;
	if (win > wnd_)
		win = wnd_;
	return (win);
}


int FmtcpSubflow::fmtcp_get_remaining_window() {
	return window() - (t_seqno_ - last_ack_ - 1);
}

double FmtcpSubflow::get_earliest(){
	return send_time_[0];
}

double FmtcpSubflow::fmtcp_get_srtt() {
	return (int(t_srtt_) >> T_SRTT_BITS)*tcp_tick_;
}

double FmtcpSubflow::fmtcp_get_trtt() {
	int temp = (int)t_rtt_;
	double rtt_tmp = temp * tcp_tick_;
	char tmp[100]; memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "RTT = %d, RTT %f", temp, rtt_tmp);
	//totest(tmp);
	return rtt_tmp;
}

double FmtcpSubflow::get_EAT(int count){
	double now = Scheduler::instance().clock();
	int temp = (int)t_rtt_;
	double rtt_tmp = temp * tcp_tick_;

	if(rtt_tmp == 0.0){
		rtt_tmp = now;  // for starting
	}
	EDT_ = (rtt_tmp/2.0)*(1.0+lossrate_)/(1.0-lossrate_);
	RT_ = (1-lossrate_)*rtt_tmp + lossrate_*fmtcp_get_RTO();

	//char tmp[100]; memset(tmp,0,sizeof(tmp));
	//sprintf(tmp, "RTT = %lf, EDT %lf, RT %lf", rtt_tmp, EDT_, RT_);
	//totest(tmp);

	/*if(EDT_ == 0.0){ // no ack received
	}*/

	if(count || fmtcp_get_remaining_window() <= 0){
		double past = 0.0;
		if(count > 0) count--;
		if(count < send_time_.size()){
			past = now - send_time_[count];
		} else {
			int tt = count - send_time_.size();
			if(send_time_.size()>0){
				tt = count/send_time_.size();
			}
			if(tt<0) tt = 1;
			past = - rtt_tmp*tt;
		}
		double wait = 0;
		if(RT_ > past){
			wait = RT_ - past;
		}
		EAT_ = EDT_ + wait ;
	} else {
		EAT_ = EDT_;
	}

	char tmp[100]; memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "count = %d, EDT %lf, EAT %lf", count, EDT_, EAT_);
	//core_->totest(tmp);

	return EAT_;
}

void FmtcpSubflow::opencwnd()
{
        double increment;
	if (cwnd_ < ssthresh_) {
		/* slow-start (exponential) */
		cwnd_ += 1;
	} else {
	        increment = core_->get_increment(subflow_id_);
	        if ((last_cwnd_action_ == 0 ||
			  last_cwnd_action_ == CWND_ACTION_TIMEOUT)
			  && max_ssthresh_ > 0) {
				increment = limited_slow_start(cwnd_,
				  max_ssthresh_, increment);
			}
                cwnd_ += increment;
	}
}

//Main reception path
//1. update rtt, 2. update sent_, 3. if some packets are acked, open the cong window, 4. if the oldest packet are acked,
//reset the rtx timer (if there are still outstanding packets)
void FmtcpSubflow::recv(Packet *pkt, Handler*) {
	int valid_ack = 0;

  hdr_tcp *tcph = hdr_tcp::access(pkt);

  //check if this is from a previous incarnation
  if (tcph->ts() < lastreset_) {
    Packet::free(pkt);
    return;
  }

  /*char tmp[100]; memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "last_ack = %d, recv seqno %d, fmtcp_ack %d, symbolnum %d, t_seqno = %d, target_ack_ = %d", last_ack_, tcph->seqno(), tcph->fmtcp_seqno_ack(), tcph->fmtcp_symbolnum_ack(), t_seqno_, target_ack_);
  totest(tmp);*/

  //update rtt
  ++nackpack_;
  ts_peer_ = tcph->ts();
  double now = Scheduler::instance().clock();
  rtt_update(now - tcph->ts_echo());
  //edt_update(tcph->ts() - tcph->ts_echo());
  while((!send_time_.empty()) && send_time_[0] <= tcph->ts_echo()){
	send_time_.pop_front();
  }

  if (target_ack_ != -1) {

    //cerr << "target_ack_ = " << target_ack_ << "; last_ack = " << last_ack_ <<endl;

    //in fast recovery, if the packet does not ack correctly, do not send extra data
    if (tcph->seqno() < target_ack_) {
      core_->update_ack(tcph->fmtcp_seqno(), tcph->fmtcp_seqno_ack(), tcph->fmtcp_symbolnum_ack(), subflow_id_, false);
      return;
    }
    else {
      //cerr << "Yes, fast recovery ends." << endl;
      target_ack_ = -1;
    }
  }

  char tmp[100];
  memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "subflow %d : recv, block seqno = %d last_ack = %d, recv seqno = %d t_seqno_ = %d win = %d", subflow_id_, tcph->fmtcp_seqno_ack(), last_ack_, tcph->seqno(), (int)t_seqno_, (int)window());
  //totest(tmp);
  //core_->totest(tmp);

  if (tcph->seqno() > last_ack_ || tcph->seqno() == highest_seqno_) {
	valid_ack = 1;
	if (last_cwnd_action_ == CWND_ACTION_DUPACK)
		last_cwnd_action_ = CWND_ACTION_EXITED;
    //it is a correct ack, try to open congestion window and reset rtx timer
    cnt_dupack_ = 0;
	dupwnd_ = 0;
    reset_rtx_timer(1, 0);
    //totrace("RTX timer reset!");
    opencwnd();
  }
  else if (tcph->seqno() == last_ack_){ //duplicate ack
	valid_ack = 1;
    cnt_dupack_ ++;
    if (cnt_dupack_ == numdupacks_) {

      /*loss_num_++;
      lossrate_ = (double)loss_num_/t_seqno_;
      memset(tmp,0,sizeof(tmp));
      sprintf(tmp, "lossrate_ = %d", lossrate_);
      totest(tmp);*/

      core_->update_ack(tcph->fmtcp_seqno(), tcph->fmtcp_seqno_ack(), tcph->fmtcp_symbolnum_ack(), subflow_id_, false);
      triple_ack();

	  if (!exitFastRetrans_)
		  dupwnd_ = numdupacks_;

      Packet::free(pkt);
      return;
    } else if (cnt_dupack_ > numdupacks_&& (!exitFastRetrans_ || last_cwnd_action_ == CWND_ACTION_DUPACK )) {
	  ++dupwnd_;	// fast recovery
	} else if (cnt_dupack_ < numdupacks_){

	}
  } else {
	//valid_ack = 0;
	//Packet::free(pkt);
	//return;  // not reasonable or with error
  }

  while (last_ack_ < tcph->seqno()) {
    last_ack_ ++;
	if (last_ack_ >= 0){
		sent_[last_ack_ % send_buffer_size] = -1;
	}
  }

  if(last_ack_ > t_seqno_ - 1){
	t_seqno_ = last_ack_ + 1;
  }

  core_->update_ack(tcph->fmtcp_seqno(), tcph->fmtcp_seqno_ack(), tcph->fmtcp_symbolnum_ack(), subflow_id_, true);

  memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "subflow %d : ack, last_ack_ = %d win = %d", subflow_id_,last_ack_, (int)window());
  //totest(tmp);

  Packet::free(pkt);

    /*
	 * Try to send more data.
	 */
  /*
  if (valid_ack || aggressive_maxburst_){
	  if (dupacks_ == 0 || dupacks_ > numdupacks_ - 1){
		core->send_much(0);
	  }
  }*/

}
