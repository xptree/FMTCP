#include "ip.h"
#include "flags.h"
#include "tcp.h"
#include "mptcp.h"
#include "classifier.h"

#include <iostream>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <cstring>
#include <string>
using namespace std;

class MptcpAgent;
class MptcpSubflow;

typedef unsigned int uint;
#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define sqr(x) ((x)*(x))

static class MptcpAgentClass : public TclClass {
public:
	MptcpAgentClass() : TclClass("Agent/MPTCP") {}
	TclObject* create(int, const char* const*) {
	  return (new MptcpAgent());
	}
} class_mptcp_agent;

void MptcpAgent::delay_bind_init_all(){
	delay_bind_init_one("window_");
	delay_bind_init_one("block_size_");
	Agent::delay_bind_init_all();
	//reset();
}

int MptcpAgent::delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer){
	if (delay_bind(varName, localName, "window_", &wnd_, tracer)) return TCL_OK;
	if (delay_bind(varName, localName, "block_size_", &block_packets, tracer)) return TCL_OK;
	return Agent::delay_bind_dispatch(varName, localName, tracer);
}

static class MptcpSubflowClass : public TclClass {
public:
	MptcpSubflowClass() : TclClass("Agent/TCP/MPTCPSubflow") {}
	TclObject* create(int, const char* const*) {
	  return (new MptcpSubflow());
	}
} class_mptcp_subflow;

void MptcpAgent::totrace(string msg) {
  if (tracefile_ == NULL) tracefile_ = fopen("MptcpAgent.tr", "w");
  fprintf(tracefile_, "At time %lf, %s\n", (double)Scheduler::instance().clock(), msg.c_str());
}

void MptcpAgent::totelnet(string msg) {
	if (fortelnet_ == NULL) fortelnet_ = fopen("MptcpTelnet.tr", "w");
	fprintf(fortelnet_, "At time %lf, %s\n", (double)Scheduler::instance().clock(), msg.c_str());
}

void MptcpAgent::totest(string msg) {
	if (testinfo_ == NULL) testinfo_ = fopen("MptcpAgentTest.tr", "w");
	fprintf(testinfo_, "At time %lf, %s\n", (double)Scheduler::instance().clock(), msg.c_str());
}

void MptcpSubflow::totrace(string msg) {
  string tmp;
  tmp += subflow_id_ + '0';
  if (tracefile_ == NULL) tracefile_ = fopen(("MptcpSubflow" + tmp + ".tr").c_str(), "w");
  fprintf(tracefile_, "At time %lf, %s\n", (double)Scheduler::instance().clock(), msg.c_str());
}

MptcpAgent::MptcpAgent() : Agent(PT_TCP), curseq_(0), t_seqno_(0), last_ack_(0), remaining_bytes_(0) {
  memset(buffer_,0,sizeof(buffer_));
  tracefile_ = NULL;
  testinfo_ = NULL;
}

MptcpAgent::~MptcpAgent() {
  fclose(tracefile_);
	fclose(fortelnet_);
	fclose(testinfo_);
  for(uint i = 0; i < subflow_.size(); i++)
    delete subflow_[i];
}

Subflow* MptcpAgent::find_subflow(int addr, int port) {
  for(uint i = 0; i < subflow_.size(); i++)
    if (subflow_[i] != NULL && subflow_[i]->addr_ == addr && subflow_[i]->port_ == port) return subflow_[i];
  return NULL;
}

Subflow* MptcpAgent::find_subflow(int addr) {
  for(uint i = 0; i < subflow_.size(); i++)
    if (subflow_[i] != NULL && subflow_[i]->addr_ == addr) return subflow_[i];
  return NULL;
}

//Add a new destination
void MptcpAgent::add_destination(int addr, int port) {
  Dstinfor t;
  t.addr_ = addr;
  t.port_ = port;
  dstinfor_.push_back(t);
}

