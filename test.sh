#!/bin/sh

i=0
while [ $i -lt 5 ]
do
#	./lab1_tester | tail -n1 >> test_result
#	i=$(expr $i + 1)
./test-lab-2-a.pl ./yfs1 | tail >> testresult
./test-lab-2-b.pl ./yfs1 | tail >> testresult
./test-lab-2-c.pl ./yfs1 | tail >> testresult
done
