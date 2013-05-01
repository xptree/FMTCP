#
# mptcp
#
set ns [new Simulator]

#
# specify to print mptcp option information
#
Trace set show_tcphdr_ 0

#
# setup trace files
#
set f [open mp_out.tr w]
$ns trace-all $f
set nf [open mp_out.nam w]
$ns namtrace-all $nf

#puts [expr [lindex $argv 0] + [lindex $argv 1]]

#
# mptcp sender
#
set n0 [$ns node]
set n0_0 [$ns node]
set n0_1 [$ns node]
#set n0_2 [$ns node]

$n0 color red
$n0_0 color red
$n0_1 color red
#$n0_2 color red

$ns multihome-add-interface $n0 $n0_0
$ns multihome-add-interface $n0 $n0_1
#$ns multihome-add-interface $n0 $n0_2

#
# mptcp receiver
#
set n1 [$ns node]
set n1_0 [$ns node]
set n1_1 [$ns node]
#set n1_2 [$ns node]

$n1 color blue
$n1_0 color blue
$n1_1 color blue
#$n1_2 color blue

$ns multihome-add-interface $n1 $n1_0
$ns multihome-add-interface $n1 $n1_1
#$ns multihome-add-interface $n1 $n1_2

$ns duplex-link $n0_0 $n1_0 54Mb [lindex $argv 0]ms DropTail
$ns duplex-link $n0_1 $n1_1 54Mb [lindex $argv 2]ms DropTail
#$ns duplex-link $n0_2 $n1_2 1Mb 100ms DropTail

set tcp0 [new Agent/TCP/MPTCPSubflow]
$tcp0 set window_ [lindex $argv 3]
$ns attach-agent $n0_0 $tcp0

set tcp1 [new Agent/TCP/MPTCPSubflow]
$tcp1 set window_ [lindex $argv 3]
$ns attach-agent $n0_1 $tcp1

#set tcp2 [new Agent/TCP/MPTCPSubflow]
#$tcp2 set window_ 50
#$ns attach-agent $n0_2 $tcp2

set mptcp [new Agent/MPTCP]
$mptcp set window_ [lindex $argv 3]
$mptcp set block_size_ [lindex $argv 4]
$mptcp attach-tcp $tcp0
$mptcp attach-tcp $tcp1
#$mptcp attach-tcp $tcp2
$ns multihome-attach-agent $n0 $mptcp
set ftp [new Application/FTP]
$ftp attach-agent $mptcp


#
# create mptcp receiver
#
set mptcpsink [new Agent/MPTCPSink]
$mptcpsink set window_ [lindex $argv 3]
$mptcpsink set block_size_ [lindex $argv 4]

set sink0 [new Agent/TCPSink/MPTCPSubflowSink]
$sink0 set window_ [lindex $argv 3]
$ns attach-agent $n1_0 $sink0 

set sink1 [new Agent/TCPSink/MPTCPSubflowSink]
$sink1 set window_ [lindex $argv 3]
$ns attach-agent $n1_1 $sink1 

#set sink2 [new Agent/TCPSink/MPTCPSubflowSink]
#$ns attach-agent $n1_2 $sink2

$mptcpsink attach-tcpsink $sink0
$mptcpsink attach-tcpsink $sink1
#$mptcpsink attach-tcpsink $sink2

$ns multihome-attach-agent $n1 $mptcpsink
$ns multihome-connect $mptcp $mptcpsink


#LINK ERROR
set em [new ErrorModel]
$em unit pkt
$em set rate_ [lindex $argv 1]
$em ranvar [new RandomVariable/Uniform]
$em drop-target [new Agent/Null]
$ns link-lossmodel $em $n0_0 $n1_0


# multi-state errormodel
#set tmp1 [new ErrorModel/Uniform .002 pkt]
#set tmp2 [new ErrorModel/Uniform .35 pkt]
# Array of states (error models)
#set m_states [list $tmp1 $tmp2]
# Durations for each of the states, tmp, tmp1 and tmp2, respectively
#set m_periods [list 100 100]
# Transition state model matrix
#set m_transmx { {0 1}
#		{1 0} }
#set m_trunit pkt
# Use time-based transition
#set m_sttype time
#set m_nstates 2
#set m_nstart [lindex $m_states 0]
#set em [new ErrorModel/MultiState $m_states $m_periods $m_transmx $m_trunit $m_sttype $m_nstates $m_nstart]

set em [new ErrorModel]
$em unit pkt
$em set rate_ [lindex $argv 5]
$em ranvar [new RandomVariable/Uniform]
$em drop-target [new Agent/Null]
$ns link-lossmodel $em $n0_1 $n1_1

#set em [new ErrorModel]
#$em unit pkt
#$em set rate_ 0.01
#$em ranvar [new RandomVariable/Uniform]
#$em drop-target [new Agent/Null]
#$ns link-lossmodel $em $n0_2 $n1_2

set f0 [open mp_test_0.txt w]
set f1 [open mp_test_1.txt w]

proc record {} {
	global ns f0 tcp0
	global f1 tcp1
	set now [$ns now]
	puts $f0 "$now\t[$tcp0 set rtt_]\t[$tcp0 set cwnd_]\t[$tcp0 set window_]\t[$tcp0 set ssthresh_]"
	puts $f1 "$now\t[$tcp1 set rtt_]\t[$tcp1 set cwnd_]\t[$tcp1 set window_]\t[$tcp1 set ssthresh_]"
	$ns at [expr $now + 1] "record"
}



$ns at 0.0 "$ftp start"   
$ns at 0.1 "record"      
$ns at 300 "finish"


proc finish {} {
	global ns f 
	global nf
	$ns flush-trace
	close $f
	close $nf

#   	set awkcode {
#   	 	{	if (($1 == "r" || $1 == "d" ) && $5 == "tcp"  &&  ($3 == "1" || $3 == "2")) {
#   	 			
#				print $2, $3, $4>> "mptcp-throughput";
#					if ($3 == "1" && $4 == "4"  ) {
#						print $2, $3, $4 >> "sub1-throughput";
#					}				
#					if ($3 == "2" && $4 == "5" ) {
#						print $2, $3, $4 >> "sub2-throughput";
#					}
#				}
#			}
#		} 
#	exec rm -f sub1-throughput sub2-throughput mptcp-throughput
#	exec awk $awkcode out.tr
#	exec xgraph -M -m -nl tcp1 tcp2 sub1 sub2
#   	exec nam out.nam &
	exit
}

$ns run
