cd /home/ubuntu/GAP
m5 resetstats
taskset -c 0 ./bc -f web.sg -i4 -n16 &
taskset -c 1 ./bc -f web.sg -i4 -n16 &
taskset -c 2 ./bc -f web.sg -i4 -n16 &
taskset -c 3 ./bc -f web.sg -i4 -n16 