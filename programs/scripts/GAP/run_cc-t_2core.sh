cd /home/ubuntu/GAP
m5 resetstats
taskset -c 0 ./cc -f twitter.sg -n16 &
taskset -c 1 ./cc -f twitter.sg -n16
