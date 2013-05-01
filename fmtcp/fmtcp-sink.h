#ifndef FMTCP_SINK_H_
#define FMTCP_SINK_H_

#include "tcp.h"
#include "agent.h"
#include "packet.h"
#include "classifier.h"

#include "fmtcpconst.h"

#include <vector>
#include <map>
#include <set>
#include <string>

class FmtcpSink;
class FmtcpSubflowSink;

//The information of subflow
struct SubflowSink {
	FmtcpSubflowSink *tcpsink_;
	int id_; //subflow id
	int addr_, port_; //the address and port of tcpsink_
	int daddr_, dport_; //the address and port number of the destination
	NsObject *link_;
	NsObject *target_;
	
	SubflowSink() : tcpsink_(NULL), addr_(0), port_(0), daddr_(-1), dport_(-1), link_(NULL), target_(NULL) {}	
};

//The destination information
struct Dstinfor {
	int addr_, port_;
	
	Dstinfor() : addr_(-1), port_(-1) {}
};

class FmtcpSink : public Agent  {
public:
	FmtcpSink();
	virtual ~FmtcpSink();
	void recv(Packet* pkt, Handler* );
	int command(int argc, const char* const* argv);
	
	std::pair<int,int> update(int fmtcp_seqno); //return the last successfully received block seqno
	
	int get_last_ack() {return next_ - 1;}  // block seqno
	
	SubflowSink* find_subflow(int addr);
	SubflowSink* find_subflow(int addr, int port);
	
	void totrace(std::string msg);
	void totest(std::string msg);	
	
protected:
	FILE* tracefile_;
	FILE* testinfo_;  // for debugging

	std::vector<SubflowSink*> subflow_; //manage subflows
	std::vector<Dstinfor> dstinfor_; //manage the destination information

	// for window
	double wnd_; 
	int block_packets;
	virtual void delay_bind_init_all();
	virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);
	//void reset();
	// @wang
	
	int buffer_[recv_buffer_block];  //the number of successfully received symbols for each block
	int next_; //expected (mptcp) sequence no -> block seq
	
	Classifier *core_;
	
	bool check_routable(int sid, int addr, int port); //check if we can reach (addr,port) using subflow sid
	void add_destination(int addr, int port); //add a destination with pair (addr, port)
};

class FmtcpSubflowSink : public TcpSink {
public:
	FmtcpSubflowSink();
	virtual ~FmtcpSubflowSink();
	void recv(Packet* pkt, Handler* );
	
	void fmtcpsink_setdata(int id, FmtcpSink *core); //mptcp sink setting data
	FmtcpSink *core_; //the mptcpsink agent it is attached to	
	int get_id() {return subflow_id_; }
	
	void totrace(std::string msg);
	void totest(std::string msg);
	
protected:
	FILE* tracefile_;
	FILE *testinfo_;
	
	int next_;
	std::set<int> buffer_;
	
	int subflow_id_; //subflow id
	void ack(Packet *pkt, int fmtcp_seqno_ack, int symbolnum);
	
};

#endif
