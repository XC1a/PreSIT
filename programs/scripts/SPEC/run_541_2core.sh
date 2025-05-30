
cd /home/ubuntu/541_ori/
m5 resetstats
taskset -c 0 ./leela_r ref.sgf &
taskset -c 1 ./leela_r ref.sgf
