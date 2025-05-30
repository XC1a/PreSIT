
cd /home/ubuntu/531_ori/
m5 resetstats
taskset -c 0 ./deepsjeng_r ref.txt &
taskset -c 1 ./deepsjeng_r ref.txt
