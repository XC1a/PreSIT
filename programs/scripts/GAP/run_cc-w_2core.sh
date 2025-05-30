cd /home/ubuntu/GAP
m5 resetstats
taskset -c 0 ./cc -f web.sg -n16 &
taskset -c 1 ./cc -f web.sg -n16
