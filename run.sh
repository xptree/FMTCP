#!/bin/bash
i=1
sum=0
while [ $i -le 100 ]
do
  sum=$(($sum+$i))
  let i++
done
echo $sum

rm *.tr 
rm MyRecord

:fmbufgoodput
delay1=100
lr1=0.001
block=32

delay2=100
buf=128
app=1
lr2=0.001

bufarr=(32 64 128 256 512)
lrarr=(0.001 0.002 0.005 0.01 0.02 0.05 0.1 0.2 0.5)
apparr=(0 0 1 1 1 1 2 2 2)
arrlen=${#bufarr[@]}
a2=${bufarr[2]}

delay2=100
i=0
./argout fmtcp delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#bufarr[@]} ]
do
    buf=${bufarr[$i]}
  	j=0
	./argout buf=$buf
	./argout lr ${lrarr[*]} 
  	while [ $j -lt ${#lrarr[@]} ]
  	do
  		lr2=${lrarr[$j]}
		app=${apparr[$j]}
  		ns fmtcp/fmtcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2 $app
		./trace fm
  		./cntftp fm
  		let j++
  	done
  	let i++
done

echo 100ms done
./argout

delay2=300
i=0
./argout fmtcp delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#bufarr[@]} ]
do
    buf=${bufarr[$i]}
  	j=0
	./argout buf=$buf
	./argout lr ${lrarr[*]} 
  	while [ $j -lt ${#lrarr[@]} ]
  	do
  		lr2=${lrarr[$j]}
		app=${apparr[$j]}
  		ns fmtcp/fmtcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2 $app
		./trace fm
  		./cntftp fm
  		let j++
  	done
  	let i++
done

echo 300ms done
./argout

delay2=150
i=0
./argout fmtcp delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#bufarr[@]} ]
do
    buf=${bufarr[$i]}
  	j=0
	./argout buf=$buf
	./argout lr ${lrarr[*]} 
  	while [ $j -lt ${#lrarr[@]} ]
  	do
  		lr2=${lrarr[$j]}
		app=${apparr[$j]}
  		ns fmtcp/fmtcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2 $app
		./trace fm
  		./cntftp fm
  		let j++
  	done
  	let i++
done
echo 150ms done
./argout

delay2=200
i=0
./argout fmtcp delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#bufarr[@]} ]
do
    buf=${bufarr[$i]}
  	j=0
	./argout buf=$buf
	./argout lr ${lrarr[*]} 
  	while [ $j -lt ${#lrarr[@]} ]
  	do
  		lr2=${lrarr[$j]}
		app=${apparr[$j]}
  		ns fmtcp/fmtcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2 $app
		./trace fm
  		./cntftp fm
  		let j++
  	done
  	let i++
done
echo 200ms done
./argout
mv MyRecord MyRecord-buf-goodput-fm

:mpbufgoodput
delay1=100
lr1=0.001
block=32

delay2=100
buf=128
app=1
lr2=0.001

bufarr=(32 64 128 256 512)
lrarr=(0.001 0.002 0.005 0.01 0.02 0.05 0.1 0.2 0.5)
apparr=(0 0 1 1 1 1 2 2 2)
arrlen=${#bufarr[@]}
a2=${bufarr[2]}

delay2=100
i=0
./argout mptcp delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#bufarr[@]} ]
do
    buf=${bufarr[$i]}
  	j=0
	./argout buf=$buf
	./argout lr2 ${lrarr[*]} 
  	while [ $j -lt ${#lrarr[@]} ]
  	do
  		lr2=${lrarr[$j]}		
  		ns mptcp/mptcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2
		./trace mp
  		./cntftp mp
  		let j++
  	done
  	let i++
done
echo 100ms done
./argout

delay2=300
i=0
./argout mptcp delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#bufarr[@]} ]
do
    buf=${bufarr[$i]}
  	j=0
	./argout buf=$buf
	./argout lr2 ${lrarr[*]} 
  	while [ $j -lt ${#lrarr[@]} ]
  	do
  		lr2=${lrarr[$j]}		
  		ns mptcp/mptcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2
		./trace mp
  		./cntftp mp
  		let j++
  	done
  	let i++
done
echo 300ms done
./argout

delay2=150
i=0
./argout mptcp delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#bufarr[@]} ]
do
    buf=${bufarr[$i]}
  	j=0
	./argout buf=$buf
	./argout lr2 ${lrarr[*]} 
  	while [ $j -lt ${#lrarr[@]} ]
  	do
  		lr2=${lrarr[$j]}		
  		ns mptcp/mptcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2
		./trace mp
  		./cntftp mp
  		let j++
  	done
  	let i++
done
echo 150ms done
./argout

delay2=200
i=0
./argout mptcp delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#bufarr[@]} ]
do
    buf=${bufarr[$i]}
  	j=0
	./argout buf=$buf
	./argout lr2 ${lrarr[*]} 
  	while [ $j -lt ${#lrarr[@]} ]
  	do
  		lr2=${lrarr[$j]}		
  		ns mptcp/mptcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2
		./trace mp
  		./cntftp mp
  		let j++
  	done
  	let i++
done
echo 200ms done
./argout

mv MyRecord MyRecord-buf-goodput-mp


:fmapp
delay1=100
lr1=0.001
delay2=100

bufarr=(32 64 128 256 512)
lrarr=(0.001 0.002 0.005 0.01 0.02 0.05 0.1 0.2 0.5)
apparr=(0 1 2 3 4 5 6 7 8)

block=32
buf=128
i=0
./argout fmtcp block32 appendpkts delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block buf=$buf
while [ $i -lt ${#lrarr[@]} ]
do
    lr2=${lrarr[$i]}
  	j=0
	./argout lr2=$lr2
	./argout append ${apparr[*]} 
  	while [ $j -lt ${#apparr[@]} ]
  	do
  		app=${apparr[$j]}		
  		ns fmtcp/fmtcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2 $app
		./trace fm
  		./cntftp fm
		./trace fm
		./cntftp fm 32 2
  		let j++
  	done
  	let i++
done

echo 32block done
./argout

block=64
buf=128
i=0
./argout fmtcp block64 appendpkts delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block buf=$buf
while [ $i -lt ${#lrarr[@]} ]
do
    lr2=${lrarr[$i]}
  	j=0
	./argout lr2=$lr2
	./argout append ${apparr[*]} 
  	while [ $j -lt ${#apparr[@]} ]
  	do
  		app=${apparr[$j]}		
  		ns fmtcp/fmtcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2 $app
		./trace fm
  		./cntftp fm 64
  		let j++
  	done
  	let i++
done

echo 64block done
./argout
mv MyRecord MyRecord-append

:goodputmpfm
delay1=100
lr1=0.001
buf=128
block=32

delayarr=(100 150 200 300 500 100 150 200 300 500)
lrarr=(0.001 0.001 0.001 0.001 0.001 0.01 0.01 0.01 0.01 0.01)
apparr=(0 0 0 0 0 1 1 1 1 1)

i=0
./argout fmtcp goodput delay1=$delay1 lr1=$lr1 block=$block buf=$buf
./argout delay2 ${delayarr[*]} 
./argout lr2 ${lrarr[*]} 
./argout append ${apparr[*]} 
while [ $i -lt ${#lrarr[@]} ]
do
	delay2=${delayarr[$i]}
    lr2=${lrarr[$i]}
	app=${apparr[$i]}  	
  	ns fmtcp/fmtcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2 $app
	./trace fm
  	./cntftp fm  	
  	let i++
done

echo fmtcp goodput done
./argout

delay1=100
lr1=0.001
buf=128
block=32

delayarr=(100 150 200 300 500 100 150 200 300 500)
lrarr=(0.001 0.001 0.001 0.001 0.001 0.01 0.01 0.01 0.01 0.01)
apparr=(0 0 0 0 0 1 1 1 1 1)

i=0
./argout mptcp goodput delay1=$delay1 lr1=$lr1 block=$block buf=$buf
./argout delay2 ${delayarr[*]} 
./argout lr2 ${lrarr[*]} 
while [ $i -lt ${#lrarr[@]} ]
do
	delay2=${delayarr[$i]}
    lr2=${lrarr[$i]}	 	
  	ns mptcp/mptcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2
	./trace mp
  	./cntftp mp 	
  	let i++
done

echo mptcp goodput done
./argout
mv MyRecord MyRecord-goodput

exit

:bufdelaympfm
delay1=100
lr1=0.001
block=32
bufarr=(32 64 128 256 384 512 640 768 896 1024)
lrarr=(0.001 0.01 0.05)
apparr=(0 1 1)

delay2=100
i=0
./argout fmtcp buf-delay delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#lrarr[@]} ]
do
    lr2=${lrarr[$i]}
	app=${apparr[$i]}
  	j=0
	./argout lr2=$lr2 app=$app
	./argout buf ${bufarr[*]} 
  	while [ $j -lt ${#bufarr[@]} ]
  	do
  		buf=${bufarr[$j]}		
  		ns fmtcp/fmtcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2 $app
		./trace fm
  		./cntftp fm
  		let j++
  	done
  	let i++
done

echo fmtcp buf-delay 100ms done
./argout

delay2=100
i=0
./argout mptcp buf-delay delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#lrarr[@]} ]
do
    lr2=${lrarr[$i]}
	app=${apparr[$i]}
  	j=0
	./argout lr=$lr2 app=$app
	./argout buf ${bufarr[*]} 
  	while [ $j -lt ${#bufarr[@]} ]
  	do
  		buf=${bufarr[$j]}		
  		ns mptcp/mptcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2
		./trace mp
  		./cntftp mp
  		let j++
  	done
  	let i++
done

echo mptcp buf-delay 100ms done
./argout

delay2=300
i=0
./argout fmtcp buf-delay delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#lrarr[@]} ]
do
    lr2=${lrarr[$i]}
	app=${apparr[$i]}
  	j=0
	./argout lr=$lr2 app=$app
	./argout buf ${bufarr[*]} 
  	while [ $j -lt ${#bufarr[@]} ]
  	do
  		buf=${bufarr[$j]}		
  		ns fmtcp/fmtcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2 $app
		./trace fm
  		./cntftp fm
  		let j++
  	done
  	let i++
done

echo fmtcp buf-delay 300ms done

delay2=300
i=0
./argout mptcp buf-delay delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#lrarr[@]} ]
do
    lr2=${lrarr[$i]}
	app=${apparr[$i]}
  	j=0
	./argout lr=$lr2 app=$app
	./argout buf ${bufarr[*]} 
  	while [ $j -lt ${#bufarr[@]} ]
  	do
  		buf=${bufarr[$j]}		
  		ns mptcp/mptcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2
		./trace mp
  		./cntftp mp
  		let j++
  	done
  	let i++
done
echo mptcp buf-delay 300ms done
./argout

delay2=200
i=0
./argout fmtcp buf-delay delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#lrarr[@]} ]
do
    lr2=${lrarr[$i]}
	app=${apparr[$i]}
  	j=0
	./argout lr2=$lr2 app=$app
	./argout buf ${bufarr[*]} 
  	while [ $j -lt ${#bufarr[@]} ]
  	do
  		buf=${bufarr[$j]}		
  		ns fmtcp/fmtcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2 $app
		./trace fm
  		./cntftp fm
  		let j++
  	done
  	let i++
done
echo fmtcp buf-delay 200ms done
./argout

delay2=200
i=0
./argout mptcp buf-delay delay1=$delay1 lr1=$lr1 delay2=$delay2 block=$block 
while [ $i -lt ${#lrarr[@]} ]
do
    lr2=${lrarr[$i]}
	app=${apparr[$i]}
  	j=0
	./argout lr=$lr2 app=$app
	./argout buf ${bufarr[*]} 
  	while [ $j -lt ${#bufarr[@]} ]
  	do
  		buf=${bufarr[$j]}		
  		ns mptcp/mptcp.tcl $delay1 $lr1 $delay2 $buf $block $lr2
		./trace mp
  		./cntftp mp
  		let j++
  	done
  	let i++
done
echo mptcp buf-delay 200ms done
./argout

mv MyRecord MyRecord-buf-delay

