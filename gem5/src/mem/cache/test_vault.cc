/*
 * @Author: Xinrui Wang 
 * @Date: 2023-04-06 15:37:14 
 * @Last Modified by: Xinrui Wang
 * @Last Modified time: 2023-04-07 19:17:56
 */

#include "vault.hh"
int main(){
    Enclave::vault V1(true,2);
    V1.writeTrace(0x100);
    V1.writeTrace(0x10000);
    for (size_t i = 0; i < 128; i++)
    {
        V1.writeTrace(0x10000);
    }
    
    V1.readTrace(0x100);
    return 0;
}