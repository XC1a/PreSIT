cd /home/ubuntu/520_ori/
m5 resetstats
taskset -c 0 ./omnetpp_r -c General -r 0 &
taskset -c 1 ./omnetpp_r -c General -r 0