//Check whether we can route packet to (addr, port) using subflow sid
bool MptcpAgent::check_routable(int sid, int addr, int port) {
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
void MptcpAgent::recv(Packet *pkt, Handler* h) {
  hdr_ip *iph = hdr_ip::access(pkt);
  Subflow *sf = find_subflow(iph->daddr());
  if (sf == NULL) {
    cerr << "Fatal: MptcpAgent::recv cannot find destination of a packet" << endl;
    abort();
  }

  sf->tcp_->recv(pkt, h);
}

int MptcpAgent::command(int argc, const char* const* argv) {

  if (strcmp(argv[1], "reset") == 0) {

    //reset everything
    t_seqno_ = 0;
    curseq_ = 0;
    last_ack_ = -1;
    memset(buffer_,0,sizeof(buffer_));
    remaining_bytes_ = 0;

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
    MptcpSubflow *subflow_agent = (MptcpSubflow*) TclObject::lookup(argv[2]);
    subflow_agent->mptcp_setdata(subflow_.size(), this);
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
void MptcpAgent::sendmsg(int nbytes, const char* flags) {

  if (nbytes == 0) nbytes = MSS;

  if (nbytes == -1) {
    curseq_ = TCP_MAXSEQ;
    send_much(0);
  }
  else {

    //cerr << "nbytes = " << nbytes << "remaining bytes = " << remaining_bytes_ << endl;

    remaining_bytes_ += nbytes;
    if (remaining_bytes_ >= MSS * block_packets) {
      int last_curseq = curseq_;
      curseq_ += remaining_bytes_ / (MSS * block_packets) * block_packets;
      if (last_curseq / block_packets < curseq_ / block_packets) {
        for(int i = last_curseq / block_packets + 1; i <= curseq_ / block_packets; i++) {
          char tmp[100]; memset(tmp,0,sizeof(tmp));
          sprintf(tmp, "arriving block %d", i-1);
          totelnet(tmp);
        }
      }
      remaining_bytes_ %= MSS * block_packets;
      send_much(0);
    }
  }

  //totest("Here, in sendmsg");

}

//Advance curseq_ by delta bytes (but do not send them immediately)
void MptcpAgent::advanceby(int delta) {

  cerr << "size_ = " << size_ << endl;

  if (delta > 0) {
    curseq_ += (delta/size_ + (delta%size_ ? 1 : 0));
  }
}

void MptcpAgent::update_ack(int mptcp_seqno, int subflow_id) {

  char tmp[100]; memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "upate mptcp seqno: %d", mptcp_seqno);
  //totest(tmp);

  if (mptcp_seqno <= last_ack_ || mptcp_seqno - last_ack_ > send_buffer_size) return;

  buffer_[mptcp_seqno % send_buffer_size] = true;

  /*int last_offset = (last_ack_+1) % send_buffer_size;
  while (buffer_[last_offset]) {
    char tmp[100]; memset(tmp,0,sizeof(tmp));
    sprintf(tmp, "recvd packet mptcpseqno = %d", last_ack_ + 1);
    //totest(tmp);
    buffer_[last_offset] = false;
    last_ack_ ++;
    last_offset = (last_offset + 1) % send_buffer_size;
  }*/

  last_ack_ = mptcp_seqno;

  send_much(0);

}

int MptcpAgent::get_mptcp_seqno() {

  int winsize = (wnd_ < send_buffer_size ? wnd_ : send_buffer_size);
  if (t_seqno_ - last_ack_ > winsize) {
    //cerr << "Fatal: sending out of congestion window." << endl;
    //abort();
    //return t_seqno_;
	return -1;
  }

  char tmp[100];
  if (t_seqno_ % block_packets == 0) {
	memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "sending block %d", t_seqno_ / block_packets);
	totrace(tmp);
  }

  return t_seqno_ ++;

}

bool cmp(const Subflow* A, const Subflow* B)
{
	return A->tcp_->cwnd_ * sqr(B->tcp_->t_srtt_) < B->tcp_->cwnd_ * sqr(B->tcp_->t_srtt_);
}

double MptcpAgent::get_increment(int subflow_id_)
{
	sort(subflow_.begin(), subflow_.end(), cmp);
	int p = -1;
	double s = 0, ans = 1e100, res = 0;
	MptcpSubflow *pflow = 0;
	for (int i = 0; i < subflow_.size(); ++i) {
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
	for (int i = p; i < subflow_.size(); ++i) {
		pflow = subflow_[i]->tcp_;
		s += pflow->cwnd_ / pflow->t_srtt_;
		res = pflow->cwnd_ / sqr(pflow->t_srtt_) / sqr(s);
		ans = MIN(res, ans);
	}
	return ans;
}

//Send as much bytes as maxburst (if maxburst = 0, do not limit the bytes)
//Use loop and ask to find subflows whose sending window haven't been full yet
void MptcpAgent::send_much(int maxburst) {

  char tmp[100]; memset(tmp,0,sizeof(tmp));
  sprintf(tmp, "in send_much, t_seqno_ = %d, last_ack_ = %d", t_seqno_, last_ack_);
  //totest(tmp);

  if (subflow_.empty()) {
    cerr << "Fatal: MptcpAgent::send_much: no subflows found." << endl;
    abort();
  }

  int sent = 0; //packets already sent

  int winsize = (wnd_ < send_buffer_size ? wnd_ : send_buffer_size);

  while ((maxburst==0||sent<maxburst) && t_seqno_ < curseq_ && t_seqno_ <= last_ack_ + winsize) {
    //Try to find a subflow to send the current packet
    bool success = false;
    for(uint i = 0; i < subflow_.size(); i++) {

      	MptcpSubflow *sf_agent = subflow_[i]->tcp_;
      	int window_remaining = sf_agent->mptcp_get_window() - (sf_agent->mptcp_get_tseqno() - sf_agent->mptcp_get_last_ack() - 1);

  //      cerr << "subflow id = " << subflow_[i]->id_ << "; window remaining = " << window_remaining << endl;


      	char tmp[100]; memset(tmp,0,sizeof(tmp));
      	sprintf(tmp, "subflow %d, window remaining: %d", i, window_remaining);
   //   totest(tmp);

      	if (window_remaining > 0) {
        	sf_agent->send_much(0, 0, 1);
                sent ++;
        	success = true;
                if (t_seqno_ - last_ack_ > winsize) break;
      	}
    }

    if (!success) break;

  }
}

MptcpSubflow::MptcpSubflow() : TcpAgent(), subflow_id_(-1), core_(NULL), highest_seqno_(-1) {
  syn_ = 0;
  frto_enabled_ = 0;
  sfrto_enabled_ = 0;
  ecn_ = 0;
  ect_ = 0;
  eln_ = 0;
  qs_enabled_ = 0;

  maxburst_ = 1;

  memset(sent_,0,sizeof(sent_));
  cnt_dupack_ = 0;
  last_ack_ = -1;

  timeout_state = false;

}

MptcpSubflow::~MptcpSubflow() {}

void MptcpSubflow::timeout(int tno)
{

  if (tno == TCP_TIMER_RTX) {

    if (sent_[(last_ack_ + 1) % send_buffer_size] == -1) return;

    //printf("core_lastack = %d, core_curseq = %d, last_ack = %d, t_seqno_ = %d\n", (int)(core_->get_last_ack()), (int)(core_->get_curseq()), (int)(last_ack_), (int)(t_seqno_));

    if (core_->get_last_ack() >= core_->get_curseq()-1 && last_ack_ >= t_seqno_ - 1) return;

    trace_event("TIMEOUT");

    //cerr << "Timeout!" << endl;
    //totrace("Timeout!");

    if (cwnd_ < 1) cwnd_ = 1;
    slowdown(CLOSE_SSTHRESH_HALF | CLOSE_CWND_ONE);

    last_cwnd_action_ = CWND_ACTION_TIMEOUT;

    t_seqno_ = last_ack_ + 1;
    cnt_dupack_ = 0;

	timeout_state = true;

    output(t_seqno_, 0);

    reset_rtx_timer(1, 0);

  }

}

void MptcpSubflow::triple_ack() {

  //cerr << "Triple Ack!" << endl;
  //totrace("Triple Ack!");

  if (cwnd_ < 1) cwnd_ = 1;
  slowdown(CLOSE_SSTHRESH_HALF | CLOSE_CWND_HALF);

  output(last_ack_ + 1, 1);

  reset_rtx_timer(1, 0);

}

void MptcpSubflow::send_much(int force, int reason, int maxburst) {

	int win = window();
	int npackets = 0;

	while ( t_seqno_ - last_ack_ - 1 < win ) {
		if (core_->get_receive_window() <= 0) {
                        core->opportunistic_retransmission(subflow_id_);
                        continue;
		}

		output(t_seqno_, reason);
		t_seqno_ ++;
		npackets++;

		win = window();

		if (maxburst && npackets == maxburst)
			break;
	}

}

void MptcpSubflow::sendone(int reason)
{
	int win = window();
	if (t_seqno_ - last_ack_ - 1 < win) {
		output(t_seqno_, reason);
	}
}

//output a packet whose seqno equals (seqno)arg[0]
void MptcpSubflow::output(int seqno, int reason) {

	Packet* p = allocpkt();
	hdr_tcp *tcph = hdr_tcp::access(p);
	int databytes = hdr_cmn::access(p)->size();
	tcph->ts() = Scheduler::instance().clock();

	tcph->ts_echo() = ts_peer_;
	tcph->reason() = reason;
	tcph->last_rtt() = int(int(t_rtt_)*tcp_tick_*1000);
	tcph->seqno() = seqno;

        char tmp[100]; memset(tmp,0,sizeof(tmp));
        sprintf(tmp, "t_seqno_ = %d, highest_seqno = %d, sent_[t_seqno_] = %d", (int)t_seqno_, highest_seqno_, sent_[t_seqno_%send_buffer_size]);
	//totrace(tmp);

	//count through-put
	//cout << "Send a packet" << endl;
	//end count-through-put

	if (seqno <= highest_seqno_){ //@Qiu this packet has been sent before
	  tcph->mptcp_seqno() = sent_[seqno % send_buffer_size];
	  if(tcph->mptcp_seqno() < 0){
		/*if(reason!=1){
			t_seqno_ --;
		}*/
	    //cerr<<"mpseqno error!"<<endl;
	    return;
	  }
	} else {
	  timeout_state = false;
	  tcph->mptcp_seqno() = core_->get_mptcp_seqno();
	  if(tcph->mptcp_seqno() < 0){
	    t_seqno_ --;
	    cerr<<"buf out of bound!"<<endl;
	    return;
	  }
	  sent_[seqno % send_buffer_size] = tcph->mptcp_seqno();
	  if (seqno > highest_seqno_) highest_seqno_ = seqno;
	}

	memset(tmp,0,sizeof(tmp));
	sprintf(tmp, "send packet mptcpseqno = %d, subflow seqno = %d", tcph->mptcp_seqno(), tcph->seqno());
	//totrace(tmp);

        if (seqno - last_ack_ - 1 == 0) {
          set_rtx_timer();
        }

        ++ndatapack_;
        ndatabytes_ += databytes;

	send(p, 0);

}

void MptcpSubflow::recv(Packet *pkt, Handler*) {

	hdr_tcp *tcph = hdr_tcp::access(pkt);

  	//check if this is from a previous incarnation
 	if (tcph->ts() < lastreset_) {
    		Packet::free(pkt);
    		return;
  	}

  	char tmp[100]; memset(tmp,0,sizeof(tmp));
  	sprintf(tmp, "recv packet: mptcp_seqno = %d, subflow seqno = %d", tcph->mptcp_seqno(), tcph->seqno());
  	//totrace(tmp);

  	//update rtt

  	++nackpack_;
  	ts_peer_ = tcph->ts();
  	double now = Scheduler::instance().clock();
  	rtt_update(now - tcph->ts_echo());
/*
  if (tcph->seqno() > last_ack_) {
    //it is a correct ack, try to open congestion window and reset rtx timer
    cnt_dupack_ = 0;
    reset_rtx_timer(1, 0);
    //totrace("RTX timer reset!");
    opencwnd();
  }
  else { //duplicate ack
    cnt_dupack_ ++;
    if (cnt_dupack_ == 3) {
      triple_ack();
      Packet::free(pkt);
      return;
    }
  }
*/
	if (tcph->seqno() > last_ack_) {
		//new ack
		cnt_dupack_ = 0;
		reset_rtx_timer(1, 0);
		opencwnd();
	} else {
		++cnt_dupack_;
		if (cnt_dupack_ == 3) {
			triple_ack();
			Packet::free(pkt);
			return;
		}
		sendone(0);//?@Qiu
		cwnd_ += core_->get_increment(subflow_id_);
	}

  	while (last_ack_ < tcph->seqno()) {//@Qiu 累计确认
    		if (last_ack_ >= 0) {
      			core_->update_ack(sent_[last_ack_ % send_buffer_size], subflow_id_);
      			sent_[last_ack_ % send_buffer_size] = -1;
    		}
    		last_ack_ ++;
  	}

  	while(timeout_state && (t_seqno_ - last_ack_ - 1 < window())){
		send_much(0,2,1);
  	}

  	memset(tmp,0,sizeof(tmp));
  	sprintf(tmp, "t_seqno_ = %d, last_ack_ = %d, win = %d", (int)t_seqno_, (int)last_ack_, (int)window());
  	//totrace(tmp);

  	core_->update_ack(tcph->mptcp_seqno(), subflow_id_);

  	memset(tmp,0,sizeof(tmp));
  	sprintf(tmp, "mp_seqno_ = %d, last_ack_ = %d", core_->get_t_seqno(), core_->get_last_ack());
  	//totrace(tmp);

  	Packet::free(pkt);
}

void MptcpSubflow::oppo_retransmission()
{
        int seqno = -1;
        int reason = TCP_REASON_OPPO_RETRANSMISSION;

        Packet* p = allocpkt();
	hdr_tcp *tcph = hdr_tcp::access(p);
	int databytes = hdr_cmn::access(p)->size();
	tcph->ts() = Scheduler::instance().clock();

	tcph->ts_echo() = ts_peer_;
	tcph->reason() = reason;
	tcph->last_rtt() = int(int(t_rtt_)*tcp_tick_*1000);
	tcph->mptcp_seqno() = core_->get_last_ack() + 1

	for ()
}
