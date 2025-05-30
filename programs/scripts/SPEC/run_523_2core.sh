cd /home/ubuntu/523_ori/
m5 resetstats
taskset -c 0 ./cpuxalan_r -v t5.xml xalanc.xsl &
taskset -c 1 ./cpuxalan_r -v t5.xml xalanc.xsl
