cd /home/ubuntu/GAP
m5 resetstats
taskset -c 0 ./pr -f web.sg -i1000 -t1e-4 -n16 &
taskset -c 1 ./pr -f web.sg -i1000 -t1e-4 -n16
