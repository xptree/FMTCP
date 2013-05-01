#ifndef MPTCP_SINK_H_
#define MPTCP_SINK_H_

#include "tcp.h"
#include "tcp-sink.h"
#include "agent.h"
#include "packet.h"
#include "classifier.h"

#include <map>
#include <vector>
#include <string>

const int recv_buffer_size = 1024; //both for subflow and receiver
const int MSS = 1024; //Maximum segment size for MPTCP, 1024 bytes currently
const int block_packets = 32; //the block size, used in method sendmsg

class MptcpSink;
class MptcpSubflowSink;
class MpAcker;

//The information of subflow
struct SubflowSink {
	MptcpSubflowSink *tcpsink_;
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


class MptcpSink : public Agent {
public:
	MptcpSink();
	virtual ~MptcpSink();
	virtual void recv(Packet*, Handler*);
	int command(int argc, const char* const* argv);
	
 	bool update(int mptcp_seqno, int subflow_id);
	
	SubflowSink* find_subflow(int addr);
	SubflowSink* find_subflow(int addr, int port);
	
	int get_last_ack() {return next_ - 1;}
	
	void totrace(std::string msg);
	void totest(std::string msg);	
	
protected:
	FILE *tracefile_;
	FILE* testinfo_;  // for debugging
	
	std::vector<SubflowSink*> subflow_; //To manage all the subflows attached to this mptcp agent
	std::vector<Dstinfor> dstinfor_;   //To manage all the destinations
	
	// for window
	double wnd_; 
	int block_packets;
	virtual void delay_bind_init_all();
	virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);
	//void reset();
	// @wang
	
	bool buffer_[recv_buffer_size]; //receive buffer 
	int next_; //expected (mptcp) sequence no 
	
	Classifier *core_; //classifier for MptcpSink
	
	bool check_routable(int sid, int addr, int port); //check if we can reach (addr,port) using subflow sid
	void add_destination(int addr, int port); //add a destination with pair (addr, port)
};

class MptcpSubflowSink : public TcpSink {
public:
	MptcpSubflowSink(Acker *acker);
	virtual ~MptcpSubflowSink();
	virtual void recv(Packet*, Handler*);
	
	void mptcpsink_setdata(int id, MptcpSink *core) {subflow_id_ = id; core_ = core; } //mptcp sink setting data
	MptcpSink *core_; //the mptcpsink agent it is attached to	
	int get_id() {return subflow_id_; }
	
	void totrace(std::string msg);
protected:
	FILE *tracefile_;
	
	int subflow_id_; //subflow id
	
	int buffer_[recv_buffer_size]; //store mptcp_seqno
	int next_;
	//int last_update_;	
	
	// for window
	double wnd_; 	
	virtual void delay_bind_init_all();
	virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);
	//void reset();
	// @wang
	
};


#endif
