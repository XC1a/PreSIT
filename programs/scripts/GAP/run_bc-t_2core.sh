cd /home/ubuntu/GAP
m5 resetstats
taskset -c 0 ./bc -f twitter.sg -i4 -n16 &
taskset -c 1 ./bc -f twitter.sg -i4 -n16
