#ifndef FMTCP_H_
#define FMTCP_H_

/*1. Add these lines to the header of Tcp packet: 
  	//For fmtcp function
  	
	int fmtcp_seqno_;
	int& fmtcp_seqno() {return fmtcp_seqno_; }
	
	int fmtcp_seqno_ack_;
	int& fmtcp_seqno_ack() {return fmtcp_seqno_ack_; }
	
	int fmtcp_symbolnum_ack_;
	int& fmtcp_symbolnum_ack() {return fmtcp_symbolnum_ack_; }
	
*/

#include "tcp.h"
#include "agent.h"
#include "packet.h"
#include "classifier.h"

#include "fmtcpconst.h"

#include <vector>
#include <map>
#include <set>
#include <deque>
#include <string>

class FmtcpAgent;
class FmtcpSubflow;

//The information of subflow
struct Subflow {
	FmtcpSubflow *tcp_;
	int id_;            //subflow id
	int addr_, port_;   //address and port number for the subflow
	int daddr_, dport_; //the destination of address and port number for the subflow
	double scwnd_;      //smoothed congestion window (for this subflow)
	double EAT_;
	int virt_num_;
	NsObject *link_;
	NsObject *target_;
	
	Subflow() : tcp_(NULL), addr_(0), port_(0), daddr_(-1), dport_(-1), scwnd_(0), EAT_(0), virt_num_(0), link_(NULL), target_(NULL) {}
};

struct EAT_ID {
	double EAT;
	int WIN;
	int ID;
	
	EAT_ID() : EAT(0), WIN(0), ID(0) {}
};

//the information of the destinations
struct Dstinfor {
	int addr_, port_; 
	Dstinfor() : addr_(0), port_(0) {}
};

//the information of each symbol
struct SymbolInfo {
	double time_; //sent time
	int sid_;     //sent subflow
	SymbolInfo() : time_(0), sid_(-1) {}
	SymbolInfo(double xt, int sid) : time_(xt), sid_(sid) {}
};

class FmtcpAgent : public Agent {

public:
	FmtcpAgent();
	~FmtcpAgent();
	virtual void recv(Packet* pkt, Handler*);
	//virtual void timeout(int tno); 
	int command(int argc, const char* const* argv);	
	virtual void sendmsg(int nbytes, const char *flags = 0);
	virtual void advanceby(int delta);
	//Functions above are all public functions of class TcpAgent	
	
	void update_ack(int lastack_seqno, int fmtcp_seqno_ack, int symbolnum, int subflow_id, bool force = true);       //ack block_seqno, #symbols recved
	void update_timeout(int fmtcp_seqno_ack, int subflow_id, bool force = true);                                     //time out, block_seqno
	int get_fmtcp_seqno(int subflow_id = 0);                                  //get fmtcp_seqno for a new delivery
	void send_much(int subflow_id, int maxburst = 0); //send packets	
	void totrace(std::string msg);	
	void totest(std::string msg);
	void totelnet(std::string msg);	
	void tolost(std::string msg);
	
	void update_EDT(double dt, int sid);
	
	int get_last_ack() {return last_ack_; }
	int get_curseq() {return curseq_; }		
	
	double get_wnd();
	int remaining_wnd(int sender_wnd); // receive remaining
	
protected:
	FILE *tracefile_;
	FILE *testinfo_;
	FILE *fortelnet_;
	FILE *lostinfo_;

	std::vector<Subflow*> subflow_;    //subflows	
	std::vector<Dstinfor> dstinfor_;   //To manage all the destinations	
	
	// for lost count
	int lost_count_[block_pkts];
	// for EAT rank
	std::vector<EAT_ID> rank_;
	int get_first_subflow();
	void get_eat();
	// @wang
	
	// for data seqno and ack @wang
	int unacked_pkts_;
	int t_data_seqno_;
	int last_data_ack_;	
	
	// for scheduling
	int appending_block_;
	int scheduled_[send_buffer_block];
	
	int compute_appendpackets(double loss_rate);
	
