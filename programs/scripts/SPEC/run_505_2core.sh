cd /home/ubuntu/505_ori/
m5 resetstats
taskset -c 0 ./mcf_r inp.in &
taskset -c 1 ./mcf_r inp.in
