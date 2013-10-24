#!/bin/sh

i=0
while [ $i -lt 100 ]
do
	./lab1_tester | tail -n1 >> test_result
	i=$(expr $i + 1)
done