	// for window
	double wnd_; 
	int block_packets;
	int extra_packets;
	virtual void delay_bind_init_all();
	virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);
	//void reset();
	// @wang
	
	double loss_rate_; 		   //(estimated) loss rate 
	int append_packets_;		   //the number of appended packets for one block

	//@wang	
	int symbols_actualsent_[send_buffer_block];
	int symbols_out_[send_buffer_block]; 
	//@wang
	int symbols_acked_[send_buffer_block];     //the number of symbols acked for each block
	int symbols_lost_[send_buffer_block];
	int symbols_sent_[send_buffer_block];      //the number of symbols sent for each block	 
	double send_ok_time_[max_block];   	   //the time when the block is successfully delivered to the other end
	double send_start_time_[max_block];
	//SymbolInfo symbols_[send_buffer_block][block_packets * 2];
	
	double EDT_[max_subflow];
	int cnt_EDT_[max_subflow];
	double lastsent_[max_subflow];    //the last time this subflow sent a packet
	//int rank_; 			  //the rank of the packet that is to be sent
	
	int curseq_;    //the sequence number (block) it should send
	int t_seqno_;   //current sequence number (block)	
	int last_ack_;  //the last successfully delivered block	
	int unack_symbols_; //the number of unack symbols
	int current_databytes_;
		
		
	double trace_arrive_time_[max_block];
	double trace_send_time_[max_block];
	int arvtop_, sdtop_;
		
	Classifier *core_; //classifier of fmtcp agent
	
	Subflow *find_subflow(int daddr); 
	Subflow *find_subflow(int daddr, int dport);
	bool check_routable(int sid, int addr, int port); //check whether we can send packet to (addr,port) using subflow sid
	void add_destination(int addr, int port); //add a destination pair (addr, port)		

};

class FmtcpSubflow : public TcpAgent {

public:
	FmtcpSubflow();
	~FmtcpSubflow();
	
	void fmtcp_setdata(int id, FmtcpAgent *core); //allow fmtcp agent to set some data
	
	virtual void recv(Packet*, Handler*);
	virtual void timeout(int tno); 	
	void send_much(int block_seqno, int reason, int maxburst = 0); //modify the "sliding window" part of this function 								 
	void triple_ack();	//triple ack, treates as if the packet was lost
	
	int& fmtcp_get_subflow_id() {return subflow_id_; }
	int fmtcp_get_window() {return window(); } //return the current window
	double fmtcp_get_cwnd() {return cwnd_; } //return the congestion window
	int fmtcp_get_ssthresh() {return ssthresh_; } //return the slowstart threshold
	int fmtcp_get_tseqno() {return t_seqno_; } //return the current subflow sequence number
	int fmtcp_get_highest_ack() {return highest_ack_; } //return tha last acked subflow seqno
	double fmtcp_get_srtt(); //{return (int(t_srtt_) >> T_SRTT_BITS)*tcp_tick_; } //return the smoothed round-trip time
	double fmtcp_get_trtt(); //{return (double)t_rtt_* tcp_tick_; } //return the round-trip time
	double fmtcp_get_RTO() {return t_rtxcur_; } //return the current RTO time	
	int fmtcp_get_size() {return size_; }	
	
	// send remaining  
	int fmtcp_get_remaining_window();
	
	/*int sender_remaining_wnd() {
		return  window() - (t_seqno_ - last_ack_ - 1);
	}*/
	
	double get_EAT(int count);
	double get_earliest();
	
	void totrace(std::string msg);
	void totest(std::string msg); 
	
protected:
	FILE *tracefile_;
	FILE *testinfo_;

	int subflow_id_;

	FmtcpAgent *core_;	
	
	int sent_[send_buffer_size];
	
	int last_ack_;
	int t_seqno_;	
	int cnt_dupack_; //used in fast retransmission
	int target_ack_; //used in fast recovery
	
	// for Reno-action
	int dupwnd_;
	virtual int window();	
	virtual double windowd();
	
	// @wang
	// for estimation 
	int lost_num_;
	double lossrate_;
	double EDT_;
	double RT_;
	double EAT_;
	std::deque<double> send_time_;
	// for scheduling
	int block_cnt;
	// @wang	
	int highest_seqno_;
	
	void output(int block_seqno, int seqno, int reason); //remove the "timer set" part of this function


};

#endif
