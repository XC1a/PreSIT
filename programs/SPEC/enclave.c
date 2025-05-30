#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

void _EncStart(){
    printf("Begin init from software\n");
    int fp0=open("/dev/mem",O_RDWR|O_SYNC);
	if(fp0<0){
		printf("cannot open /dev/mem\n");
		exit(0);
	}
    printf("Open dev mem YES\n");
    void *vmem;
    //the last arg is aligned to 4KiB
    u_int64_t ENCLAVE_STARTADDR = 0x3d077a000;
    u_int64_t ENCLAVE_SIZE=0xA0000000; // 16GB
    unsigned int ENCLAVE_end=(ENCLAVE_SIZE-64)/8;
    vmem=mmap(NULL,ENCLAVE_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fp0,ENCLAVE_STARTADDR);
    if (vmem==NULL)
    {
        printf("Can not map the region\n");
        exit(0);
    }
    // memset(vmem,0,ENCLAVE_SIZE);

    char* ch = (char*)vmem;
    u_int64_t* u64 = (u_int64_t*)vmem;
    u64[ENCLAVE_end] = INST_COUNT;
    ch[0] = 0x1;
    printf("Begin test YES. Inst counter is %lld\n",INST_COUNT);

}