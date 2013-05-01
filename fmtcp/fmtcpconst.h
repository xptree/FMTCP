#ifndef FMTCP_CONST_H_
#define FMTCP_CONST_H_

//constants in FMTCP protocol

const int max_subflow = 10; //the maximum number of subflows
const int max_block = 100000; //the maximum number of blocks

const int block_pkts = 32; //receiving 64 packets can successfully decode a block (equivaletly, the number of symbols for one block)
const int MSS = 1024;  // size_
const double fail_threshold = 0.1; //probability of unsuccessful decoding should be less than this threshold

const int send_buffer_size = 1024; //send buffer size (measured in KB i.e. packets)
const int send_buffer_block = send_buffer_size / block_pkts; //measured in #block
const int recv_buffer_size = 1024; //recv buffer size (measured in KB i.e. packets)
const int recv_buffer_block = recv_buffer_size / block_pkts; //measured in #block

#endif
