cd /home/ubuntu/507_ori/
m5 resetstats
taskset -c 0 ./cactusBSSN_r spec_ref.par &
taskset -c 1 ./cactusBSSN_r spec_ref.par
