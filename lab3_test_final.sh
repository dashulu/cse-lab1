#!/bin/bash
i=0
export RPC_LOSSY=0
while [ $i -lt 10 ]
do
  ./start.sh 0
  echo $i+"th test a:"
  time ./test-lab-3-a yfs1 yfs2 >> test_result
  echo $i+"th test b:"
  time ./test-lab-3-b yfs1 yfs2 >> test_result
  i=$(expr $i + 1)
  ./stop.sh
  ./stop.sh
done

echo "lossy=0 test a result:" >> test_result_stat
cat test_result | grep "Passed all test" | wc -l >> test_result_stat
cat test_result | grep "tests completed OK" | wc -l >> test_result_stat

export RPC_LOSSY=5
i=0
while [ $i -lt 10 ]
do
  ./start.sh 5
  echo $i+"th test a:"
  time ./test-lab-3-a yfs1 yfs2 >> test_result
  echo $i+"th test b:"
  time ./test-lab-3-b yfs1 yfs2 >> test_result
  i=$(expr $i + 1)
  ./stop.sh
  ./stop.sh
done
echo "lossy=5 test a result:" >> test_result_stat
cat test_result | grep "Passed all test" | wc -l >> test_result_stat
cat test_result | grep "tests completed OK" | wc -l >> test_result_stat

