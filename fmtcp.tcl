#
# FMTCP
#
set ns [new Simulator]

#
# specify to print FMTCP option information
#
Trace set show_tcphdr_ 0

#
# setup trace files
#
set f [open fm_out.tr w]
$ns trace-all $f
#set nf [open fm_out.nam w]
#$ns namtrace-all $nf


#
# FMTCP sender
#
set n0 [$ns node]
set n0_0 [$ns node]
set n0_1 [$ns node]

$n0 color red
$n0_0 color red
$n0_1 color red

$ns multihome-add-interface $n0 $n0_0
$ns multihome-add-interface $n0 $n0_1

#
# FMTCP receiver
#
set n1 [$ns node]
set n1_0 [$ns node]
set n1_1 [$ns node]

$n1 color blue
$n1_0 color blue
$n1_1 color blue

$ns multihome-add-interface $n1 $n1_0
$ns multihome-add-interface $n1 $n1_1

$ns duplex-link $n0_0 $n1_0 54Mb [lindex $argv 0]ms DropTail
$ns duplex-link $n0_1 $n1_1 54Mb [lindex $argv 2]ms DropTail

set tcp0 [new Agent/TCP/FMTCPSubflow]
$tcp0 set window_ [lindex $argv 3]
$ns attach-agent $n0_0 $tcp0

set tcp1 [new Agent/TCP/FMTCPSubflow]
$tcp1 set window_ [lindex $argv 3]
$ns attach-agent $n0_1 $tcp1


set FMTCP [new Agent/FMTCP]
$FMTCP set window_ [lindex $argv 3]
$FMTCP set block_size_ [lindex $argv 4]
$FMTCP set append_pkts_ [lindex $argv 6]
$FMTCP attach-tcp $tcp0
$FMTCP attach-tcp $tcp1
$ns multihome-attach-agent $n0 $FMTCP
set ftp [new Application/FTP]
$ftp attach-agent $FMTCP


#
# create FMTCP receiver
#
set FMTCPsink [new Agent/FMTCPSink]
$FMTCPsink set window_ [lindex $argv 3]
$FMTCPsink set block_size_ [lindex $argv 4]

set sink0 [new Agent/TCPSink/FMTCPSubflowSink]
$ns attach-agent $n1_0 $sink0 

set sink1 [new Agent/TCPSink/FMTCPSubflowSink]
$ns attach-agent $n1_1 $sink1 


$FMTCPsink attach-tcpsink $sink0
$FMTCPsink attach-tcpsink $sink1

$ns multihome-attach-agent $n1 $FMTCPsink
$ns multihome-connect $FMTCP $FMTCPsink


#FMTCP ERROR
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

set f0 [open fm_test_0.txt w]
set f1 [open fm_test_1.txt w]

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
	#close $nf

#  	set awkcode {
#   	 	{	if (($1 == "r" || $1 == "d" ) && $5 == "tcp"  &&  ($3 == "1" || $3 == "2")) {
#   	 			
#				print $2, $3, $4>> "fmtcp-throughput.tr";
#					if ($3 == "1" && $4 == "4"  ) {
#						print $2, $3, $4 >> "sub1-throughput.tr";
#					}				
#					if ($3 == "2" && $4 == "5" ) {
#						print $2, $3, $4 >> "sub2-throughput.tr";
#					}
#				}
#			}
#		}

#	exec rm -f sub1-throughput.tr sub2-throughput.tr fmtcp-throughput.tr
#	exec awk $awkcode out.tr

#	exec xgraph -M -m -nl tcp1 tcp2 sub1 sub2
#  	exec nam out.nam &
	exit
}

$ns run
