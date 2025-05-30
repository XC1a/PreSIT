/*
 * @Author: Xinrui Wang 
 * @Date: 2023-04-04 11:14:43 
 * @Last Modified by: Xinrui Wang
 * @Last Modified time: 2023-04-14 18:57:33
 */
/*
This src file contains the class of vault
which realized the function in ASPLOS'18 paper
*/
#ifndef VAULT_HH
#define VAULT_HH

#include "encrypt_lib.hh"
#include <iostream>
#include <vector>
#include <bitset>
#include <tuple>

namespace gem5{

/**
 * Counters: write 1.cache write to mem 2.add the counter and ancestore counters
 * 3.optioanal reseting the counters
 * 
 * MT:write 1. after counter, re-hash the MT
 *    read  check the hash
 * 
 * MACs: encrypt data and counters writting to memory
 *       if compression, just read data entry.
 *       otherwise, one more reading access of MACs
*/

typedef unsigned char    byte;
class vault{
private:
    //********************** MACOR setting of vault
    bool VAULT_DEBUG = false; //open the debug to print some message 
    bool VAULT_SIMULATE = false; //to simulate the vault
    int MEM_SIZE = 4096; //the unit is MB
    int BLK_SIZE = 64; //the unit is B, the size is same with cacheline
    int MT_LEVEL; // the last level is counter. this encrypted by above counters(not protected by hash) 
    unsigned long ENCLAVE_STR_ADDR; //this addr represents the phy addr of the MT+Counter+MACs, initialized by constructor
    bool VAULT_COMP = false; //open the compression indicates 7-b counter needs to be  decremented to 6-b

    int BIT_COUNTER_LEN = VAULT_COMP ? 6 : 7;
    int BIT_L2_LEN = 12;
    int BIT_LH_LEN = 24;


    //********************** Counter and MT
    std::vector<int> mt_level_entry_num;
    // std::vector<std::vector<byte*>> Counter; //the counter mode encryption: counter
    byte*** Counter;
    // byte** Counter_area;
    std::vector<int> Counter_modify_list; //tupe(1) indicates blk entry (512b) index, tuple(2) counter index

public:
    //********************** entryption and hash
    AES AES_Engine;
    void getMTLevel();
    void getCounterKey(int Counter_entry, byte* key_split); //this function will get the counter key from L2(arity 32) level MT
    void initHash();
    void hashBlock(int level, int Counter_entry); //only hash a signal 512-b data block 
    bool checkHashBlock(int level, int Counter_entry); 
    void reHash();

    void allocCounter();
    void getCounterTrace(unsigned long phy_addr);
    void splitCounter(byte* CounterMixed , byte* splitCounter, byte* globalCounter, int arity);
    void MixedCounter(byte* CounterMixed , byte* splitCounter, byte* globalCounter, int arity); //the reverse process of spliting
    void CounterIncreasement(byte* counters, int counter_index, int arity);

    void XorEngine(byte* src, byte* pad, byte* dst);

    void bitset_2_array(const std::bitset<512> &bits, int n_set_size, byte *buf, int  &n_bytes);
    void array_2_bitset(const byte *buf, int  n_bytes, std::bitset<512> &bits, int  &n_set_size);

public:
    vault(bool DEBUG, int MEM_SIZE);
    vault(bool fake);
    ~vault();
    void writeTrace(unsigned long phy_addr); //simulates the processing of writing operation
    void readTrace(unsigned long phy_addr); //simulates the processing of reading operation

    //gem5 co-design
    

};

/*
class vault_gem5{
    typedef uint64_t Addr;
private:
    int Enclave_start_addr;
    int Enclace_mem_size;

    AES AES_Engine;
    char key[16];
    char PreNode[64*8];


public:
    //generate the pack
    void vaultGenPacket(PacketPtr pkt, Addr addr, int size, bool write);
    //read counter/hash/mt from l2
    void vaultReadL2(Addr addr, char* cache_blk, PacketPtr pkt);
    //encrypt/decrypt the data
    void vaultEncrypt(PacketPtr pkt);
    void vaultDecrypt(PacketPtr pkt);


    
};
*/

}

#endif