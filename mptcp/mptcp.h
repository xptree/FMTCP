#ifndef MPTCP_H_
#define MPTCP_H_

#include "tcp.h"
#include "agent.h"
#include "packet.h"
#include "classifier.h"

#include <map>
#include <vector>
#include <string>

const int send_buffer_size = 1024; //both for Agent and subflow
const int MSS = 1024; //Maximum segment size for MPTCP, 1024 bytes currently
//const int block_packets = 64; //the block size, used in method sendmsg

class MptcpAgent;
class MptcpSubflow;

//The information of subflow
struct Subflow {
	MptcpSubflow *tcp_;
	int id_;            //subflow id
	int addr_, port_;   //address and port number for the subflow
	int daddr_, dport_; //the destination of address and port number for the subflow
	double scwnd_;      //smoothed congestion window (for this subflow)
	NsObject *link_;
	NsObject *target_;

	Subflow() : tcp_(NULL), addr_(0), port_(0), daddr_(-1), dport_(-1), scwnd_(0), link_(NULL), target_(NULL) {}
};

//the information of the destinations
struct Dstinfor {
	int addr_, port_;
	Dstinfor() : addr_(0), port_(0) {}
};

//The send agent of mptcp
class MptcpAgent : public Agent {

public:
	MptcpAgent();
	virtual ~MptcpAgent();
	virtual void recv(Packet*, Handler*);
	int command(int argc, const char* const* argv);
	virtual void sendmsg(int nbytes, const char *flags = 0);
	virtual void advanceby(int delta);

	//Functions above are all public functions of class TcpAgent

	void update_ack(int mptcp_seqno, int subflow_id);   //update the send window and the seqno mapping

	int get_mptcp_seqno();				//return mptcp seqno to send

	int get_last_ack() {return last_ack_;}
	int get_curseq() {return curseq_; }
	int get_t_seqno() {return t_seqno_; }
	int get_receive_window()
		{ return wnd_ - (get_t_seqno() - get_last_ack()) + 1; } //@Qiu
	double get_increment(int subflow_id_); //@Qiu
	void add_record(Packet* p, int subflow_id_);//@Qiu
	int get_origin_subflow_id(int seqno);//@Qiu
	void penalize_subflow();//@Qiu

	void totrace(std::string msg);
	void totest(std::string msg);
	void totelnet(std::string msg);

protected:
	FILE* tracefile_;
	FILE* testinfo_;  // for debugging
	FILE *fortelnet_;

	std::vector<Subflow*> subflow_; //To manage all the subflows attached to this mptcp agent
	std::vector<Dstinfor> dstinfor_;   //To manage all the destinations

	// for window
	double wnd_;
	int block_packets;
	virtual void delay_bind_init_all();
	virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);

	int curseq_; //the highest (mptcp) sequence number it should send
	int t_seqno_; //the current (mptcp) sequence number
	int last_ack_; //the (mptcp) sequence number of the last acked packet

	bool buffer_[send_buffer_size];
	vector<int> record[send_buffer_size];

	int remaining_bytes_;

	Classifier *core_; //classifier of Mptcp Agent

	void send_much(int maxburst = 0); //Try to send (at most maxburst) packets (using subflows)

	Subflow* find_subflow(int daddr, int dport);
	Subflow* find_subflow(int daddr);
	bool check_routable(int sid, int addr, int port); //check whether we can send packet to (addr,port) using subflow sid
	void add_destination(int addr, int port);
};

//The subflow sending agent
class MptcpSubflow : public TcpAgent {
	friend class MptcpAgent; //@Qiu
	friend bool cmp(const Subflow*, const Subflow*); //@Qiu
	friend bool id_cmp(const Subflow*, const Subflow*); //@Qiu
public:
	MptcpSubflow();
	virtual ~MptcpSubflow();

	virtual void recv(Packet*, Handler*);
	virtual void timeout(int tno);
	virtual void send_much(int force, int reason, int maxburst = 0);
        void opencwnd();
	void triple_ack();		//triple ack, treated as time out
	void get_penalty();     //@Qiu
	bool oppo_retransmission();

	void mptcp_setdata(int id, MptcpAgent *core) {subflow_id_ = id; core_ = core; } //allow mptcp agent to set some data

	int& mptcp_get_subflow_id() {return subflow_id_; }
	int mptcp_get_window() {return window(); } //return the current window
	double mptcp_get_cwnd() {return cwnd_; } //return the congestion window
	int mptcp_get_ssthresh() {return ssthresh_; } //return the slowstart threshold
	int mptcp_get_tseqno() {return t_seqno_; } //return the current subflow sequence number
	int mptcp_get_last_ack() {return last_ack_; } //return tha last acked subflow seqno
	int mptcp_get_srtt() {return t_srtt_; } //return the smoothed round-trip time
	double mptcp_get_RTO() {return t_rtxcur_; } //return the current RTO time
	int mptcp_get_size() {return size_; }

	void totrace(std::string msg);

protected:
	FILE *tracefile_;

	int subflow_id_; //subflow id
	MptcpAgent *core_; //the mptcp agent it belongs to

	int sent_[send_buffer_size];  //store mptcp seqnos
	int cnt_dupack_;		//the number of dup acks
	int highest_seqno_;
	double last_penalty_time;

	virtual void output(int seqno, int reason = 0);

	bool timeout_state;

};

#endif
