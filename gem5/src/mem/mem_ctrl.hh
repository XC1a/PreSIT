/*
 * Copyright (c) 2012-2020 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2013 Amin Farmahini-Farahani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * MemCtrl declaration
 */

#ifndef __MEM_CTRL_HH__
#define __MEM_CTRL_HH__

#include "Enclave/encrypt_lib.hh"
#include <stdlib.h>
#include <time.h>
#include <cmath>

#include <deque>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <queue>

#include "base/callback.hh"
#include "base/statistics.hh"
#include "enums/MemSched.hh"
#include "mem/qos/mem_ctrl.hh"
#include "mem/qport.hh"
#include "params/MemCtrl.hh"
#include "sim/eventq.hh"
//++++++++++++++++++++++++++++wxr define
#define ENC_VALUE 0x1
#define ENC_PROTECT true

//debug info out
#define STATE_DEBUG false

#define ERROR_DEBUG false

#define TRACE_DEBUG false

#define PREDICTION_DEBUG false

#define VAULT_LAST_64 true

#define TURNON_PRE true
#define TURNON_PRE_AES false
#define TURNON_PRE_WRITE false

#define TURNON_PRE_GAP false
#define LINUX_MEM_STR 0x80000000

#define CYCLETICK 500


#define PREDICTION_TAG1 true

#define INPROG true

#define PDA true
#define APA true

#define N_HASH 1
#define HASHDEBUG false
#define EAGER_MODE false
#define HASH_FOR_TREE 5

#define PRE_GAP_NUM 8
#define PRE_GAP_THREHOLD 6

//////////////////////// R1 adjust to memsize
#define COREMEM 162 //////////////// 161 = 16G + 1core 
#define _MEM 16 //////////// to used for generate

extern bool trace_control;
extern uint64_t trace_cnt;
extern uint64_t trace_global_cnt;



  // The K array
  const uint32_t K[64] = {
      0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
      0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
      0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
      0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
      0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
      0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
      0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
      0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
      0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
      0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
      0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
      0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
      0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL};
#define DEBUG_HASH false
#define ATT1 false
#define ATT2 false

#define ONLY_4KB false
//---------------------------wxr
namespace gem5
{

namespace memory
{

class MemInterface;
class DRAMInterface;
class NVMInterface;

/**
 * A burst helper helps organize and manage a packet that is larger than
 * the memory burst size. A system packet that is larger than the burst size
 * is split into multiple packets and all those packets point to
 * a single burst helper such that we know when the whole packet is served.
 */
class BurstHelper
{
  public:

    /** Number of bursts requred for a system packet **/
    const unsigned int burstCount;

    /** Number of bursts serviced so far for a system packet **/
    unsigned int burstsServiced;

    BurstHelper(unsigned int _burstCount)
        : burstCount(_burstCount), burstsServiced(0)
    { }
};

/**
 * A memory packet stores packets along with the timestamp of when
 * the packet entered the queue, and also the decoded address.
 */
class MemPacket
{
  public:

    /** When did request enter the controller */
    const Tick entryTime;

    /** When will request leave the controller */
    Tick readyTime;

    /** This comes from the outside world */
    const PacketPtr pkt;

    /** RequestorID associated with the packet */
    const RequestorID _requestorId;

    const bool read;

    /** Does this packet access DRAM?*/
    const bool dram;

    /** pseudo channel num*/
    const uint8_t pseudoChannel;

    /** Will be populated by address decoder */
    const uint8_t rank;
    const uint8_t bank;
    const uint32_t row;

    /**
     * Bank id is calculated considering banks in all the ranks
     * eg: 2 ranks each with 8 banks, then bankId = 0 --> rank0, bank0 and
     * bankId = 8 --> rank1, bank0
     */
    const uint16_t bankId;

    /**
     * The starting address of the packet.
     * This address could be unaligned to burst size boundaries. The
     * reason is to keep the address offset so we can accurately check
     * incoming read packets with packets in the write queue.
     */
    Addr addr;

    /**
     * The size of this dram packet in bytes
     * It is always equal or smaller than the burst size
     */
    unsigned int size;

    /**
     * A pointer to the BurstHelper if this MemPacket is a split packet
     * If not a split packet (common case), this is set to NULL
     */
    BurstHelper* burstHelper;

    /**
     * QoS value of the encapsulated packet read at queuing time
     */
    uint8_t _qosValue;

    /**
     * Set the packet QoS value
     * (interface compatibility with Packet)
     */
    inline void qosValue(const uint8_t qv) { _qosValue = qv; }

    /**
     * Get the packet QoS value
     * (interface compatibility with Packet)
     */
    inline uint8_t qosValue() const { return _qosValue; }

    /**
     * Get the packet RequestorID
     * (interface compatibility with Packet)
     */
    inline RequestorID requestorId() const { return _requestorId; }

    /**
     * Get the packet size
     * (interface compatibility with Packet)
     */
    inline unsigned int getSize() const { return size; }

    /**
     * Get the packet address
     * (interface compatibility with Packet)
     */
    inline Addr getAddr() const { return addr; }

    /**
     * Return true if its a read packet
     * (interface compatibility with Packet)
     */
    inline bool isRead() const { return read; }

    /**
     * Return true if its a write packet
     * (interface compatibility with Packet)
     */
    inline bool isWrite() const { return !read; }

    /**
     * Return true if its a DRAM access
     */
    inline bool isDram() const { return dram; }

    MemPacket(PacketPtr _pkt, bool is_read, bool is_dram, uint8_t _channel,
               uint8_t _rank, uint8_t _bank, uint32_t _row, uint16_t bank_id,
               Addr _addr, unsigned int _size)
        : entryTime(curTick()), readyTime(curTick()), pkt(_pkt),
          _requestorId(pkt->requestorId()),
          read(is_read), dram(is_dram), pseudoChannel(_channel), rank(_rank),
          bank(_bank), row(_row), bankId(bank_id), addr(_addr), size(_size),
          burstHelper(NULL), _qosValue(_pkt->qosValue())
    { }

};

// The memory packets are store in a multiple dequeue structure,
// based on their QoS priority
typedef std::deque<MemPacket*> MemPacketQueue;


/**
 * The memory controller is a single-channel memory controller capturing
 * the most important timing constraints associated with a
 * contemporary controller. For multi-channel memory systems, the controller
 * is combined with a crossbar model, with the channel address
 * interleaving taking part in the crossbar.
 *
 * As a basic design principle, this controller
 * model is not cycle callable, but instead uses events to: 1) decide
 * when new decisions can be made, 2) when resources become available,
 * 3) when things are to be considered done, and 4) when to send
 * things back. The controller interfaces to media specific interfaces
 * to enable flexible topoloties.
 * Through these simple principles, the model delivers
 * high performance, and lots of flexibility, allowing users to
 * evaluate the system impact of a wide range of memory technologies.
 *
 * For more details, please see Hansson et al, "Simulating DRAM
 * controllers for future system architecture exploration",
 * Proc. ISPASS, 2014. If you use this model as part of your research
 * please cite the paper.
 *
 */
class MemCtrl : public qos::MemCtrl
{
  //wxr++++++++++++++++++++++
  private:

  #if ATT1
  bool receiveAtt1 = false;
  #elif ATT2
  bool receiveAtt2 = false;
  uint8_t ATTdataRecord[64];
  uint8_t ATTMacRecord[8];
  #else
  #endif

  //////////////////////////
  //HMAC R1 +++++++++++++++++++++++++++
  //////////////////////////

  class HMAC{
  public:
  typedef struct {
  uint64_t length;
  uint32_t state[8];
  uint32_t curlen;
  uint8_t buf[64];
  } Sha256Context;

  #define SHA256_HASH_SIZE (256 / 8)

  typedef struct {
    uint8_t bytes[SHA256_HASH_SIZE];
  } SHA256_HASH;

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  WjCryptLib_Sha256
  //
  //  Implementation of SHA256 hash function.
  //  Original author: Tom St Denis, tomstdenis@gmail.com, http://libtom.org
  //  Modified by WaterJuice retaining Public Domain license.
  //
  //  This is free and unencumbered software released into the public domain -
  //  June 2013 waterjuice.org
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  IMPORTS
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  MACROS
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  #define ror(value, bits) (((value) >> (bits)) | ((value) << (32 - (bits))))

  #define MIN(x, y) (((x) < (y)) ? (x) : (y))

  #define STORE32H(x, y)                     \
    {                                        \
      (y)[0] = (uint8_t)(((x) >> 24) & 255); \
      (y)[1] = (uint8_t)(((x) >> 16) & 255); \
      (y)[2] = (uint8_t)(((x) >> 8) & 255);  \
      (y)[3] = (uint8_t)((x)&255);           \
    }

  #define LOAD32H(x, y)                                                         \
    {                                                                           \
      x = ((uint32_t)((y)[0] & 255) << 24) | ((uint32_t)((y)[1] & 255) << 16) | \
          ((uint32_t)((y)[2] & 255) << 8) | ((uint32_t)((y)[3] & 255));         \
    }

  #define STORE64H(x, y)                     \
    {                                        \
      (y)[0] = (uint8_t)(((x) >> 56) & 255); \
      (y)[1] = (uint8_t)(((x) >> 48) & 255); \
      (y)[2] = (uint8_t)(((x) >> 40) & 255); \
      (y)[3] = (uint8_t)(((x) >> 32) & 255); \
      (y)[4] = (uint8_t)(((x) >> 24) & 255); \
      (y)[5] = (uint8_t)(((x) >> 16) & 255); \
      (y)[6] = (uint8_t)(((x) >> 8) & 255);  \
      (y)[7] = (uint8_t)((x)&255);           \
    }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  CONSTANTS
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


  #define BLOCK_SIZE 64

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  INTERNAL FUNCTIONS
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // Various logical functions
  #define Ch(x, y, z) (z ^ (x & (y ^ z)))
  #define Maj(x, y, z) (((x | y) & z) | (x & y))
  #define S(x, n) ror((x), (n))
  #define R(x, n) (((x)&0xFFFFFFFFUL) >> (n))
  #define Sigma0(x) (S(x, 2) ^ S(x, 13) ^ S(x, 22))
  #define Sigma1(x) (S(x, 6) ^ S(x, 11) ^ S(x, 25))
  #define Gamma0(x) (S(x, 7) ^ S(x, 18) ^ R(x, 3))
  #define Gamma1(x) (S(x, 17) ^ S(x, 19) ^ R(x, 10))

  #define Sha256Round(a, b, c, d, e, f, g, h, i)    \
    t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i]; \
    t1 = Sigma0(a) + Maj(a, b, c);                  \
    d += t0;                                        \
    h = t0 + t1;

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  TransformFunction
  //
  //  Compress 512-bits
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  static void TransformFunction(Sha256Context* Context, uint8_t const* Buffer) {
    uint32_t S[8];
    uint32_t W[64];
    uint32_t t0;
    uint32_t t1;
    uint32_t t;
    int i;

    // Copy state into S
    for (i = 0; i < 8; i++) {
      S[i] = Context->state[i];
    }

    // Copy the state into 512-bits into W[0..15]
    for (i = 0; i < 16; i++) {
      LOAD32H(W[i], Buffer + (4 * i));
    }

    // Fill W[16..63]
    for (i = 16; i < 64; i++) {
      W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
    }

    // Compress
    for (i = 0; i < 64; i++) {
      Sha256Round(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i);
      t = S[7];
      S[7] = S[6];
      S[6] = S[5];
      S[5] = S[4];
      S[4] = S[3];
      S[3] = S[2];
      S[2] = S[1];
      S[1] = S[0];
      S[0] = t;
    }

    // Feedback
    for (i = 0; i < 8; i++) {
      Context->state[i] = Context->state[i] + S[i];
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  PUBLIC FUNCTIONS
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  Sha256Initialise
  //
  //  Initialises a SHA256 Context. Use this to initialise/reset a context.
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void Sha256Initialise(Sha256Context* Context  // [out]
  ) {
    Context->curlen = 0;
    Context->length = 0;
    Context->state[0] = 0x6A09E667UL;
    Context->state[1] = 0xBB67AE85UL;
    Context->state[2] = 0x3C6EF372UL;
    Context->state[3] = 0xA54FF53AUL;
    Context->state[4] = 0x510E527FUL;
    Context->state[5] = 0x9B05688CUL;
    Context->state[6] = 0x1F83D9ABUL;
    Context->state[7] = 0x5BE0CD19UL;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  Sha256Update
  //
  //  Adds data to the SHA256 context. This will process the data and update the
  //  internal state of the context. Keep on calling this function until all the
  //  data has been added. Then call Sha256Finalise to calculate the hash.
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void Sha256Update(Sha256Context* Context,  // [in out]
                    void const* Buffer,      // [in]
                    uint32_t BufferSize      // [in]
  ) {
    uint32_t n;

    if (Context->curlen > sizeof(Context->buf)) {
      return;
    }

    while (BufferSize > 0) {
      if (Context->curlen == 0 && BufferSize >= BLOCK_SIZE) {
        TransformFunction(Context, (uint8_t*)Buffer);
        Context->length += BLOCK_SIZE * 8;
        Buffer = (uint8_t*)Buffer + BLOCK_SIZE;
        BufferSize -= BLOCK_SIZE;
      } else {
        n = MIN(BufferSize, (BLOCK_SIZE - Context->curlen));
        memcpy(Context->buf + Context->curlen, Buffer, (size_t)n);
        Context->curlen += n;
        Buffer = (uint8_t*)Buffer + n;
        BufferSize -= n;
        if (Context->curlen == BLOCK_SIZE) {
          TransformFunction(Context, Context->buf);
          Context->length += 8 * BLOCK_SIZE;
          Context->curlen = 0;
        }
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  Sha256Finalise
  //
  //  Performs the final calculation of the hash and returns the digest (32 byte
  //  buffer containing 256bit hash). After calling this, Sha256Initialised must
  //  be used to reuse the context.
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void Sha256Finalise(Sha256Context* Context,  // [in out]
                      SHA256_HASH* Digest      // [out]
  ) {
    int i;

    if (Context->curlen >= sizeof(Context->buf)) {
      return;
    }

    // Increase the length of the message
    Context->length += Context->curlen * 8;

    // Append the '1' bit
    Context->buf[Context->curlen++] = (uint8_t)0x80;

    // if the length is currently above 56 bytes we append zeros
    // then compress.  Then we can fall back to padding zeros and length
    // encoding like normal.
    if (Context->curlen > 56) {
      while (Context->curlen < 64) {
        Context->buf[Context->curlen++] = (uint8_t)0;
      }
      TransformFunction(Context, Context->buf);
      Context->curlen = 0;
    }

    // Pad up to 56 bytes of zeroes
    while (Context->curlen < 56) {
      Context->buf[Context->curlen++] = (uint8_t)0;
    }

    // Store length
    STORE64H(Context->length, Context->buf + 56);
    TransformFunction(Context, Context->buf);

    // Copy output
    for (i = 0; i < 8; i++) {
      STORE32H(Context->state[i], Digest->bytes + (4 * i));
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  Sha256Calculate
  //
  //  Combines Sha256Initialise, Sha256Update, and Sha256Finalise into one
  //  function. Calculates the SHA256 hash of the buffer.
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void Sha256Calculate(void const* Buffer,   // [in]
                      uint32_t BufferSize,  // [in]
                      SHA256_HASH* Digest   // [in]
  ) {
    Sha256Context context;

    Sha256Initialise(&context);
    Sha256Update(&context, Buffer, BufferSize);
    Sha256Finalise(&context, Digest);
  }

  #define SHA256_BLOCK_SIZE 64

  /* LOCAL FUNCTIONS */

  // Concatenate X & Y, return hash.
  // void* H(const void* x,
  //               const size_t xlen,
  //               const void* y,
  //               const size_t ylen,
  //               void* out,
  //               const size_t outlen);

  // Wrapper for sha256
  // static void* sha256(const void* data,
  //                     const size_t datalen,
  //                     void* out,
  //                     const size_t outlen);

  void* sha256(const void* data,
                      const size_t datalen,
                      void* out,
                      const size_t outlen) {
    size_t sz;
    Sha256Context ctx;
    SHA256_HASH hash;

    Sha256Initialise(&ctx);
    Sha256Update(&ctx, data, datalen);
    Sha256Finalise(&ctx, &hash);

    sz = (outlen > SHA256_HASH_SIZE) ? SHA256_HASH_SIZE : outlen;
    return memcpy(out, hash.bytes, sz);
  }


  void* H(const void* x,
                const size_t xlen,
                const void* y,
                const size_t ylen,
                void* out,
                const size_t outlen) {
    void* result;
    size_t buflen = (xlen + ylen);
    uint8_t* buf = (uint8_t*)malloc(buflen);

    memcpy(buf, x, xlen);
    memcpy(buf + xlen, y, ylen);
    result = sha256(buf, buflen, out, outlen);

    free(buf);
    return result;
  }


  // Declared in hmac_sha256.h
  size_t hmac_sha256(const void* key,
                    const size_t keylen,
                    const void* data,
                    const size_t datalen,
                    void* out,
                    const size_t outlen) {
    uint8_t k[SHA256_BLOCK_SIZE];
    uint8_t k_ipad[SHA256_BLOCK_SIZE];
    uint8_t k_opad[SHA256_BLOCK_SIZE];
    uint8_t ihash[SHA256_HASH_SIZE];
    uint8_t ohash[SHA256_HASH_SIZE];
    size_t sz;
    int i;

    memset(k, 0, sizeof(k));
    memset(k_ipad, 0x36, SHA256_BLOCK_SIZE);
    memset(k_opad, 0x5c, SHA256_BLOCK_SIZE);

    if (keylen > SHA256_BLOCK_SIZE) {
      // If the key is larger than the hash algorithm's
      // block size, we must digest it first.
      sha256(key, keylen, k, sizeof(k));
    } else {
      memcpy(k, key, keylen);
    }

    for (i = 0; i < SHA256_BLOCK_SIZE; i++) {
      k_ipad[i] ^= k[i];
      k_opad[i] ^= k[i];
    }

    // Perform HMAC algorithm: ( https://tools.ietf.org/html/rfc2104 )
    //      `H(K XOR opad, H(K XOR ipad, data))`
    H(k_ipad, sizeof(k_ipad), data, datalen, ihash, sizeof(ihash));
    H(k_opad, sizeof(k_opad), ihash, sizeof(ihash), ohash, sizeof(ohash));

    sz = (outlen > SHA256_HASH_SIZE) ? SHA256_HASH_SIZE : outlen;
    memcpy(out, ohash, sz);
    return sz;
  }




  std::string key = "super-secret-key";

  };

  HMAC Hmac;



  ///for check
  bool macCheck=false;
  uint8_t RMAC[64]={0};
  uint64_t MAC_ADDR = 0;
  uint64_t macAddrread=0;


  int Lread = 0; //read how many levels of tree


  bool checkHAMC(bool useforWrite = false)
  {
    //1. we need to check read counter (so L+1 required)
    //2. data mac required to check
    Lread = vault_engine.MT_LEVEL - 2;
    
    // firstly, always check data MAC
    //all 0 no
    bool notAll0=true;
    bool equalMAC = true;
    bool equalTree = true;
    for (size_t i = 0; i < 64; i++)
    {
      notAll0 = notAll0 && ( vault_engine.counterOP[0].blk[i] == 0);
    }

    #if ATT1
    if (!useforWrite && receiveAtt1)
    #elif ATT2
    if (!useforWrite && receiveAtt2)
    #else
    if (!notAll0 && !useforWrite)
    #endif
    {
      if (macCheck)
      {
        //get the correct MAC localtion
        uint8_t nowMAC[8];
        memcpy(nowMAC,RMAC+(MAC_ADDR*8),8);
        bool MACall0=true;
        for (size_t i = 0; i < 8; i++)
        {
          MACall0 = MACall0 && (nowMAC[i] == 0);
        }
        #if ATT1
        std::cout<<"Checking ...  ";
        #elif ATT2
        std::cout<<"Checking ...  ";
        #else
        if (!MACall0)
        #endif
        {
          uint8_t oriCheck[73]; //64+8(global counter)+1(split counter) + 8 dataaddr = 120

          //we need to split the counter to global counter + SC
          uint64_t splitOri[64];
          uint64_t global;
          uint8_t split;
          uint8_t globalArray[64];

          vault_engine.splitCounter(vault_engine.counterOP[0].blk,splitOri,global,64,false);
          assert(splitOri[vault_engine.Counter_modify_list[1]] < 256);
          split = splitOri[vault_engine.Counter_modify_list[1]];

          std::bitset<512> gBS = global;
          int ptr;
          vault_engine.bitset_2_array(gBS,512,globalArray,ptr);
          #if ATT1
            uint8_t attackBuffer[73];
            for (size_t i = 0; i < 64; i++)
            {
              attackBuffer[i] = i;
            }
            memcpy(attackBuffer+64,globalArray,8);
            *(attackBuffer+64+8) = split;
            std::cout<<"ATT1: attack happen! "<<std::endl;
          #elif ATT2
            uint8_t attackBuffer[73];
            memcpy(attackBuffer,ATTdataRecord,64);
            memcpy(attackBuffer+64,globalArray,8);
            *(attackBuffer+64+8) = split;
            std::cout<<"ATT2: attack happen! "<<std::endl;
          #else
            memcpy(oriCheck,readDataRecord,64);
          #endif
          memcpy(oriCheck+64,globalArray,8);
          *(oriCheck+64+8) = split;


          std::string oriToString((char *)oriCheck);
          oriToString = oriToString + std::to_string(ReadDataTransAddrRecord);
          std::vector<uint8_t> out(SHA256_HASH_SIZE);

          Hmac.hmac_sha256(Hmac.key.data(),Hmac.key.size(),oriToString.data(),oriToString.size(),out.data(),out.size());

          #if ATT1
          //in attack 1, we firstly compute the orginal MAC and compare with the now MAC
          std::vector<uint8_t> OriginalMac(SHA256_HASH_SIZE);
          std::string notInMemToString((char *)attackBuffer);
          Hmac.hmac_sha256(Hmac.key.data(),Hmac.key.size(),notInMemToString.data(),notInMemToString.size(),OriginalMac.data(),OriginalMac.size());
          bool ATTcheck = true;
          for (size_t i = 0; i < 8; i++)
          {
            ATTcheck= ATTcheck&& (out[i] == OriginalMac[i]);
          }
          if (!ATTcheck)
          {
            std::cout<<"Regular physical attack detected! "<<std::endl;
          }
          #elif ATT2
          std::vector<uint8_t> OriginalMac(SHA256_HASH_SIZE);
          std::string notInMemToString((char *)attackBuffer);
          Hmac.hmac_sha256(Hmac.key.data(),Hmac.key.size(),notInMemToString.data(),notInMemToString.size(),OriginalMac.data(),OriginalMac.size());
          bool ATTcheck = true;
          for (size_t i = 0; i < 8; i++)
          {
            ATTcheck= ATTcheck&& (ATTMacRecord[i] == OriginalMac[i]);
          }
          if (!ATTcheck)
          {
            std::cout<<"replay attack detected! "<<std::endl;
          }
          //at last, we record the MAC and data
          memcpy(ATTdataRecord,readDataRecord,64);
          memcpy(ATTMacRecord,nowMAC,8);
          #else
          for (size_t i = 0; i < 8; i++)
          {
            equalMAC = equalMAC && (out[i] == nowMAC[i]);
            #if DEBUG_HASH
            std::cout<<std::hex<<int(out[i])<<"  "<<int(nowMAC[i])<<std::dec<<std::endl;
            #endif
          }
          #endif
          #if DEBUG_HASH
          if (!equalMAC)
          {
            std::cout<<" now has reset? "<<EncS.EncReset<<" MAC Loc "<<macAddrread<<" "<<MAC_ADDR<<std::endl;
          }
          #endif
        }
      }
    }
    else
    {
      equalMAC = true;
    }

    //we need to check LRead level
    uint8_t treeToVerify[72]; //56B counter + upper 8B gc + upper 8 (we just treat it as uint64_t)      
    uint8_t nowTreeHash[8];
    // std::string treeToVerifyString;

    for (size_t i = 0; i < Lread; i++)
    {
      bool treeAll0 = true;
      for (size_t j = 0; j < 64; j++)
      {
        treeAll0 = treeAll0 & (vault_engine.counterOP[i].blk[j] == 0);
      }
      
      if (treeAll0)
        continue;
      
      memcpy(treeToVerify,vault_engine.counterOP[i].blk+8,56);
      memcpy(nowTreeHash,vault_engine.counterOP[i].blk,8);
      //to get upper level data
      uint64_t splitOri[64];
      uint64_t global;
      uint64_t scCnt;
      uint8_t splitArray[64];
      uint8_t globalArray[64];

      if ((i+1) == vault_engine.MT_LEVEL-2) //root2
      {
        vault_engine.splitCounter(vault_engine.root2[vault_engine.Counter_modify_list[(i+1)*2]],splitOri,global,16,true);
      }
      else
      {
        vault_engine.splitCounter(vault_engine.counterOP[i+1].blk,splitOri,global,vault_engine.counterOP[i+1].arity,false);
      }

      scCnt = splitOri[vault_engine.Counter_modify_list[(i+1)*2+1]];

      std::bitset<512> gBS = global;
      std::bitset<512> sBS = scCnt;
      int ptr;
      vault_engine.bitset_2_array(gBS,512,globalArray,ptr);
      vault_engine.bitset_2_array(sBS,512,splitArray,ptr);

      memcpy(treeToVerify+56,splitArray,8);
      memcpy(treeToVerify+64,globalArray,8);

      std::string treeToVerifyString((char *)treeToVerify);
      std::vector<uint8_t> out(SHA256_HASH_SIZE);
      
      Hmac.hmac_sha256(Hmac.key.data(),Hmac.key.size(),treeToVerifyString.data(),treeToVerifyString.size(),out.data(),out.size());
      for (size_t j = 0; j < 8; j++)
      {
        equalTree = equalTree && (out[j] == nowTreeHash[j]);
        // #if DEBUG_HASH
        // std::cout<<std::hex<<"Tree Hash "<<i<<" "<<int(out[j])<<"  "<<int(nowTreeHash[j])<<std::dec<<std::endl;

        // #endif
      }
      #if DEBUG_HASH
      if (!equalTree)
      {
        std::cout<<" Print the counter: "<<useforWrite<<" Level: "<<i<<" counterAddr: "<<vault_engine.counterOP[i].addr<<" ";
        std::bitset<512> tmp;
        int ptr;
        vault_engine.array_2_bitset(vault_engine.counterOP[i].blk,64,tmp,ptr);
        std::cout<<tmp<<std::endl;

        for (size_t j = 0; j < 8; j++)
        {
          std::cout<<std::hex<<"Tree Hash "<<i<<" "<<int(out[j])<<"  "<<int(nowTreeHash[j])<<std::dec<<std::endl;
        }
      }
      #endif
    }

    #if DEBUG_HASH
    if (!(equalTree && equalMAC))
      std::cout<<" Both res: "<<equalTree<<"  "<<equalMAC<<std::endl;
    #endif
    
    return equalTree && equalMAC;

  }

  //////////////////////
  //////////////////////
  ///// update tree hash
  /////////////////////
  /////////////////////
  void updateTreeMAC()
  {
    //firstly, we must update data MAC and eCounter's MAC
    //secondly, we need to verify the tree before update it
    //--only read 0 counters,
    
    //writeHashNum is for tree updating hash

    for (size_t i = 0; i < writeHashNum; i++)
    {
      /* code */
      uint8_t treeToCal[72]; //56B counter + upper 8B gc + upper 8 (we just treat it as uint64_t)      

      
      memcpy(treeToCal,vault_engine.counterOP[i].blk+8,56);
      //to get upper level data
      uint64_t splitOri[64];
      uint64_t global;
      uint64_t scCnt;
      uint8_t splitArray[64];
      uint8_t globalArray[64];

      if ((i+1) == vault_engine.MT_LEVEL-2) //root2
      {
        vault_engine.splitCounter(vault_engine.root2[vault_engine.Counter_modify_list[2*(i+1)]],splitOri,global,16,true);
      }
      else
      {
        vault_engine.splitCounter(vault_engine.counterOP[i+1].blk,splitOri,global,vault_engine.counterOP[i+1].arity,false);
      }

      scCnt = splitOri[vault_engine.Counter_modify_list[2*(1+i)+1]];

      std::bitset<512> gBS = global;
      std::bitset<512> sBS = scCnt;
      int ptr;
      vault_engine.bitset_2_array(gBS,512,globalArray,ptr);
      vault_engine.bitset_2_array(sBS,512,splitArray,ptr);

      memcpy(treeToCal+56,splitArray,8);
      memcpy(treeToCal+64,globalArray,8);

      std::string treeToVerifyString((char *)treeToCal);
      std::vector<uint8_t> out(SHA256_HASH_SIZE);
      
      Hmac.hmac_sha256(Hmac.key.data(),Hmac.key.size(),treeToVerifyString.data(),treeToVerifyString.size(),out.data(),out.size());

      for (size_t j = 0; j < 8; j++)
      {
        vault_engine.counterOP[i].blk[j] = out[j];
      }

      #if DEBUG_HASH
        // std::cout<<" update Level: "<<i<<" counterAddr: "<<vault_engine.counterOP[i].addr<<" ";
        // std::bitset<512> tmp;
        // ptr;
        // vault_engine.array_2_bitset(vault_engine.counterOP[i].blk,64,tmp,ptr);
        // std::cout<<tmp<<std::endl;
      #endif
      
    }
  }

  uint64_t writeDataRecord[64];
  bool updateDataMAC(uint64_t macAddr, int MAC_loc, uint8_t* MemV)
  {
    assert(MAC_loc < 8);
    uint8_t oriCheck[73]; //64+8(global counter)+1(split counter) = 120

    //we need to split the counter to global counter + SC
    uint64_t splitOri[64];
    uint64_t global;
    uint8_t split;
    uint8_t globalArray[64];

    vault_engine.splitCounter(vault_engine.counterOP[0].blk,splitOri,global,64,false);
    assert(splitOri[vault_engine.Counter_modify_list[1]] < 256);
    split = splitOri[vault_engine.Counter_modify_list[1]];

    std::bitset<512> gBS = global;
    int ptr;
    vault_engine.bitset_2_array(gBS,512,globalArray,ptr);

    memcpy(oriCheck,writeDataRecord,64);
    memcpy(oriCheck+64,globalArray,8);
    *(oriCheck+64+8) = split;


    std::string oriToString((char *)oriCheck);
    oriToString = oriToString + std::to_string(WriteDataTransAddrRecord);
    std::vector<uint8_t> out(SHA256_HASH_SIZE);

    ////judge cache hit
    bool sawHit = macCache.MacSeeHit(macAddr);
    uint8_t newHeld[64]={};
    if (!sawHit)
      memcpy(newHeld,MemV,64);
    

    Hmac.hmac_sha256(Hmac.key.data(),Hmac.key.size(),oriToString.data(),oriToString.size(),out.data(),out.size());
    uint8_t res_hold[64]={0};
    std::bitset<64> byetE = 0xff;
    byetE = byetE<<(MAC_loc*8);
    for (size_t i = 0; i < 8; i++)
    {
      res_hold[i+(MAC_loc*8)] = out[i];
      if (!sawHit)
        newHeld[i+(MAC_loc*8)] = out[i];
    }

    #if DEBUG_HASH
      std::cout<<" update MAC Loc "<<macAddr<<" "<<MAC_loc<<" ";
      for (size_t i = 0; i < 8; i++)
      {
        std::cout<<std::hex<<int(res_hold[MAC_loc*8 + i])<<" ";
      }
      std::cout<<std::dec<<std::endl;
    #endif
    if (sawHit)
      return macCache.EncWrite(macAddr,res_hold,byetE);
    else
      return macCache.EncWrite(macAddr,newHeld,byetE);
  }

  //////////////////////////
  //HMAC R1 +++++++++++++++++++++++++++
  //////////////////////////

  //////////////////////
  //debug countrol
  //////////////////////
  void EncStateMachine();
  bool EncRetry = false;

  void hashProcess();
  /********
  here we define the states of the memory integrity checking
  ********/
  int stateEnc=0;
  bool WriteIf = false;
  std::vector<Addr> splitOriPkt; //a pkt may contain more than one blocks
  int splitOriPkt_cnt; //record the process
  std::vector<std::vector<bool>> cntRwTrace; //a trace of counter *HIT/MISS* and *EVICTED*
  bool pre_issue=false; //this is used to keep the state to nextEvent
  int GlobalThreadID; //record the thread id


  //3 record list to handle
  ////////////////////////////////
  //
  //  At this time it should again try to call sendTiming(); however the packet may again be rejected. 
  //  Note: The original packet does not need to be resent, a higher priority packet can be sent instead.
  //
  /////////////////////////////////
  class info
  {
    public:
    Addr addr;
    int size;
    bool writeIf;

    info(Addr _addr, int _size, bool _writeIf){ 
      addr=_addr;
      size=_size;
      writeIf=_writeIf;
    }

    bool operator==(info& in)
    {
      return (in.addr == this->addr) && (in.size==this->size) && (in.writeIf==this->writeIf);
    }

  };


  struct stackPkt{
    Addr addr;
    bool writeIf;
    int size;

    void print()
    {
      std::cout<<"==>>>> pkt "<<addr<<" "<<size<<" "<<writeIf<<std::endl;
    }
    
  };

  class filterPkt
  {
    public:
    stackPkt Spkt;
    bool verified;

    filterPkt(PacketPtr _pkt, bool _verified)
    {
      Spkt.addr = _pkt->getAddr();
      Spkt.size = _pkt->getSize();
      Spkt.writeIf = _pkt->isWrite();
      verified = _verified;
    }
  };
  
  class infoRecv
  {
    public:

    std::list<filterPkt> filterPktList;


    void updatePkt(PacketPtr pkt)
    {
      auto it = filterPktList.end();
      while(it != filterPktList.begin())
      {
        --it;
        if (it->Spkt.addr == pkt->getAddr() && it->Spkt.size == pkt->getSize())
        {
          return;
        }
      }
      filterPktList.emplace_back(pkt,false);
    }

    void updatePkt(stackPkt pkt)
    {
      auto it = filterPktList.end();
      while(it != filterPktList.begin())
      {
        --it;
        if (it->Spkt.addr == pkt.addr && it->Spkt.size == pkt.size)
        {
          it->verified = true;
          return;
        }
      }
    }

    void prunePkt()
    {
      if (filterPktList.size() >= 8)
      {
        filterPktList.pop_front();
        filterPktList.pop_front();
      }
    }

    stackPkt chooseNext()
    {
      auto it = filterPktList.end();
      while(it != filterPktList.begin())
      {
        --it;
        if (!it->verified)
        {
          return it->Spkt;
        }
      }
      stackPkt nullPkt{0,0,0};
      return nullPkt;
    }
  };
  
  infoRecv infoR;


  Cycles EncPreC;
  enum StatesEnc{
      IDLE, //empty state, waiting for a new request
      INIT, //to initialize the counter and hash, note the MAC is fake in this version
      POLLING,  //waiting for req
      READY,   //ready to protect when receive flag
      SPLITAG,   //split packet again
      RDCNT,   //the state to read the counter
      CNTOP,   //the state of operating the counters (add / rehash) 
      CNTSANITY, //check the counter sanity
      WBLIMB, //write back the single limb
      WUPMAC, //write req updates the MACs
      RRDMAC, //read req read the MACs
      RRDMAC_WB, //read req read the MACs
      RRDMAC_REFILL, //read req read the MACs
      RRDMAC_RESP, //wait for resp
      DORESETTREE, //reset operations (main for re-hash, arity times of read and write), has son states
      DATATRAN, //original data transit
      RDONE, //read sistutation done
      WDONE, 
      TESTING,  //testing 

      //***** prediction state
      PREDICTION_PREFETCH_DECIDE,
      PREDICTION_PREFETCH_DO,
      PREDICTION_PREFETCH_WAIT,
      PREDICTION_CAL,
      PREDICTION_DONE,

      PREDICTION_GAP_PREFETCH,
      PREDICTION_GAP_PREFETCH_DO,
      PREDICTION_GAP_PREFETCH_WAIT,
      PREDICTION_GAP_CAL,
      PREDICTION_GAP_DONE,
  };

  int stateRSTEnc=0;
  enum RstState{
    Pre,
    MACDATA,
    TREErdEncCache,
    TREErdMem,
    TREEWB,
    WaitResp,
    DATA_HBUF,
    DATA_R,
    DATA_W_R,
    DATA_W, //read/write the data directly from memory and encryption and decryption
  };

  //********************************RDCNT state data stucture
  std::vector<PacketPtr> RDCNTwRpkt;
  int RDCNTwRpkt_cnt;
  std::vector<PacketPtr> RDCNTrDpkt;
  int RDCNTrDpkt_cnt;
  bool RDCNTneedRD;
  bool RDCNTneedWB;

  int RDCNTrecvCNT;

  //********************************WBLIMB state data stucture
  std::vector<PacketPtr> WBLIMBwRpkt;
  int WBLIMBwRpkt_cnt;
  bool WBLIMBneedWB;

  //********************************DORESETTREE state data stucture
  std::vector<PacketPtr> DORESETTREEwRpkt;
  std::vector<PacketPtr> DORESETTREErDpkt;
  int DORESETTREErDpkt_cnt;
  int DORESETTREEwRpkt_cnt;
  int DORESETTREEdata_cnt;
  int DORESETTREEdata_recv;
  
  /********
  the packet management:
  create and delete the packet
  ********/
  void TestOne();

  void EncGenPacket(PacketPtr &pkt, RequestPtr &req, Addr addr, unsigned size, uint8_t* data, bool write);
  bool EncAccessPacket(PacketPtr pkt);
  std::vector<PacketPtr> EncPktPool;
  void poolDelete();

  int getWQueueVacancy();
  int getRQueueVacancy();
  #if COREMEM==161
  Addr ENCLAVE_STARTADDR = 0x3d077a000;
  uint64_t ENCLAVE_SIZE=0xA0000000; // 16GB
  #elif COREMEM==164
  Addr ENCLAVE_STARTADDR = 0x3d0749000;
  uint64_t ENCLAVE_SIZE=0xA0000000; // 16GB
  #elif COREMEM==162
  Addr ENCLAVE_STARTADDR = 0x3d076a000;
  uint64_t ENCLAVE_SIZE=0xA0000000; // 16GB
  #elif COREMEM==81
  Addr ENCLAVE_STARTADDR = 0x228382000;
  uint64_t ENCLAVE_SIZE=0x50000000; // 16GB
  #elif COREMEM==321
  Addr ENCLAVE_STARTADDR = 0x720f69000;
  uint64_t ENCLAVE_SIZE=0x140000000; // 16GB
  #endif

  uint64_t MemSize = (uint64_t)1024*1024*1024*_MEM;
  uint64_t ENCALVE_MASK = (1 << uint64_t(log2(MemSize))) - 1;

  /*******
  Initialization vars
  *******/
  int init_p = 0; //record the initial times
  PacketPtr pkt_encBuild;
  bool CounterInit();

  /*******
  the function of vault to some fundalmental operations
  *******/
  uint8_t Enclave_key[16] = {0};
  void EncDeEncrypt(PacketPtr pkt, uint8_t* seed);

  /*
  the EncCache for caching the counter hash
  designed with parameters to flexibly evaluate
  */
  struct EncCacheBlk{
    uint8_t blk[64]={0};
    bool dirty=0;
    bool valid=0;
    uint8_t use_flag=0;
    uint64_t tag=0;
    uint64_t addr=0; //full address
    uint64_t access_cycle=0;
    
    int CalLfuPriority()
    {
      int sum(0);
      for (size_t i = 0; i < 8; i++)
      {
        int tmp = use_flag & 0x1;
        sum += tmp;
        use_flag = use_flag>> 1;
      }
      return sum;
    }

    void updataCycle(uint64_t new_cycle)
    {
      access_cycle = new_cycle;
    }

  };

  //status setup (hash encrytion cycle)
  #define hashC 80
  #define encryptC 80
  #define cacheC 2
  //Enclave status
  struct EncOpStatus
  {

      //exception situtation record
      uint64_t EncPreNotEqual = 0;
      

      //basic status
      uint64_t EncReadReq = 0;
      uint64_t EncWriteReq = 0;
      uint64_t EncTotGap= 0;

      uint64_t EncEncryptTime = 0;
      uint64_t EncDecryptTime = 0;
      uint64_t EncHasUpdata = 0;
      uint64_t EncCacheAccessTime=0;

      uint64_t EncHashOverlapRDMAC= 0;

      uint64_t EncBeginCycle = 0;
      uint64_t EncEndCycle = 0;

      //enc cache status
      uint64_t EncCacheRd=0;
      uint64_t EncCacheWr=0;
      uint64_t EncCacheRd_hit=0;
      uint64_t EncCacheWr_hit=0;

      //mac cache status
      uint64_t MacCacheRd=0;
      uint64_t MacCacheWr=0;
      uint64_t MacCacheRd_hit=0;
      uint64_t MacCacheWr_hit=0;

      //different cycles
      uint64_t EncRDCNTC = 0;
      uint64_t EncWBLIMB = 0;

      uint64_t EncReset = 0;
      uint64_t EncReset_cycle = 0;
      uint64_t EncReset_Mem_RdTime = 0;
      uint64_t EncReset_Mem_WrTime = 0;
      uint64_t EncReset_Mem_HashTime = 0;
      uint64_t EncReset_Mem_EncryptTime = 0;
      uint64_t EncReset_Mem_DecryptTime = 0;
      uint64_t EncLimbUpdata = 0;

      //////////////////////
      //
      //consider the parallel proccessing, the bias will be added
      //
      //////////////////////
      uint64_t EncReadAesBias = 0;
      uint64_t EncPrefetchHashOverlap = 0;

      //prediction status
      uint64_t EncPredictionSuccess = 0;
      uint64_t EncPredictionRecvNew = 0;
      uint64_t EncPredictionSaveAes = 0;
      uint64_t EncPredictionAesSuccess = 0;

      uint64_t EncPredictionNorAesSuccess = 0;
      uint64_t EncPredictionSuccessTree= 0;
      uint64_t EncPrefetchWaitPort= 0;
        //tis is for write prediction
      uint64_t EncWritePredictionNoneed = 0;
      uint64_t EncPredictionWriteSuccess = 0;
      uint64_t EncWritePredictionBias= 0;

      uint64_t EncHashHardwareWaitTime = 0;


      //counte reset situation
      uint64_t EncResetAesBias = 0;
      uint64_t EncResetHashBias = 0;

      //hash group status
      uint64_t EncHashGroupNum= 0;
      uint64_t EncHashGroupBusyNum= 0;
      uint64_t EncHashGroupBusyMax= 0;
      
      uint64_t EncPreWaitCaled = 0;
      uint64_t EncPrefetchTime= 0;
      uint64_t EncPrefetchCycle= 0;
      
      uint64_t EncPrefetchGapPreTime= 0;

      void print()
      {
        std::cout<<" ______     __   __     ______     __         ______     __   __   ______    "<<std::endl;
        std::cout<<"/\\  ___\\   /\\ \"-.\\ \\   /\\  ___\\   /\\ \\       /\\  __ \\   /\\ \\ / /  /\\  ___\\   "<<std::endl;
        std::cout<<"\\ \\  __\\   \\ \\ \\-.  \\  \\ \\ \\____  \\ \\ \\____  \\ \\  __ \\  \\ \\ \\//   \\ \\  __\\   "<<std::endl;
        std::cout<<" \\ \\_____\\  \\ \\_\\ \"\\_\\  \\ \\_____\\  \\ \\_____\\  \\ \\_\\ \\_\\  \\ \\__|    \\ \\_____\\ "<<std::endl;
        std::cout<<"  \\/_____/   \\/_/ \\/_/   \\/_____/   \\/_____/   \\/_/\\/_/   \\/_/      \\/_____/ "<<std::endl;
        std::cout<<"                                                                             "<<std::endl;

        uint64_t totalTime = EncEndCycle-EncBeginCycle + encryptC*(EncEncryptTime+EncDecryptTime) + EncHasUpdata*hashC + EncCacheAccessTime*cacheC + EncResetAesBias + EncResetHashBias;

        std::cout<<"<<<< 1  >>>> Total time exclude encrypt/hash. Result: "<<std::dec<<EncEndCycle-EncBeginCycle<<std::endl;
        std::cout<<"<<<< 2  >>>> Time of encrypt. Result: "<<EncEncryptTime*encryptC<<std::endl;
        std::cout<<"<<<< 3  >>>> Time of decrypt. Result: "<<EncDecryptTime*encryptC<<std::endl;
        std::cout<<"<<<< 4  >>>> Time of hash. Result: "<<EncHasUpdata*hashC<<std::endl;
        std::cout<<"<<<< 5  >>>> Time of cache access. Result: "<<EncCacheAccessTime*cacheC<<std::endl;
        std::cout<<"<<<< 6  >>>> Total time. Result: "<<totalTime<<std::endl;
        std::cout<<"<<<< 7  >>>> Read Req. Result: "<<EncReadReq<<std::endl;
        std::cout<<"<<<< 8  >>>> Write Req. Result: "<<EncWriteReq<<std::endl;
        std::cout<<"<<<< 9  >>>> Warnning. Pre not Requal Req. Result: "<<EncPreNotEqual<<std::endl;
        std::cout<<"<<<< 10 >>>> Counter Reset. Result: "<<EncReset<<std::endl;

        std::cout<<"<<<< Reset.1 >>>> Counter Reset read. Result: "<<EncReset_Mem_RdTime<<std::endl;
        std::cout<<"<<<< Reset.2 >>>> Counter Reset write. Result: "<<EncReset_Mem_WrTime<<std::endl;
        std::cout<<"<<<< Reset.3 >>>> Counter Reset hashTime. Result: "<<EncReset_Mem_HashTime*hashC<<std::endl;
        std::cout<<"<<<< Reset.4 >>>> Counter Reset encryption. Result: "<<EncReset_Mem_EncryptTime*encryptC<<std::endl;
        std::cout<<"<<<< Reset.5 >>>> Counter Reset decryption. Result: "<<EncReset_Mem_DecryptTime*encryptC<<std::endl;
        std::cout<<"<<<< Reset.6 >>>> Counter Reset cryption Bias. Result: "<<EncResetAesBias<<std::endl;
        std::cout<<"<<<< Reset.7 >>>> Counter Reset Hash Bias. Result: "<<EncResetHashBias<<std::endl;
        std::cout<<std::endl;
                                                                          
        std::cout<<"EncCache <<<< 1 >>>> Read. Result: "<<EncCacheRd<<std::endl;
        std::cout<<"EncCache <<<< 2 >>>> Read hit. Result: "<<EncCacheRd_hit<<std::endl;
        std::cout<<"EncCache <<<< 3 >>>> Write. Result: "<<EncCacheWr<<std::endl;
        std::cout<<"EncCache <<<< 4 >>>> Write hit. Result: "<<EncCacheWr_hit<<std::endl;
        std::cout<<"EncCache <<<< 5 >>>> hit rate. Result: "<<float(EncCacheRd_hit+EncCacheWr_hit)/float(EncCacheRd+EncCacheWr)<<std::endl;
        std::cout<<std::endl;

        std::cout<<"MacCache <<<< 1 >>>> Read. Result: "<<MacCacheRd<<std::endl;
        std::cout<<"MacCache <<<< 2 >>>> Read hit. Result: "<<MacCacheRd_hit<<std::endl;
        std::cout<<"MacCache <<<< 3 >>>> Write. Result: "<<MacCacheWr<<std::endl;
        std::cout<<"MacCache <<<< 4 >>>> Write hit. Result: "<<MacCacheWr_hit<<std::endl;
        std::cout<<"MacCache <<<< 5 >>>> hit rate. Result: "<<float(MacCacheRd_hit+MacCacheWr_hit)/float(MacCacheRd+MacCacheWr)<<std::endl;
        std::cout<<std::endl;

        std::cout<<"Prediction <<<< 1 >>>> Success. Result: "<<EncPredictionSuccess<<std::endl;
        std::cout<<"Prediction <<<< 2 >>>> RecvNew During Prediction. Result: "<<EncPredictionRecvNew<<std::endl;
        std::cout<<"Prediction <<<< 3 >>>> Read AES Bias. Result: "<<EncReadAesBias<<std::endl;
        std::cout<<"Prediction <<<< 4 >>>> Hash Bias. Result: "<<EncPrefetchHashOverlap<<std::endl;
        std::cout<<"Prediction <<<< 5 >>>> Prediction save AES time. Result: "<<EncPredictionSaveAes<<std::endl;
        std::cout<<"Prediction <<<< 6 >>>> Prediction wait for hash hardware. Result: "<<EncHashHardwareWaitTime<<std::endl;
        std::cout<<"Prediction <<<< 7 >>>> write prediction success. Result: "<<EncPredictionWriteSuccess<<std::endl;
        std::cout<<"Prediction <<<< 8 >>>> write prediction bias success. Result: "<<EncWritePredictionBias<<std::endl;
        std::cout<<"Prediction <<<< 9 >>>> Hash overlap ReadMac. Result: "<<EncHashOverlapRDMAC<<std::endl;
        std::cout<<"Prediction <<<<10  >>>> Memory Access GAP. Result: "<<double(EncTotGap)/(EncReadReq+EncWriteReq)<<std::endl;
      }
  };

  EncOpStatus EncS;
  

  class EncCache final{
    private:
      // except the special illustrution. the unit is uint8_t
      int CACHELINE = 64;
      int TOTAL_SIZE = 32*1024;
      int NWAY = 1;
      int REPLACE_POLICY = 0;

      int CACHELINE_NUM = TOTAL_SIZE/CACHELINE;
      int CACHELINE_NUM_WAY = CACHELINE_NUM/NWAY;

      uint64_t TagMask;
      uint64_t OffMask;
      uint64_t InxMask;

      enum RP{
        LRU,
        LFU,
        FIFO,
        RANDOM
      };

      uint64_t mem_cycle = 0;

    public:
      //the state of EncClave logic
      struct EncStatus
      {
        bool nowWRhit=0;
        bool nowRDhit=0;
        uint64_t tWRhit=0;
        uint64_t tRDhit=0;
        uint64_t tWR=0;
        uint64_t tRD=0;

        uint64_t EncCacheAccTime = 0;
      };

      //the cache content
      EncCacheBlk* EncCacheBlk_ptr;
      EncStatus EncS;


      EncCacheBlk Evicted_ptr;
      std::vector<EncCacheBlk*> RdMiss_ptr;
      std::vector<Addr> RdMissAddr;
      EncCacheBlk* RdMiss;

      #if COREMEM==161
      Addr ENCLAVE_STARTADDR = 0x3d077a000;
      uint64_t ENCLAVE_SIZE=0xA0000000; // 16GB
      #elif COREMEM==164
      Addr ENCLAVE_STARTADDR = 0x3d0749000;
      uint64_t ENCLAVE_SIZE=0xA0000000; // 16GB
      #elif COREMEM==162
      Addr ENCLAVE_STARTADDR = 0x3d076a000;
      uint64_t ENCLAVE_SIZE=0xA0000000; // 16GB
      #elif COREMEM == 81
      Addr ENCLAVE_STARTADDR = 0x228382000;
      uint64_t ENCLAVE_SIZE=0x50000000; // 8GB
      #elif COREMEM == 321
      Addr ENCLAVE_STARTADDR = 0x720f69000;
      uint64_t ENCLAVE_SIZE=0x140000000; // 8GB
      #endif
      /*
      return true: no evicted
      false: need write back the dirty block
      */
      bool EncWrite(Addr addr, uint8_t* data, std::bitset<64> byteEnable = 0xffffffffffffffff)
      {
        //**************************************************comment by xrwang
        //Enclave write adopts write-allocate and write-back by default.
        //When a write select a blk, write-hit means no matter what is the dirty bit 
        //is, the memory write can be avoided. As for write-miss, the dirty bit
        //decides whether to update the memory.
        if (addr<ENCLAVE_STARTADDR || addr>(ENCLAVE_STARTADDR+ENCLAVE_SIZE))
        {
          std::cout<<"EncCache illegal write addr: "<<std::hex<<addr<<std::dec<<std::endl; 
          exit(0);
        }
        

        mem_cycle ++;
        uint64_t TagAddr = addr & TagMask;

        EncS.nowRDhit = EncS.nowWRhit = false;
        EncS.tWR ++;
        EncS.EncCacheAccTime ++;

        //by default, write allocate
        uint64_t InxAddr = ((addr & InxMask) >> 6) / NWAY;
        uint64_t cycle_tmp = 0;
        int max_id = InxAddr;
        int hit_id = -1;
        for (size_t j = 0; j < NWAY; j++)
        {
          uint64_t acycle = EncCacheBlk_ptr[InxAddr + j*CACHELINE_NUM_WAY].access_cycle;
          if ((EncCacheBlk_ptr+InxAddr + j*CACHELINE_NUM_WAY)->tag == TagAddr)
          {
            hit_id = InxAddr + j*CACHELINE_NUM_WAY;
          }
          
          cycle_tmp = acycle > cycle_tmp ? acycle : cycle_tmp;
          max_id = acycle > cycle_tmp ? InxAddr + j*CACHELINE_NUM_WAY : max_id;
        }

        EncCacheBlk* selectBlk = EncCacheBlk_ptr+ (hit_id!=-1 ? hit_id : max_id);
        bool return_vuale=false;
        if (REPLACE_POLICY == RP::LRU)
        {
          if (hit_id != -1)
          {
            EncS.nowWRhit = true;
            EncS.tWRhit ++;
            return_vuale = false;
          }
          else{
            if (selectBlk->dirty)
            {
              memcpy(Evicted_ptr.blk,selectBlk->blk,64);
              Evicted_ptr.tag = selectBlk->tag;
              Evicted_ptr.addr = selectBlk->addr;
              return_vuale = true;
            }
            else{
              return_vuale = false;
            }
          }

          selectBlk->dirty = true;
          selectBlk->tag = TagAddr;
          selectBlk->addr = addr;
          selectBlk->valid = true;
          // memcpy(selectBlk->blk,data,64);
          ////////////////////////////R1
          ///////////////// add byteEnable to store MAC.
          for (size_t i = 0; i < 64; i++)
          {
            if (byteEnable[i] == 1)
            {
              selectBlk->blk[i] = data[i];
            }
          }
          
          selectBlk->updataCycle(mem_cycle);
        }
        return return_vuale;
      }
      
      bool MacSeeHit(Addr addr)
      { 
        uint64_t TagAddr = addr & TagMask;
        uint64_t InxAddr = ((addr & InxMask) >> 6) / NWAY;
        uint64_t cycle_tmp = 0;
        int max_id = InxAddr;
        int hit_id = -1;
        for (size_t j = 0; j < NWAY; j++)
        {
          uint64_t acycle = EncCacheBlk_ptr[InxAddr + j*CACHELINE_NUM_WAY].access_cycle;
          if ((EncCacheBlk_ptr+InxAddr + j*CACHELINE_NUM_WAY)->tag == TagAddr)
          {
            hit_id = InxAddr + j*CACHELINE_NUM_WAY;
          }
          
          cycle_tmp = acycle > cycle_tmp ? acycle : cycle_tmp;
          max_id = acycle > cycle_tmp ? InxAddr + j*CACHELINE_NUM_WAY : max_id;
        }
        return hit_id != -1;

      }

      bool EncRead(Addr addr, uint8_t* data)
      {
        //*************************************comment by xrwang
        //Read miss could bring a new memory read.
        //A miss + dirty brings a evict and refill
        if (addr<ENCLAVE_STARTADDR || addr>(ENCLAVE_STARTADDR+ENCLAVE_SIZE))
        {
          std::cout<<"EncCache illegal read addr: "<<std::hex<<addr<<std::dec<<std::endl; 
          exit(0);
        }
        

        mem_cycle ++;
        uint64_t TagAddr = addr & TagMask;

        EncS.nowRDhit = EncS.nowWRhit = false;
        EncS.tRD ++;
        EncS.EncCacheAccTime ++;
        
        uint64_t InxAddr = ((addr & InxMask) >> 6) / NWAY;
        uint64_t cycle_tmp = 0;
        int max_id = InxAddr;
        int hit_id = -1;
        for (size_t j = 0; j < NWAY; j++)
        {
          uint64_t acycle = EncCacheBlk_ptr[InxAddr + j*CACHELINE_NUM_WAY].access_cycle;
          if ((EncCacheBlk_ptr+InxAddr + j*CACHELINE_NUM_WAY)->tag == TagAddr)
          {
            hit_id = InxAddr + j*CACHELINE_NUM_WAY;
          }
          cycle_tmp = acycle > cycle_tmp ? acycle : cycle_tmp;
          max_id = acycle > cycle_tmp ? InxAddr + j*CACHELINE_NUM_WAY : max_id;
        }

        EncCacheBlk* selectBlk = EncCacheBlk_ptr+ (hit_id!=-1 ? hit_id : max_id);
        bool return_value=false;
        
        
        if (REPLACE_POLICY == RP::LRU)
        {
          if (hit_id != -1)
          {
            EncS.nowRDhit = true;
            EncS.tRDhit ++;
            memcpy(data,selectBlk->blk,64);
          }
          else
          {
            RdMiss = selectBlk;
            if (selectBlk->dirty)
            {
              return_value = true;
              memcpy(Evicted_ptr.blk,selectBlk->blk,64);
              Evicted_ptr.tag = selectBlk->tag;
              Evicted_ptr.addr = selectBlk->addr;
            }
            else
            {
              return_value = false;
            }
            
          }
          selectBlk->updataCycle(mem_cycle);
        }

        return return_value;

      }

      //return index of the vector hold
      int EncRefill(Addr addr,uint8_t* data)
      {
        if (addr<ENCLAVE_STARTADDR || addr>(ENCLAVE_STARTADDR+ENCLAVE_SIZE))
        {
          std::cout<<"EncCache illegal refill addr: "<<std::hex<<addr<<std::dec<<std::endl; 
          exit(0);
        }
        
        int index = -1;
        for (size_t i = 0; i < RdMissAddr.size(); i++)
        {
          if (addr == RdMissAddr[i])
          {
            index = i;
          }
        }
        if (index == -1)
        {
          std::cerr<<"<<<<==== EncCache miss record fault ====>>>>"<<std::endl;
          exit(0);
        }

        memcpy(RdMiss_ptr[index]->blk,data,64);
        RdMiss_ptr[index]->tag = addr & TagMask;
        RdMiss_ptr[index]->addr = addr;
        RdMiss_ptr[index]->dirty = false;
        return index;
      }

      void EncReflush()
      {
        for (size_t i = 0; i < TOTAL_SIZE; i++)
        {
          (EncCacheBlk_ptr+i)->tag = 0;
          (EncCacheBlk_ptr+i)->dirty = 0;
          (EncCacheBlk_ptr+i)->valid = 0;
          (EncCacheBlk_ptr+i)->use_flag  = 0;
          (EncCacheBlk_ptr+i)->access_cycle = 0;
          memset((EncCacheBlk_ptr+i)->blk,0,64);
        }
      }
      
      std::map<std::string,int> init_map {{"nway",1}, {"size",32*1024}};
      EncCache(std::map<std::string,int> init_map = {{"nway",1}, {"size",32*1024}})
      {
        std::cout<<"<<<==== EncCache TagMask is initializing ====>>>>"<<std::endl;
        //receive the parameter initialization
        for (auto param : init_map)
        {
          if (param.first == "nway")
          {
            NWAY = param.second;
          }
          else if (param.first == "size")
          {
            TOTAL_SIZE = param.second;
          }
          else if (param.first == "policy")
          {
            REPLACE_POLICY = param.second;
          }
        }
        CACHELINE_NUM = TOTAL_SIZE/CACHELINE;
        CACHELINE_NUM_WAY = CACHELINE_NUM/NWAY;

        EncCacheBlk_ptr = new EncCacheBlk[TOTAL_SIZE];
        // Evicted_ptr = new EncCacheBlk;
        int width = int(log2(TOTAL_SIZE));
        TagMask = 0xffffffffffffffff - ((1<<(width+6)) - 1) ;
        OffMask = (1<<6) - 1;
        InxMask = (1<<width) - 1;
        std::cout<<"<<<==== EncCache TagMask is "<<std::hex<<TagMask<<std::dec<<" ====>>>>"<<std::endl;
        std::cout<<"<<<==== EncCache OffMask is "<<std::hex<<OffMask<<std::dec<<" ====>>>>"<<std::endl;
        std::cout<<"<<<==== EncCache InxMask is "<<std::hex<<InxMask<<std::dec<<" ====>>>>"<<std::endl;

      }

      ~EncCache(){
        delete [] EncCacheBlk_ptr;
        // delete Evicted_ptr;
      }

  };

  //this is used for tree cache
  EncCache encCache;
  //this is used for MAC cache
  EncCache macCache;

  /********
  the class to handle some basic functions
  ********/
 
  //a limb of the tree
  struct vault_blk
  {
    uint8_t blk[64]={0};
    Addr addr=0; //whole addr
    int off=0; //the split counter
    int arity=0; //hold the arity
  };


  class vault{
  public:
      //********************** MACOR setting of vault
      bool VAULT_DEBUG = false; //open the debug to print some message 
      bool VAULT_SIMULATE = false; //to simulate the vault
      int MEM_SIZE = 4096; //the unit is MB
      int BLK_SIZE = 64; //the unit is B, the size is same with cacheline
      int MT_LEVEL; // the last level is counter. this encrypted by above counters(not protected by hash) 
      unsigned long ENCLAVE_STR_ADDR; //this addr represents the phy addr of the MT+Counter+MACs, initialized by constructor
      bool VAULT_COMP = false; //open the compression indicates 7-b counter needs to be  decremented to 6-b

      int COUNTER_SIZE = 0;

      int BIT_COUNTER_LEN =(VAULT_LAST_64 || VAULT_COMP) ? 6 : 7;
      int BIT_L2_LEN = 12;
      int BIT_LH_LEN = 24;
      //********************** Counter and MT
      std::vector<int> mt_level_entry_num;
      std::vector<int> Counter_modify_list;

  public:
      //root of tree
      uint8_t root[64]={0};
      uint8_t root2[16][64]={{0}}; //l2
      /*
      *  Size is return as unit Mega
      */
      int getEncSize()
      {
        return MEM_SIZE;
      }

      int getMacSize()
      {
        return MEM_SIZE/8;
      }
      
      int getCounterSize()
      {
        return COUNTER_SIZE;
      }


      vault_blk* counterOP;
      std::vector<bool> resetRecord; //to record different level reset
      

      void bitset_2_array(const std::bitset<512> &bits, int n_set_size, uint8_t *buf, int  &n_bytes)
      {
          n_bytes = 0;
          for (int i = 0; i < n_set_size; i += 8)
          {
              uint8_t ch=0;
              for (int j = 0; j < 8; ++j)
              {
                  if (bits.test(i + j))
                      ch |= (1 << j);
                  else
                      ch &= ~(1 << j);
              }
              buf[n_bytes++] = ch;
          }
      }

      void array_2_bitset(const uint8_t *buf, int  n_bytes, std::bitset<512> &bits, int  &n_set_size)
      {
          n_set_size = 0;
          // int n_bits = n_bytes * 8;
          for (int i = 0; i < n_bytes; ++i)
          {
              uint8_t ch = buf[i];
              int n_offset = i * 8;
              for (int j = 0; j < 8; ++j)
              {
                  bits.set(n_offset + j, ch & (1 << j));
                  ++n_set_size;
              }
          }
      }

      ////////////////////////
      //
      //16 GB cannot fully statisfy arity
      //
      ////////////////////////
      void getMTLevel()
      {
          int counter_num = (MEM_SIZE * 1024 * 1024  ) / 64 / BLK_SIZE;
          int counter_byte = counter_num * BLK_SIZE;
          //L1 MEM/64 = counter_byte, L2 L1/32
          int L2 = counter_byte / 32;
          mt_level_entry_num.push_back(counter_num);
          mt_level_entry_num.push_back(L2/BLK_SIZE);
          if(L2 < 64){ //the uint8_t number < 64, the vault initial fault
              std::cerr<<"The vault initial fault. Please check MEM_SIZE!"<<std::endl;
              return;
          }
          int Li(L2);
          int i;
          for (i = 0; Li > 64; i++)
          {
              Li /= 16;
              if (Li>=64)
              {   //////////////////////R.1 for 8GB bugs
                // mt_level_entry_num.push_back(Li/BLK_SIZE);
                if (i == 2)
                {
                  if (Li < 2048)
                  {
                    mt_level_entry_num.push_back(2048/64);
                    Li = 2048;
                    std::cout<<"R1 version must = 8GB simulation correct to 2048"<<std::endl;
                  }
                  else
                  {
                    mt_level_entry_num.push_back(Li/BLK_SIZE);
                  }
                }
                else
                {
                  mt_level_entry_num.push_back(Li/BLK_SIZE);
                }
              }
              else
              {
                mt_level_entry_num.push_back(1);
              }
          }
          if(Li <64){
              // std::cerr<<"Please check MEM_SIZE to satisfy VAULT arity!"<<std::endl;
              // return;
              std::cout<<"Please check MEM_SIZE to satisfy VAULT arity!"<<std::endl;
          }
          MT_LEVEL = i + 2;
          if(VAULT_DEBUG){
              std::cout<<"L1 entry: "<<mt_level_entry_num[0]<<". mt level list size: "<<mt_level_entry_num.size()<<std::endl;
              std::cout<<"L2_entry: "<<mt_level_entry_num[1]<<std::endl;
              std::cout<<"MEM_SIZE: "<<MEM_SIZE<<" MT_LEVEL: "<<MT_LEVEL<<std::endl;
          }
      }

      void splitCounter(uint8_t* CounterMixed , uint64_t* splitCounter, uint64_t& globalCounter, int arity, bool onChip)
      {
        std::bitset<512> Counter_bitset;
        std::bitset<512> tmp_bitset;
        std::bitset<512> mask_bitset_Last_level_64(0x3f);
        std::bitset<512> mask_bitset(0x7f);
        std::bitset<512> mask_comp_bitset(0x3f);
        std::bitset<512> mask_bitset_32(0xfff);
        std::bitset<512> mask_bitset_16(0xffffff);
        std::bitset<512> mask_bitset_16_onChip(0xfffffff);
        int ptr;
        array_2_bitset(CounterMixed,64,Counter_bitset,ptr);
        // std::cout<<"Counter_bitset: "<<Counter_bitset<<std::endl;

        if (64 == arity)
        {
          #if VAULT_LAST_64
          Counter_bitset >>= 64;
          #endif
            for (size_t i = 0; i < 64; i++)
            {
                tmp_bitset = Counter_bitset & (VAULT_LAST_64 ? mask_bitset_Last_level_64 : (VAULT_COMP ? mask_comp_bitset : mask_bitset));
                splitCounter[i] = tmp_bitset.to_ulong();
                Counter_bitset >>= BIT_COUNTER_LEN;
            }
            globalCounter = Counter_bitset.to_ulong();
            return;
        }
        if (32 == arity)
        {
            //Notice to delete hash first
            Counter_bitset >>= 64;
            for (size_t i = 0; i < 32; i++)
            {
                tmp_bitset = Counter_bitset & mask_bitset_32;
                splitCounter[i] = tmp_bitset.to_ulong();
                Counter_bitset >>=12;
            }
            globalCounter = Counter_bitset.to_ulong();
            return;
        }
        if (16 == arity)
        {
            if (!onChip)
            {
              //the hash is no need for on-chip nodes
              Counter_bitset >>= 64;
            }
            for (size_t i = 0; i < 16; i++)
            {
                if (!onChip)
                {
                  tmp_bitset = Counter_bitset & mask_bitset_16;
                  splitCounter[i] = tmp_bitset.to_ulong();
                  Counter_bitset >>=24;
                }
                else{
                  tmp_bitset = Counter_bitset & mask_bitset_16_onChip;
                  splitCounter[i] = tmp_bitset.to_ulong();
                  Counter_bitset >>=28;
                }
            }
            globalCounter = Counter_bitset.to_ulong();
        }
      }

      ////////////////////
      //
      //Modified as the tree init
      //
      ///////////////////
      void getCounterTrace(unsigned long phy_addr)
      {
          //L1 counter directly map as the addr
          Counter_modify_list.clear();
          int l1_blk_entry = phy_addr/(64*64);
          int l1_cnt_entry = (phy_addr/64)%64;
          Counter_modify_list.push_back(l1_blk_entry);
          Counter_modify_list.push_back(l1_cnt_entry);
          //following level needs to calculate with arity
          int l2_blk_entry = l1_blk_entry/(32);
          int l2_cnt_entry = (l1_blk_entry%(32));
          Counter_modify_list.push_back(l2_blk_entry);
          Counter_modify_list.push_back(l2_cnt_entry);
          //arity 16 with for loop
          int li_blk_entry = l2_blk_entry;
          int li_cnt_entry = l2_cnt_entry;
          for (size_t i = 0; i < MT_LEVEL-2; i++)
          {
              li_cnt_entry = (li_blk_entry % (16));
              li_blk_entry /= (16);
              Counter_modify_list.push_back(li_blk_entry);
              Counter_modify_list.push_back(li_cnt_entry);
          }
          if(VAULT_DEBUG){
              std::cout<<"<<====The counter trace"<<std::endl;
              for (size_t i = 0; i < MT_LEVEL; i++)
              {
                  std::cout<<"Level: "<<i<<":"<<std::hex<<Counter_modify_list[i*2]<<std::dec<<" "<<Counter_modify_list[i*2+1]<<std::endl;
              }
              std::cout<<"The counter trace====>>"<<std::endl;
          }
          if (TRACE_DEBUG && trace_control)
          {
              for (size_t i = 0; i < MT_LEVEL; i++)
              {
                  std::cout<<std::dec<<"Level "<<i<<": "<<Counter_modify_list[i*2]<<std::dec<<" "<<Counter_modify_list[i*2+1];
                  std::cout<<"    ";
              }
              std::cout<<std::endl;
          }
          
      }

      void MixedCounter(uint8_t* CounterMixed , uint64_t* splitCounter, uint64_t& globalCounter, int arity, bool onChip)
      {
        std::bitset<512> Counter_bitset(0);
        std::bitset<512> tmp_bitset;
        std::bitset<512> mask_bitset(0x7f);
        std::bitset<512> mask_comp_bitset(0x3f);
        std::bitset<512> mask_bitset_32(0xfff);
        std::bitset<512> mask_bitset_16(0xffffff);
        int ptr;

        if (64 == arity)
        {
            for (size_t i = 0; i < 64; i++)
            {
                tmp_bitset = splitCounter[i];
                Counter_bitset |= (VAULT_LAST_64 ? (tmp_bitset<<(i*BIT_COUNTER_LEN + 64)) : (tmp_bitset<<(i*(VAULT_COMP ? 6 : 7))));
            }
            tmp_bitset = globalCounter;
            Counter_bitset |= (tmp_bitset<<448);
            bitset_2_array(Counter_bitset,512,CounterMixed,ptr);
            return;
        }
        if (32 == arity)
        {
            //Notice to pedding hash first
            for (size_t i = 0; i < 32; i++)
            {
                tmp_bitset = splitCounter[i];
                Counter_bitset |= (tmp_bitset<<(i*12 + 64));
            }
            tmp_bitset = globalCounter;
            Counter_bitset |= (tmp_bitset<<448);
            bitset_2_array(Counter_bitset,512,CounterMixed,ptr);
            return;
        }
        if (16 == arity)
        {
            for (size_t i = 0; i < 16; i++)
            {
                tmp_bitset = splitCounter[i];
                if (!onChip)
                {
                  Counter_bitset |= (tmp_bitset<<(i*24+64));
                }
                else
                {
                  Counter_bitset |= (tmp_bitset<<(i*28));
                }
            }
            tmp_bitset = globalCounter;
            Counter_bitset |= (tmp_bitset<<448);
            bitset_2_array(Counter_bitset,512,CounterMixed,ptr);
            return;
        }
      }

      //************** the counter propragating flow needs to be merged here
      //A signal node adding realized as follows
      //off means the offset of this counter
      bool singleAdd(uint8_t* vblk, int arity, int off, bool onChip)
      {
        if (off>=arity)
        {
          if(VAULT_DEBUG) std::cout<<"VAULT CLASS"<<std::endl;
          assert(off>=arity);
        }
        
        uint64_t* sCounter;
        uint64_t gCounter;
        sCounter = new uint64_t[arity];
        splitCounter(vblk,sCounter,gCounter,arity,onChip);
        uint64_t resetFlag=0;
        if (arity==64)
        {
          if (VAULT_COMP || VAULT_LAST_64)
          {
            resetFlag = 0x3f;
          }
          else
          {
            resetFlag = 0x7f;
          }
          
        }
        else if(arity == 32)
        {
          resetFlag = 0xfff;
        }
        else if(arity == 16)
        {
          if (!onChip)
          {
            resetFlag = 0xffffff;
          }
          else
          {
            resetFlag = 0xfffffff;
          }
        }

        if (sCounter[off] == resetFlag)
        {
          if(VAULT_DEBUG) std::cout<<"VAULT CLASS <<<<==== reset happen in arity ====>>>> "<<arity<<std::endl;
          memset(sCounter,0,arity*8);
          gCounter ++;
          MixedCounter(vblk,sCounter,gCounter,arity,onChip);
          delete [] sCounter;
          return true;
        }
        else
        {
          sCounter[off] ++;
          MixedCounter(vblk,sCounter,gCounter,arity,onChip);
          delete [] sCounter;
          return false;
        }
      }

      //for another anding
      void singleAdd(std::bitset<512>& counterIn, int arity, int off, bool onChip=false)
      {
        uint8_t vblk[64];
        int ptr;
        bitset_2_array(counterIn,512,vblk,ptr);
        if (off>=arity)
        {
          if(VAULT_DEBUG) std::cout<<"VAULT CLASS"<<std::endl;
          assert(off>=arity);
        }
        
        uint64_t* sCounter;
        uint64_t gCounter;
        sCounter = new uint64_t[arity];
        splitCounter(vblk,sCounter,gCounter,arity,onChip);
        uint64_t resetFlag=0;
        if (arity==64)
        {
          if (VAULT_COMP || VAULT_LAST_64)
          {
            resetFlag = 0x3f;
          }
          else
          {
            resetFlag = 0x7f;
          }
          
        }
        else if(arity == 32)
        {
          resetFlag = 0xfff;
        }
        else if(arity == 16)
        {
          resetFlag = 0xffffff;
        }

        if (sCounter[off] == resetFlag)
        {
          if(VAULT_DEBUG) std::cout<<"VAULT CLASS <<<<==== reset happen in arity ====>>>> "<<arity<<std::endl;
          memset(sCounter,0,arity*8);
          gCounter ++;
          MixedCounter(vblk,sCounter,gCounter,arity,onChip);
          array_2_bitset(vblk,64,counterIn,ptr);
          delete [] sCounter;
        }
        else
        {
          sCounter[off] ++;
          MixedCounter(vblk,sCounter,gCounter,arity,onChip);
          array_2_bitset(vblk,64,counterIn,ptr);
          delete [] sCounter;
        }
      }

      void counterPropragate(int hashNum = 5)
      {
        #if EAGER_MODE
        for (int i = 0; i < MT_LEVEL; i++)
        {
          bool reset;
          if (i==0)
          {
            reset = singleAdd(root,16,Counter_modify_list[2*(MT_LEVEL-1)+1],true);
            resetRecord[MT_LEVEL-1] = (reset);
          }
          else if(i==1)
          {
            reset = singleAdd(root2[Counter_modify_list[2*(MT_LEVEL-2)]],16,Counter_modify_list[2*(MT_LEVEL-2)+1],true);
            resetRecord[MT_LEVEL-2] = (reset);
          }
          else{
            reset = singleAdd((counterOP+i-2)->blk,(counterOP+i-2)->arity,(counterOP+i-2)->off,false);
            resetRecord[i-2] =(reset);
          }
        }
        #else

        for (int i = 0; i < MT_LEVEL; i++)
        {
            resetRecord[i]=false;
        }

        if (hashNum >= 5)
        {
          for (int i = 0; i < MT_LEVEL; i++)
          {
            bool reset;
            if (i==0)
            {
              reset = singleAdd(root,16,Counter_modify_list[2*(MT_LEVEL-1)+1],true);
              resetRecord[MT_LEVEL-1] = (reset);
            }
            else if(i==1)
            {
              reset = singleAdd(root2[Counter_modify_list[2*(MT_LEVEL-2)]],16,Counter_modify_list[2*(MT_LEVEL-2)+1],true);
              resetRecord[MT_LEVEL-2] = (reset);
            }
            else{
              reset = singleAdd((counterOP+i-2)->blk,(counterOP+i-2)->arity,(counterOP+i-2)->off,false);
              resetRecord[i-2] =(reset);
            }
          }
        }
        else
        {
          // no need to modify the higher ancestor
          for (int i = 0; i < hashNum; i++)
          {
            bool reset;
            reset = singleAdd((counterOP+i)->blk,(counterOP+i)->arity,(counterOP+i)->off,false);
            resetRecord[i] =(reset);
          }
        }
        #endif
        if (VAULT_DEBUG)
        {
          std::bitset<512> tmp;
          int ptr;
          for (size_t i = 0; i < MT_LEVEL; i++)
          {
            if (i==0)
            {
              array_2_bitset(root,64,tmp,ptr);
            }
            else if(i==1)
            {
              array_2_bitset(root2[Counter_modify_list[2*(MT_LEVEL-2)]],64,tmp,ptr);
            }
            else
            {
              array_2_bitset((counterOP+i-2)->blk,64,tmp,ptr);
            }
            std::cout<<"VAULT CLASS <<<<==== counter updata value ====>>>>"<<tmp<<std::endl;
          }
        }
      }


  public:
      vault(bool DEBUG, int MEM_SIZE) 
      {
        VAULT_DEBUG = DEBUG;
        this->MEM_SIZE = MEM_SIZE;
        getMTLevel();
        for (int i = 0; i < MT_LEVEL; i++)
        {
            COUNTER_SIZE += mt_level_entry_num[i] * 64;
            resetRecord.push_back(false);
        }
        counterOP = new vault_blk[MT_LEVEL-2];
        std::cout<<"<<<<======== The vault is ready ========>>>>"<<std::endl;
      }

  };


  vault vault_engine;

  ///////////////////////////////
  //
  //The cycle tap to measure the memory access cyclse
  //
  ///////////////////////////////
  struct MemAccessTap
  {
    Cycles C_RDCNT=Cycles(0);
    Cycles C_RDMAC=Cycles(0);
    Cycles C_RDDATA=Cycles(0);

  };
  
  MemAccessTap memAccessTap;

  ///////////////////
  //
  //Here we design the prediction data class.
  //
  ///////////////////
  typedef std::bitset<512> ChunkBlk;

  struct PredictionTableEntry{
    Addr addr=0;
    bool addr_valid=false;
    ChunkBlk ctr0=0; //cover the MAC in
    bool ctr0_valid=false;
    bool ctr0_Caled=false; // flag the HASH pre-caled
    ChunkBlk CipherText=0;
    bool CipherText_valid=false;
    bool CipherTextCal=false;
    int hashId=-1; //IDLE
    uint64_t allocCycle=0;

    ChunkBlk Pad=0;
    bool PadPreCaled=false; //flag tis pad has been pre-calculated

    bool hashHit=false;

    //--------------------------- wxr 10.9
    // a flag to indicate where is this entry from 
    int source_id = 0; 

    bool allReady()
    {
      return addr_valid && ctr0_valid && ctr0_Caled && CipherText_valid && PadPreCaled;
    }

    void print()
    {
      std::cout<<std::hex<<addr<<" addr_valid: "<<addr_valid<<" ctr0_valid: "<<ctr0_valid<<" CipherText_valid: "<<CipherText_valid<<" PadPreCaled: "<<PadPreCaled<<" hashID: "<<hashId<<std::dec<<std::endl;
    }

  };

  struct aesEntry{
    Addr addr=0;
    ChunkBlk OTP=0; //cover the MAC in

    bool caled=false; //flag tis pad has been pre-calculated
    bool hasHit=false;


  };

  struct TableInx{
    int groupInx;
    int innerInx;
  };

  class TableGroup{
    public:
    std::vector<ChunkBlk> CtrChain;
    bool CtrChainPreCaled;
    bool CtrChainValid;

    ChunkBlk chunkCtr0;
    uint8_t blkCtr0[64];
    bool chunkCtr0Valid = false;
    bool blkCtr0Valid = false;

    //the addr segmentation covered by this group
    Addr Begin_Addr;

    std::list<PredictionTableEntry> TableEntry;

    /////////////////////////////////
    //11.9  AES entries
    /////////////////////////////////
    std::list<aesEntry> AesEntry;

    /////////////////
    //
    //11.1 to get the patterns of memory access
    //
    ////////////////
    int realWidth;
    std::vector<uint64_t> realTransAddr;
    std::vector<bool> hitRecord;
    int nowBias=0x40;
    int secondBias=nowBias+0x40;

    TableGroup(int n, int TreeLevel, Addr begin_Addr=0)
    {
      realWidth=n;
      for (size_t i = 0; i < n; i++)
      {
        PredictionTableEntry p;
        TableEntry.push_back(p);
        realTransAddr.push_back(0);
        hitRecord.push_back(1);

        aesEntry Ap;
        AesEntry.push_back(Ap);
      }
      for (size_t i = 0; i < TreeLevel-1; i++)
      {
        CtrChain.push_back(ChunkBlk{0});
      }
      Begin_Addr = begin_Addr;

      CtrChainPreCaled =false;
      CtrChainValid =false;
    }

    void updateReal(Addr addr, bool hit)
    {
      for (size_t i = realWidth-1; i > 0; i--)
      {
        realTransAddr[i] = realTransAddr[i-1];
        hitRecord[i] = hitRecord[i-1];
      }
      realTransAddr[0] = addr;
      hitRecord[0] = hit;
    }

    std::vector<Addr>& getAddrValue()
    {
      return realTransAddr;
    }

    std::vector<bool>& getHitRecord()
    {
      return hitRecord;
    }

    void print()
    {
      auto j_itr = TableEntry.begin();
      for (; j_itr != TableEntry.end(); j_itr++)
      {
        (*j_itr).print();
      }
    }

    void printAes()
    {
      auto j_itr = AesEntry.begin();
      for (; j_itr != AesEntry.end(); j_itr++)
      {
        std::cout<<"  "<<std::hex<<(*j_itr).addr<<std::dec;
      }
      std::cout<<std::endl;
    }
  };


  class TeePrediction{

    public:
    int Group_cover = 4096; // a group can cover 4 KB space
    Addr Group_cover_mask = 0xffffffffffffffff - (Group_cover-1);

    ///////////////
    //
    //vault counter operation function
    //
    ///////////////
    vault tree_engine;

    public:
    std::list<TableGroup> group;
    int GroupEntryNum;
    int GroupNum;
    int TreeLevel;
    TeePrediction(int groupNum, int groupEntryNum, int enc_memSize) : tree_engine(false,enc_memSize)
    {
      GroupEntryNum = groupEntryNum;
      GroupNum = groupNum;
      TreeLevel = tree_engine.MT_LEVEL;
      for (size_t i = 0; i < groupNum; i++)
      {
        group.push_back(TableGroup{groupEntryNum, TreeLevel});
      }
    }


    /*
    this function will be called when the addr cannot be covered by
    existed group
    */
    void alloc_Group(Addr PredictionDataAddr)
    {
      group.pop_front();
      TableGroup newIn(GroupEntryNum,TreeLevel,PredictionDataAddr&Group_cover_mask);
      group.push_back(newIn);
    }

    /*
    this function will compare the Ctr chain to see if they match this prediction
    */
    bool CmpCtrChain(std::vector<ChunkBlk> CtrChain, std::vector<ChunkBlk> TableChain)
    {
      bool tmp = true;
      for (size_t i = 0; i < TreeLevel-1; i++)
      {
        tmp = tmp && ((CtrChain[i]>>64) == (TableChain[i]>>64));
      }
      return tmp;
    }

    /*
    find the match
    return:
    -1: no match
     1: all match
     2: only addr match **means prediction wrong
    */
    int findMatch(Addr DataAddr, ChunkBlk Ctr0, std::vector<ChunkBlk> CtrChain, ChunkBlk CipherText, TableInx& index, int& source_id, int& hashNum, bool updateHit=false)
    {
      int i_for_group=0;
      int j_for_inner=0;
      for (auto i_itr = group.begin(); i_itr != group.end(); i_itr++)
      {
        for (auto j_itr = (*i_itr).TableEntry.begin(); j_itr != (*i_itr).TableEntry.end(); j_itr++)
        {
          //DA:data address
          bool allCal_DA_Ctr0 = (*j_itr).addr_valid && (*j_itr).CipherText_valid && (*j_itr).CipherTextCal && (*j_itr).ctr0_valid && (DataAddr == (*j_itr).addr);
          allCal_DA_Ctr0 = allCal_DA_Ctr0 && ((Ctr0>>64) == ((*j_itr).ctr0 >> 64)); 
          #if PREDICTION_DEBUG
          std::cout<<"----------------++++xxxx++++ . find addr ctro match: "<<(DataAddr==(*j_itr).addr)<<" ctr0: "<<((Ctr0>>64) == ((*j_itr).ctr0 >> 64))<<std::endl;
          // if (DataAddr==(*j_itr).addr)
          // {
          //   std::cout<<"   Ctr0 now: "<<Ctr0<<std::endl;
          //   std::cout<<" OutCtr now: "<<(*j_itr).ctr0<<std::endl;
          // }
          
          #endif
          if (allCal_DA_Ctr0)
          {
            //+++++++++++++++++++++++++++++++++ 2023.11.20
            //the ctr branch/chain didn't be buffered now
            //---------------------------------
            // bool CtrMatch = CmpCtrChain((*i_itr).CtrChain,CtrChain);
            bool CipherMatch = CipherText == (*j_itr).CipherText;
            if (CipherMatch)
            {
              index = {i_for_group,j_for_inner};
              source_id = (*j_itr).source_id;
              if (updateHit)
              {
                (*j_itr).hashHit = true;
              }
              assert((*j_itr).hashId != -1);
              
              return 1;
            }
          }
          else
          {
            bool basicMatch= (*j_itr).addr_valid && (*j_itr).CipherText_valid && (*j_itr).ctr0_valid && ((Ctr0 >>64) == ((*j_itr).ctr0>>64));
            // bool CtrMatch = CmpCtrChain((*i_itr).CtrChain,CtrChain);
            bool CipherMatch = CipherText == (*j_itr).CipherText;
            if (DataAddr == (*j_itr).addr && CipherMatch && (!(*j_itr).CipherTextCal) && basicMatch)
            {
              index = {i_for_group,j_for_inner};
              hashNum = (*j_itr).hashId;
              if (updateHit)
              {
                (*j_itr).hashHit = true;
              }
              return 2;
            }
          }
          j_for_inner++;
        }
        i_for_group ++;
      }
      return -1;
    }

    int findMatchInterface(Addr DataAddr, vault& vault_engine, uint8_t* CipherText, int& source_id, int& hashNum)
    {
      int ptr;
      ChunkBlk Ctr0;
      ChunkBlk CipherT;
      std::vector<ChunkBlk> CtrChain;
      vault_engine.array_2_bitset(vault_engine.counterOP[0].blk,64,Ctr0,ptr);

      int deep=0;
      for (size_t i = 0; i < (TreeLevel-2-1); i++)
      {
        CtrChain.push_back(ChunkBlk{0});
        deep ++;
        vault_engine.array_2_bitset(vault_engine.counterOP[i+1].blk,64,CtrChain[i],ptr);
      }
      CtrChain.push_back(ChunkBlk{0});
      CtrChain.push_back(ChunkBlk{0});
      vault_engine.array_2_bitset(vault_engine.root2[vault_engine.Counter_modify_list[2*(TreeLevel-2)]],64,CtrChain[deep],ptr);
      deep ++;
      vault_engine.array_2_bitset(vault_engine.root,64,CtrChain[deep],ptr);
      
      vault_engine.array_2_bitset(CipherText,64,CipherT,ptr);

      int res;
      TableInx t;
      res = findMatch(DataAddr,Ctr0,CtrChain,CipherT,t,source_id,hashNum,true);
      return res;
    }


    //for aes find
    int findMatch(Addr DataAddr)
    {
      int i_for_group=0;
      int j_for_inner=0;
      for (auto i_itr = group.begin(); i_itr != group.end(); i_itr++)
      {
        for (auto j_itr = (*i_itr).TableEntry.begin(); j_itr != (*i_itr).TableEntry.end(); j_itr++)
        {
          //DA:data address
          bool allCal_DA_Ctr0 = (*j_itr).addr_valid && (DataAddr == (*j_itr).addr);
          if (allCal_DA_Ctr0)
          {
            return 1;
          }
          else
          {
            if (DataAddr == (*j_itr).addr)
            {
              return 2;
            }
          }
          j_for_inner++;
        }
        i_for_group ++;
      }
      return -1;
    }


    int findMatchInterface(Addr DataAddr)
    {
      int res;
      res = findMatch(DataAddr);
      return res;
    }

    /*
    find the group covers the predictionAddr
    */
    bool findGroupRange(Addr PredctionDataAddr, TableInx& index, bool groupValid=false)
    {
      int i = 0;
      for (auto i_itr = group.begin(); i_itr != group.end(); i_itr++)
      {
        if ( (PredctionDataAddr>= (*i_itr).Begin_Addr) &&  (PredctionDataAddr <= ((*i_itr).Begin_Addr + Group_cover-64)))
        {
          index = {i,GroupEntryNum-1};
          if (groupValid)
          {
            bool groupV = (*i_itr).CtrChainValid;
            return groupV;
          }
          else
          {
            return true;
          }
        }
        i++;
      }
      return false;
    }

    /*
    this function can only allocate the the address into the entry.
    other info will be pushed after
    */
    TableInx alloc_Entry(Addr PredictionDataAddr, uint64_t curCycle, int source_id=0)
    {
      TableInx whereisGroup;
      bool findWhereGroup = findGroupRange(PredictionDataAddr,whereisGroup);
      if (findWhereGroup)
      {
        auto i_itr = group.begin();
        for (size_t i = 0; i < whereisGroup.groupInx; i++)
        {
          i_itr++;
        }
        // (*i_itr).TableEntry.pop_front();
        // PredictionTableEntry p;
        // p.addr = PredictionDataAddr;
        // p.addr_valid = true;
        // p.source_id = source_id;
        // p.allocCycle = curCycle;
        // (*i_itr).TableEntry.push_back(p);
        // whereisGroup.innerInx = GroupEntryNum-1;

        bool hasFindHit = false;
        int entryCnt=0;
        for (auto j_itr = (*i_itr).TableEntry.begin(); j_itr != (*i_itr).TableEntry.end(); j_itr++)
        {
          if ((*j_itr).hashHit)
          {
            assert((*j_itr).hashId != -1);
            hasFindHit = true;
            (*j_itr).addr = PredictionDataAddr;
            (*j_itr).addr_valid = true;
            (*j_itr).source_id = source_id;
            (*j_itr).allocCycle = curCycle;

            (*j_itr).hashHit = false;
            (*j_itr).ctr0_valid = false;
            (*j_itr).ctr0_Caled = false;
            (*j_itr).hashId = -1;
            (*j_itr).CipherText_valid = false;
            (*j_itr).CipherTextCal = false;
            whereisGroup.innerInx = entryCnt;
            break;
          }
          entryCnt ++;
        }
        if (!hasFindHit)
        {
          (*i_itr).TableEntry.pop_front();
          PredictionTableEntry p;
          p.addr = PredictionDataAddr;
          p.addr_valid = true;
          p.source_id = source_id;
          p.allocCycle = curCycle;
          (*i_itr).TableEntry.push_back(p);
          whereisGroup.innerInx = GroupEntryNum-1;
        }

      }
      else
      {
        //here due to miss for covering by existing group
        alloc_Group(PredictionDataAddr);
        auto i_itr = group.end();
        i_itr --;
        (*i_itr).TableEntry.pop_front();
        PredictionTableEntry p;
        p.addr = PredictionDataAddr;
        p.addr_valid = true;
        p.source_id = source_id;
        p.allocCycle = curCycle;
        (*i_itr).TableEntry.push_back(p);
        whereisGroup=TableInx{GroupNum-1,GroupEntryNum-1};
      }
      return whereisGroup;
    }

    /*
    get the & of the group used with TableInx
    */
    TableGroup& getGroup(TableInx tInx)
    {
      auto itr = group.begin();
      for (size_t i = 0; i < tInx.groupInx; i++)
      {
        itr++;
      }
      return *itr;
    }

    void printRealTrans(TableInx tInx)
    {
      std::vector<Addr> realTans = getGroup(tInx).realTransAddr;
      std::cout<<std::dec<<"   +++"<<std::endl;
      for (size_t i = 0; i < realTans.size(); i++)
      {
        std::cout<<std::hex<<" "<<realTans[i];
      }
      std::cout<<" nowBias: "<<getGroup(tInx).nowBias<<std::endl<<std::dec<<"   ---"<<std::endl;
      
    }


    // int nowBias=0x40;
    /*
    get the bias when the prediction hitrate lower
    */
    int predictionBias(TableInx tInx)
    {
      std::vector<Addr> realTrans = getGroup(tInx).realTransAddr;
      std::vector<bool> hitRecord = getGroup(tInx).hitRecord;
      int defaultBias=getGroup(tInx).nowBias;
      int cnt=0;
      // int width = getGroup(tInx).realWidth;
      int width = 4;
      for (size_t i = 0; i < width; i++)
      {
        cnt += hitRecord[i];
      }
      bool badPrediction = cnt == 0;
      // bool badPrediction = true;
      if (badPrediction)
      {
        // printRealTrans(tInx);
        //we need use the realTransAddr to track the accessing pattern
        //we average the bias
        std::vector<int> biasAll;
        std::vector<int> score;
        for (size_t i = 0; i < width-1; i++)
        {
          biasAll.push_back(realTrans[i]-realTrans[i+1]);
          score.push_back(0);
        }
        for (size_t i = 2; i < width; i++)
        {
          biasAll.push_back(realTrans[0]-realTrans[i]);
          score.push_back(0);
        }
        biasAll.push_back(realTrans[1]-realTrans[width-1]);
        score.push_back(0);
        for (size_t i = 0; i < biasAll.size()-1; i++)
        {
          for (size_t j = i+1; j < biasAll.size(); j++)
          {
            if (biasAll[i] == biasAll[j])
            {
              score[i]+=1;
            }
          }
        }

        int sum=0;
        for (size_t i; i < biasAll.size(); i++)
        {
          sum += biasAll[i];
        }
        sum = (sum/64)/biasAll.size();
        sum = sum * 64;

        //then pick the highest score. or directly choose the latest
        int finalBias=0;
        int pickOne=0;
        int indexHigh=0;
        for (size_t i = 0; i < score.size(); i++)
        {
          if (pickOne<score[i])
          {
            pickOne = score[i];
            finalBias = biasAll[i];
            indexHigh = i;
          }
        }

        if (pickOne==0)
        {
          finalBias = biasAll[0];
          // finalBias = sum;
        }
        else
        {
          //we set the hightest to 0. to find the second high
          score[indexHigh] = 0;
        }

        for (size_t i = 0; i < score.size(); i++)
        {
          if (pickOne<score[i])
          {
            pickOne = score[i];
            getGroup(tInx).secondBias = biasAll[i];
          }
        }

        if (std::abs(finalBias) >= 4096 || finalBias==0)
        {
          finalBias =0x40;
        }
        
        if (pickOne == 0)
        {
          getGroup(tInx).secondBias = finalBias + 0x40;
        }
        if (std::abs(getGroup(tInx).secondBias) >= 4096)
        {
          getGroup(tInx).secondBias =0x80;
        }

        
        
        //then we align to 0x40
        getGroup(tInx).nowBias = finalBias;
        return finalBias;
      }
      return defaultBias;
    }

    /*
    predict the read request prefetcher
    Version1: directly prefetech next cacheline. Get the data addr and Ctr0 Addr.
    */
    void prefetchAddrs(Addr& PredictionDataAddr, Addr& Ctr0Addr)
    {
      //VERSION1: 
      PredictionDataAddr += 0x40; 
      Ctr0Addr = Ctr0Addr;
    }

    /*
    this function is used to find any repeat address in group
    from this PredictionDataAddr to "ring add"
    */
    bool PredictionAddr(Addr& PredictionDataAddr, TableInx groupTInx, int bias=0x40)
    {
      auto group_itr = group.begin();
      for (size_t i = 0; i < groupTInx.groupInx; i++)
      {
        group_itr ++;
      }
      bool find = false;
      //we search for the proper prediction address
      Addr thisGroupEnd = (*group_itr).Begin_Addr + 4096;
      Addr beginAddr= bias>0 ? PredictionDataAddr + bias : PredictionDataAddr - std::abs(bias);
      Addr oriAddr=PredictionDataAddr;
      Addr thisGroupBegin = (*group_itr).Begin_Addr ;

      if (beginAddr > thisGroupEnd-0x40)
      {
        beginAddr = thisGroupEnd-0x40;
      }
      if (beginAddr < thisGroupBegin)
      {
        beginAddr = thisGroupBegin;
      }
      bool UpOrDown = bias>0 ? true : false;
      // for (beginAddr = PredictionDataAddr+0x40; beginAddr<thisGroupEnd; beginAddr+=0x40)
      for (size_t i = 0; i < 64; i++)
      {
        for (auto i_itr = (*group_itr).TableEntry.begin() ; i_itr != (*group_itr).TableEntry.end(); i_itr++)
        {
          if((*i_itr).addr == beginAddr && (*i_itr).addr_valid)
          {
            find = true;
          }
        }
        if (!find)
        {
          PredictionDataAddr = beginAddr;
          return true;
        }
        find = false;

        if (beginAddr >= (thisGroupEnd-0x40))
        {
          beginAddr = thisGroupBegin;
          continue;
        }
        else if (beginAddr <= (thisGroupBegin))
        {
          beginAddr = thisGroupEnd;
          continue;
        }


        beginAddr = UpOrDown ? beginAddr +0x40 : beginAddr - 0x40;
        if (beginAddr == oriAddr)
        {
          beginAddr = UpOrDown ? beginAddr +0x40 : beginAddr - 0x40;
          if (beginAddr >= (thisGroupEnd-0x40))
          {
            beginAddr = thisGroupBegin;
          }
          else if (beginAddr <= (thisGroupBegin))
          {
            beginAddr = thisGroupEnd;
            continue;
          }
        }

      }
      std::cout<<"  ERROR no address insert"<<std::endl;
      exit(0);
    }

    /*
    this prediction used when write
    */
    bool PredictionAddrNext(Addr& PredictionDataAddr, TableInx groupTInx, Addr bias = 0x40)
    {
      auto group_itr = group.begin();
      for (size_t i = 0; i < groupTInx.groupInx; i++)
      {
        group_itr ++;
      }
      //we search for the proper prediction address
      Addr thisGroupEnd = (*group_itr).Begin_Addr + 4096;
      bool find = false;
      Addr beginAddr;
      for (beginAddr = PredictionDataAddr+bias; beginAddr<thisGroupEnd; beginAddr+=0x40)
      {
        for (auto i_itr = (*group_itr).TableEntry.begin() ; i_itr != (*group_itr).TableEntry.end(); i_itr++)
        {
          if((*i_itr).addr == beginAddr && (*i_itr).addr_valid)
          {
            find = true;
          }
        }
        if (!find)
        {
          PredictionDataAddr = beginAddr;
          break;
        }
      }

      if (PredictionDataAddr == beginAddr)
      {
        return true;
      }
      else
      {
        return false;
      }
    }
    

    /////////////////////////
    //
    //2023.11.9 update the AES table
    //
    /////////////////////////
    void updateAesEntry(TableInx tInx, Addr addr)
    {
      bool hasHit = false;
      for (auto itr = getGroup(tInx).AesEntry.begin() ; itr != getGroup(tInx).AesEntry.end(); itr++)
      {
        if ((*itr).hasHit)
        {
          hasHit = true;
          (*itr).addr = addr;
          (*itr).caled = true;
          (*itr).hasHit = false;
          return;
        }
      }
      if (!hasHit)
      {
        getGroup(tInx).AesEntry.pop_front();
        aesEntry Ap;
        Ap.addr = addr;
        Ap.caled = true;
        Ap.OTP = 0;
        getGroup(tInx).AesEntry.push_back(Ap);
      }
    }

    bool findAesResult(Addr addr)
    {
      for (auto itr = group.begin(); itr != group.end(); itr++)
      {
        for (auto jtr = (*itr).AesEntry.begin(); jtr != (*itr).AesEntry.end(); jtr++)
        {
          bool find = addr == (*jtr).addr && (*jtr).caled;
          if (find)
          {
            (*jtr).hasHit = true;
            return true;
          }
        }
      }

      return false;
    }

    bool findInVector(std::vector<Addr>& vec, Addr addr)
    {
      for (size_t i = 0; i < vec.size(); i++)
      {
        if (vec[i]==addr)
        {
          return true;
        }
      }
      return false;
    }

    void AddAesEntry(TableInx tInx, Addr nowAddr,int bias)
    {

      Addr predictionAddr = bias>0 ? nowAddr + bias : nowAddr-std::abs(bias);
      if (bias>0 && predictionAddr>=(getGroup(tInx).Begin_Addr+4096))
      {
        predictionAddr = getGroup(tInx).Begin_Addr + 4096 - 64;
      }
      else if(bias < 0 && predictionAddr<(getGroup(tInx).Begin_Addr))
      {
        predictionAddr = getGroup(tInx).Begin_Addr;
      }

      std::vector<Addr> existAddr;
      for (auto itr = getGroup(tInx).AesEntry.begin(); itr != getGroup(tInx).AesEntry.end(); itr++)
      {
        existAddr.push_back((*itr).addr);
      }
      for (auto itr = getGroup(tInx).TableEntry.begin(); itr != getGroup(tInx).TableEntry.end(); itr++)
      {
        existAddr.push_back((*itr).addr);
      }
      // std::sort(existAddr.begin(),existAddr.end());

      bool hitExist=false;
      for (size_t i = 0; i < existAddr.size(); i++)
      {
        if (predictionAddr == existAddr[i])
        {
          while (true)
          {
            predictionAddr = bias>0 ? predictionAddr +0x40 : predictionAddr - 0x40;
            if (bias>0 && predictionAddr>=(getGroup(tInx).Begin_Addr+4096))
            {
              predictionAddr = getGroup(tInx).Begin_Addr;
            }
            else if(bias < 0 && predictionAddr<(getGroup(tInx).Begin_Addr))
            {
              predictionAddr = getGroup(tInx).Begin_Addr+4096-64;
            }
            if (!findInVector(existAddr,predictionAddr))
            {
              break;
            }
          }
        }
      }
      updateAesEntry(tInx,predictionAddr);
      // getGroup(tInx).printAes();
    }

    /*
    prediction the ctr "adding pattern"
    ********************************************NOTE
    ***** the counter chain predictions are all depended on PredictionDataAddr
    */
    void predictionCounters(Addr PredictionDataAddr, ChunkBlk& Ctr0, std::vector<ChunkBlk>& CtrChain)
    {
      tree_engine.getCounterTrace(PredictionDataAddr);
      for (int i = 0; i < TreeLevel; i++)
      {
        if (i==0)
        {
          tree_engine.singleAdd(Ctr0,64,tree_engine.Counter_modify_list[i*2+1]);
        }
        else if(i==1)
        {
          tree_engine.singleAdd(CtrChain[0],32,tree_engine.Counter_modify_list[i*2+1]);
        }
        else{
          tree_engine.singleAdd(CtrChain[i-1],16,tree_engine.Counter_modify_list[i*2+1]);
        }
      }
    }

    /*
    this function will be used to update the entries after prefetching
    */
    void upadateCipherText(TableInx tInx, ChunkBlk CipherText)
    {
      auto i_itr = group.begin();
      for (size_t i = 0; i < tInx.groupInx; i++)
      {
        i_itr++;
      }
      auto j_itr = (*i_itr).TableEntry.begin();
      for (size_t i = 0; i < tInx.innerInx; i++)
      {
        j_itr++;
      }

      (*j_itr).CipherText_valid = true;
      (*j_itr).CipherText= CipherText;

    }

    void upadateCtr0(TableInx tInx, ChunkBlk Ctr0)
    {
      auto i_itr = group.begin();
      for (size_t i = 0; i < tInx.groupInx; i++)
      {
        i_itr++;
      }
      auto j_itr = (*i_itr).TableEntry.begin();
      for (size_t i = 0; i < tInx.innerInx; i++)
      {
        j_itr++;
      }

      (*j_itr).ctr0_valid = true;
      (*j_itr).ctr0_Caled = false;
      (*j_itr).ctr0 = Ctr0;
      (*i_itr).chunkCtr0 = Ctr0;
      (*i_itr).chunkCtr0Valid = true;
    }

    void upadataCtrChain(TableInx tInx, std::vector<ChunkBlk> CtrChain)
    {
      auto i_itr = group.begin();
      for (size_t i = 0; i < tInx.groupInx; i++)
      {
        i_itr++;
      }

      (*i_itr).CtrChain.assign(CtrChain.begin(),CtrChain.end());
      (*i_itr).CtrChainPreCaled = false;
      (*i_itr).CtrChainValid = true;
    }

    void upadateCipherText(TableInx tInx, uint8_t* _CipherText)
    {
      ChunkBlk CipherText;
      int ptr;
      tree_engine.array_2_bitset(_CipherText,64,CipherText,ptr);
      auto i_itr = group.begin();
      for (size_t i = 0; i < tInx.groupInx; i++)
      {
        i_itr++;
      }
      auto j_itr = (*i_itr).TableEntry.begin();
      for (size_t i = 0; i < tInx.innerInx; i++)
      {
        j_itr++;
      }

      (*j_itr).CipherText_valid = true;
      (*j_itr).CipherText= CipherText;

    }

    void upadateCtr0(TableInx tInx, uint8_t* _Ctr0)
    {
      ChunkBlk Ctr0;
      int ptr;
      tree_engine.array_2_bitset(_Ctr0,64,Ctr0,ptr);
      // std::cout<<"++++++++++++++++++++++++++  "<<Ctr0<<std::endl;
      auto i_itr = group.begin();
      for (size_t i = 0; i < tInx.groupInx; i++)
      {
        i_itr++;
      }
      auto j_itr = (*i_itr).TableEntry.begin();
      for (size_t i = 0; i < tInx.innerInx; i++)
      {
        j_itr++;
      }

      (*j_itr).ctr0_valid = true;
      (*j_itr).ctr0_Caled = true;
      (*j_itr).ctr0 = Ctr0;
      memcpy((*i_itr).blkCtr0,_Ctr0,64);
      (*i_itr).blkCtr0Valid = true;
    }

    void upadataCtrChain(TableInx tInx, vault& vault_engine)
    {
      std::vector<ChunkBlk> CtrChain;
      for (size_t i = 0; i < TreeLevel-1-2; i++)
      {
        ChunkBlk tmp;
        int ptr;
        vault_engine.array_2_bitset(vault_engine.counterOP[i+1].blk, 64, tmp, ptr);
        CtrChain.push_back(tmp);
      }
      ChunkBlk tmp0;
      ChunkBlk tmp1;
      int ptr;
      vault_engine.array_2_bitset(vault_engine.root2[vault_engine.Counter_modify_list[2*(TreeLevel-2)]], 64, tmp0, ptr);
      vault_engine.array_2_bitset(vault_engine.root, 64, tmp1, ptr);
      CtrChain.push_back(tmp0);
      CtrChain.push_back(tmp1);

      auto i_itr = group.begin();
      for (size_t i = 0; i < tInx.groupInx; i++)
      {
        i_itr++;
      }

      //---------------------------- wxr 10.8
      //we directly update all the ctr0
      ChunkBlk Ctr0;
      vault_engine.array_2_bitset(vault_engine.counterOP[0].blk, 64, Ctr0, ptr);

      for (auto j_itr = (*i_itr).TableEntry.begin(); j_itr != (*i_itr).TableEntry.end(); j_itr++)
      {
        if ((*j_itr).ctr0_valid)
        {
          (*j_itr).ctr0 = Ctr0;
          (*j_itr).ctr0_Caled = true;
        }
      }
      

      (*i_itr).CtrChain.assign(CtrChain.begin(),CtrChain.end());
      (*i_itr).CtrChainPreCaled = true;
      (*i_itr).CtrChainValid = true;
    }

    /*
    This function will parallelly calculation the table entries
    */
    void CalAllReadyContent(TableInx tInx, uint64_t mask, bool predictCounter=false)
    {
      auto i_itr = group.begin();
      for (size_t i = 0; i < tInx.groupInx; i++)
      {
        i_itr++;
      }

      if ((*i_itr).CtrChainValid)
      {
        (*i_itr).CtrChainPreCaled = true;
      }

      auto j_itr = (*i_itr).TableEntry.begin();
      for (size_t i = 0; i < tInx.innerInx; i++)
      {
        j_itr++;
      }
      (*j_itr).ctr0_Caled = true;
      (*j_itr).PadPreCaled = true;

      if(predictCounter)
        predictionCounters((*j_itr).addr & mask,(*j_itr).ctr0,(*i_itr).CtrChain);
    }


    /*
    this function will print all table
    */
    void print()
    {
      auto i_itr = group.begin();
      int ptr=0;
      std::cout<<std::endl;
      for (; i_itr != group.end(); i_itr++)
      {
        std::cout<<"----------------------------- group NUM: "<<ptr<<" CtrChain valid: "<<(*i_itr).CtrChainValid<<" CtrChain Caled: "<<(*i_itr).CtrChainPreCaled<<" StartAddr: "<<std::hex<<(*i_itr).Begin_Addr<<std::dec<<std::endl;
        (*i_itr).print();
        ptr ++;
      }
      std::cout<<std::endl;
    }

    ///////////////////////////////////////////
    //
    //main function, prediction. the steps are as follow
    //1. get the "data addr, ctr, ciphetext"
    //2. comprare them with tabel.
    //3, if find, directly jump this inspection
    //4. if miss, do prediction
    //5. generate the next Ctr0, ciphettext address and fetch
    //6. then directly merge this prediction results into table "update_Entry"
    //6.1 if the address is not in this group's range, then new a group with replacing machanism
    //6.2 if the address is in on group, just new a entry
    //
    ///////////////////////////////////////////


  };

  TeePrediction teePrediction;
  ////////////////////////////////////
  //
  //prediction var for keeping status
  //
  ////////////////////////////////////
  Addr preDataAddr;
  Addr preCtr0Addr;
  TableInx groupPreTInx;
  bool Ctr0_Cached;
  int prefetchCounter;
  int prefetchRecvCounter;

  bool predictingRetry; //flag the retry sending during the prediction

  Cycles ReadDataTransCycle;
  Addr ReadDataTransAddrRecord; 
  Addr WriteDataTransAddrRecord; 
  uint64_t PrefetchCycle;
  Cycles StartPreCalCycle=Cycles(0);

  
  class predictionRecord{
    public:
    std::queue<Addr> AddrRecord;
    std::queue<Addr> PreRecord;
    std::queue<uint64_t> CyclesRecord;

    predictionRecord()
    {
      for (size_t i = 0; i < 16; i++)
      {
        PreRecord.push(0);
      }
      for (size_t i = 0; i < 8; i++)
      {
        AddrRecord.push(0);
        CyclesRecord.push(0);
      }
    }

    void updateAddr(Addr addr, uint64_t cycle)
    {
      if (AddrRecord.size() == 8)
      {
        AddrRecord.pop();
        AddrRecord.push(addr);
      }
      if (CyclesRecord.size() == 8)
      {
        CyclesRecord.pop();
        CyclesRecord.push(cycle);
      }
    }

    void updatePreAddr(Addr addr)
    {
      if (PreRecord.size() == 16)
      {
        PreRecord.pop();
        PreRecord.push(addr);
      }
    }

    bool find()
    {
      bool tmp = false;
      for (size_t i = 0; i < 16; i++)
      {
        if ((AddrRecord.front() == PreRecord.front()) && (PreRecord.front() != 0))
        {
          tmp = true;
        }
        PreRecord.push(PreRecord.front());
        PreRecord.pop();
      }
      return tmp;
    }

    void print()
    {
      std::cout<<std::endl;
      for (size_t i = 0; i < 16; i++)
      {
        std::cout<<" "<<PreRecord.front();
        PreRecord.push(PreRecord.front());
        PreRecord.pop();
      }
      std::cout<<std::endl;
      for (size_t i = 0; i < 8; i++)
      {
        std::cout<<" "<<AddrRecord.front();
        AddrRecord.push(AddrRecord.front());
        AddrRecord.pop();
      }
      std::cout<<std::endl;
      for (size_t i = 0; i < 8; i++)
      {
        std::cout<<" "<<CyclesRecord.front();
        CyclesRecord.push(CyclesRecord.front());
        CyclesRecord.pop();
      }
      std::cout<<std::endl;
    }

    void clear()
    {
      while (!AddrRecord.empty())
      {
        AddrRecord.pop();
      }
      while (!PreRecord.empty())
      {
        PreRecord.pop();
      }
      while (!CyclesRecord.empty())
      {
        CyclesRecord.pop();
      }

      for (size_t i = 0; i < 16; i++)
      {
        PreRecord.push(0);
      }
      for (size_t i = 0; i < 8; i++)
      {
        AddrRecord.push(0);
        CyclesRecord.push(0);
      }
      
    }

    //--------------------------- wxr 10.9
    //designed for finding the write correct prediction addr
    bool exploreAddr(Addr addr)
    {
      bool tmp = false;
      for (size_t i = 0; i < 8; i++)
      {
        if ((AddrRecord.front() == addr))
        {
          tmp = true;
        }
        AddrRecord.push(AddrRecord.front());
        AddrRecord.pop();
      }
      return tmp;
    }

  };

  predictionRecord preRecord;
  
  predictionRecord readHistrory;

  //////////////////////////////////
  //
  //Reset cycles record
  //
  //////////////////////////////////
  std::vector<Cycles> ResetCycleRecord;
  bool findResetAddr(Addr addr, int& index)
  {
    for (size_t i = 0; i < DORESETTREErDpkt.size(); i++)
    {
      if (addr == DORESETTREErDpkt[i]->getAddr())
      {
        index = i;
        return true;
      }
    }
    return false;
  }


  /*
  this is lock same to OS
  */

  class muxLock
  {
    private:
    uint64_t lockTime;
    bool Lock=false;
    uint64_t source_id;

    public:
    muxLock()
    {
      Lock=false;
      source_id = 0;
    }

    void relase()
    {
      lockTime = 0;
      Lock = false;
    }
    bool lock(uint64_t curCycle, u_int64_t sid=0)
    {
      if (Lock)
      {
        return false;
      }
      Lock = true;
      lockTime = curCycle;
      source_id = sid;
      return true;
    }
    bool isLock(uint64_t source_id = 0)
    {
      return Lock && (source_id == this->source_id);
    }
    uint64_t LockTime(uint64_t curCycle)
    {
      return curCycle - lockTime;
    }
  };

  muxLock hashLock;


  class HashGroup{
    private:
      std::vector<muxLock> hashHardware;
      int N;

    public:
      HashGroup(int N)
      {
        //we init the group hash hardware, and attack them with a ID
        //this id can be used to find in teeTable to see if "table entry"
        //is kicked out from table.
        for (size_t i = 0; i < N; i++)
        {
          hashHardware.push_back(muxLock());
        }
        this->N = N;
      }

      bool isFull()
      {
        bool tmp = true;
        for (auto hash : hashHardware)
        {
          tmp = tmp&&hash.isLock();
        }
        return tmp;
      }

      bool isEmpty()
      {
        bool tmp = true;
        for (auto hash : hashHardware)
        {
          tmp = tmp&&(!hash.isLock());
        }
        return tmp;
      }

      int getBusyNum()
      {
        int tmp = 0;
        for (auto hash : hashHardware)
        {
          if (hash.isLock())
          {
            tmp++;
          }
        }
        return tmp;
      }

      bool isBusy(int id)
      {
        int tmp = 0;
        for (auto hash : hashHardware)
        {
          if (tmp == id)
          {
            return hash.isLock();
          }
          tmp++;
        }
      }

      uint64_t getIdReadyTime(int id, uint64_t curCycle)
      {
        int tmp = 0;
        for (auto hash : hashHardware)
        {
          if (tmp == id)
          {
            if (hash.LockTime(curCycle) >= hashC)
            {
              return 0;
            }
            else
            {
              return (hashC - hash.LockTime(curCycle));
            }
          }
          tmp++;
        }

      }

      uint64_t getReadyTime(uint64_t curCycle)
      {
        uint64_t minCycle=hashC + 1 ;
        int i = 0;
        for (auto hash : hashHardware)
        {
          if (hash.isLock())
          {
            uint64_t gapCycle = hash.LockTime(curCycle);
            if (gapCycle >= hashC)
            {
              minCycle = hashC;
            }
            else
            {
              if ( (hashC-gapCycle) < minCycle)
              {
                minCycle = (hashC-gapCycle);
              }
            }
          }
          else
          {
            minCycle = hashC;
          }
          i++;
        }
        return minCycle;
      }

      //return the id of the function
      int chooseFree()
      {
        if (isFull())
        {
          return -1;
        }
        int id = 0;
        for (auto hash : hashHardware)
        {
          if (!hash.isLock())
          {
            return id;
          }
          id ++;
        }
        return -1;
      }

      //update the hashhardware when allocate a new entry
      void updateNoneedHash(TeePrediction& teePrediction)
      {
        for (size_t i = 0; i < N; i++)
        {
          bool find;
          for (auto group=teePrediction.group.begin();group!=teePrediction.group.end();group++)
          {
            for (auto entry = (*group).TableEntry.begin(); entry!=(*group).TableEntry.end();entry++)
            {
              if ((*entry).hashId == i && !(*entry).CipherTextCal)
              {
                find = true;
              }
            }
          }
          if (!find)
          {
            if(hashHardware[i].isLock())
            {
              #if HASHDEBUG
              std::cout<<"----HASH  prune a hash id: "<<i<<std::endl;
              #endif
              hashHardware[i].relase();

            }
          }
        }
      }
      
      //this function could search tee table to attach a spare hashHardware to an entry waited
      //success return true otherwise false
      bool searchPTable2attach(TeePrediction& teePrediction, uint64_t nowTime)
      {
        //the closest entry has more prior
        // assert(!isFull());
        updateNoneedHash(teePrediction);

        int id=-1;
        uint64_t curCycle = 0;
        bool FindOne=false;
        for (auto group=teePrediction.group.begin();group!=teePrediction.group.end();group++)
        {
          for (auto entry = (*group).TableEntry.begin(); entry!=(*group).TableEntry.end();entry++)
          {
            if ((*entry).hashId == -1 && (*entry).CipherText_valid && (*entry).addr_valid)
            {
              if ((*entry).allocCycle > curCycle)
              {
                curCycle = (*entry).allocCycle;
              }
              FindOne = true;
            }
          }
        }
        //no entry need to be caled
        if (!FindOne)
        {
          return false;
        }
        //no free
        if (isFull())
        {
          return false;
        }
        
        for (auto group=teePrediction.group.begin();group!=teePrediction.group.end();group++)
        {
          for (auto entry = (*group).TableEntry.begin(); entry!=(*group).TableEntry.end();entry++)
          {
            if ((*entry).allocCycle == curCycle)
            {
              id = chooseFree();
              (*entry).hashId = id;
            }
          }
        }
        assert(id != -1);

        //then we lock this hardware
        hashHardware[id].lock(nowTime);
        #if HASHDEBUG
        std::cout<<"----HASH a table entry get locked num: "<<id<<std::endl;
        // teePrediction.print();
        #endif
        return true;
      }

      //in this function, we realse all lock when they're in time
      void release(TeePrediction& teePrediction, uint64_t curCycle)
      {
        for (size_t i = 0; i < N; i++)
        {
          if (hashHardware[i].isLock())
          {
            #if HASHDEBUG
            std::cout<<"----HASH lockTime: "<<hashHardware[i].LockTime(curCycle)<<std::endl;
            #endif
            if (hashHardware[i].LockTime(curCycle) >= hashC)
            {
              hashHardware[i].relase();
              for (auto group=teePrediction.group.begin();group!=teePrediction.group.end();group++)
              {
                for (auto entry = (*group).TableEntry.begin(); entry!=(*group).TableEntry.end();entry++)
                {
                  #if HASHDEBUG
                  std::cout<<" "<<(*entry).hashId<<std::endl;
                  #endif
                  if ((*entry).hashId == i && !(*entry).CipherTextCal)
                  {
                    assert(!(*entry).CipherTextCal);
                    (*entry).CipherTextCal = true;
                    #if HASHDEBUG
                    std::cout<<"----HASH relase a hashHardware: "<<i<<std::endl;
                    #endif
                  }
                }
              }
            }
          }
        }
      }


  };

  HashGroup hashGroup;

  class gapPrediction{
    public:
    int historyNum=8;

    std::vector<uint64_t> prefetchVec;
    std::vector<uint64_t> gapVec;

    gapPrediction(int historyNum)
    {
      this->historyNum = historyNum;
      for (size_t i = 0; i < historyNum; i++)
      {
        prefetchVec.push_back(0);
        gapVec.push_back(0);
      }
    }

    void updatePrefetch(uint64_t cycle)
    {
      for (size_t i = historyNum-1; i > 0; i--)
      {
        prefetchVec[i] = prefetchVec[i-1];
      }
      prefetchVec[0] = cycle;
    }

    void updateGap(uint64_t cycle)
    {
      for (size_t i = historyNum-1; i > 0; i--)
      {
        gapVec[i] = gapVec[i-1];
      }
      gapVec[0] = cycle;
    }

    uint64_t getAvgGap()
    {
      uint64_t sum=0;
      int num = 1;
      for (size_t i = 0; i < historyNum; i++)
      {
        if (gapVec[i]!=0)
        {
          sum += gapVec[i];
          if (num!=1)
          {
            num ++;
          }
        }
      }
      return sum/num;
    }

    uint64_t getAvgPrefetch()
    {
      uint64_t sum=0;
      for (size_t i = 0; i < historyNum; i++)
      {
        sum += prefetchVec[i];
      }
      return sum/historyNum;
    }

    bool cmp(int threhold)
    {
      int count=0;
      for (size_t i = 0; i < historyNum; i++)
      {
        if (gapVec[i]>prefetchVec[i])
        {
          count ++;
        }
        // std::cout<<"; "<<gapVec[i]<<" "<<prefetchVec[i];
      }
      // std::cout<<" count: "<<count<<std::endl;

      if (count>=threhold)
      {
        return true;
      }
      else
      {
        return false;
      }
    }

    bool cmp()
    {
      for (size_t i = 0; i < historyNum; i++)
      {
        if (gapVec[i]!=0)
        {
          if (gapVec[i]<=prefetchVec[i])
          {
            return false;
          }
        }
      }
      return true;
    }

  };

  gapPrediction gapPre;

  //read data record
  uint8_t readDataRecord[64];
  bool isChecking=false;
  uint64_t overlapHashRDMAC;
  bool hasResponseRead;
  uint64_t preCycle = 0;

  uint64_t equal0=0;
  uint64_t equalSame=0;

  uint8_t LastWrite[64]={0};
  uint8_t LastRead[64]={0};

  uint64_t Lat_cycle=0;

  bool noNeedTreeCheck = false;
  bool noNeedWTreeCheck = false;
  int writeHashNum  = 0;
  int reduceNum= 0;
  //wxr----------------------
  protected:

    // For now, make use of a queued response port to avoid dealing with
    // flow control for the responses being sent back
    class MemoryPort : public QueuedResponsePort
    {

        RespPacketQueue queue;
        MemCtrl& ctrl;

      public:

        MemoryPort(const std::string& name, MemCtrl& _ctrl);
        void disableSanityCheck();

      protected:

        Tick recvAtomic(PacketPtr pkt) override;
        Tick recvAtomicBackdoor(
                PacketPtr pkt, MemBackdoorPtr &backdoor) override;

        void recvFunctional(PacketPtr pkt) override;

        bool recvTimingReq(PacketPtr) override;

        AddrRangeList getAddrRanges() const override;

    };

    /**
     * Our incoming port, for a multi-ported controller add a crossbar
     * in front of it
     */
    MemoryPort port;

    /**
     * Remember if the memory system is in timing mode
     */
    bool isTimingMode;

    /**
     * Remember if we have to retry a request when available.
     */
    bool retryRdReq;
    bool retryWrReq;

    /**
     * Bunch of things requires to setup "events" in gem5
     * When event "respondEvent" occurs for example, the method
     * processRespondEvent is called; no parameters are allowed
     * in these methods
     */
    virtual void processNextReqEvent(MemInterface* mem_intr,
                          MemPacketQueue& resp_queue,
                          EventFunctionWrapper& resp_event,
                          EventFunctionWrapper& next_req_event,
                          bool& retry_wr_req);
    EventFunctionWrapper nextReqEvent;

    virtual void processRespondEvent(MemInterface* mem_intr,
                        MemPacketQueue& queue,
                        EventFunctionWrapper& resp_event,
                        bool& retry_rd_req);
    EventFunctionWrapper respondEvent;

    ////////////////////////////////////////
    //
    //The next request event of own enclave event.
    //
    ////////////////////////////////////////

    EventFunctionWrapper EncEvent;

    ////////////////////////////////////////
    //
    //The event is used for hashGroup
    //
    ///////////////////////////////////////
    EventFunctionWrapper hashHardwareEvent;

    /**
     * Check if the read queue has room for more entries
     *
     * @param pkt_count The number of entries needed in the read queue
     * @return true if read queue is full, false otherwise
     */
    bool readQueueFull(unsigned int pkt_count) const;

    /**
     * Check if the write queue has room for more entries
     *
     * @param pkt_count The number of entries needed in the write queue
     * @return true if write queue is full, false otherwise
     */
    bool writeQueueFull(unsigned int pkt_count) const;

    /**
     * When a new read comes in, first check if the write q has a
     * pending request to the same address.\ If not, decode the
     * address to populate rank/bank/row, create one or mutliple
     * "mem_pkt", and push them to the back of the read queue.\
     * If this is the only
     * read request in the system, schedule an event to start
     * servicing it.
     *
     * @param pkt The request packet from the outside world
     * @param pkt_count The number of memory bursts the pkt
     * @param mem_intr The memory interface this pkt will
     * eventually go to
     * @return if all the read pkts are already serviced by wrQ
     */
    bool addToReadQueue(PacketPtr pkt, unsigned int pkt_count,
                        MemInterface* mem_intr);

    /**
     * Decode the incoming pkt, create a mem_pkt and push to the
     * back of the write queue. \If the write q length is more than
     * the threshold specified by the user, ie the queue is beginning
     * to get full, stop reads, and start draining writes.
     *
     * @param pkt The request packet from the outside world
     * @param pkt_count The number of memory bursts the pkt
     * @param mem_intr The memory interface this pkt will
     * eventually go to
     */
    void addToWriteQueue(PacketPtr pkt, unsigned int pkt_count,
                         MemInterface* mem_intr);

    /**
     * Actually do the burst based on media specific access function.
     * Update bus statistics when complete.
     *
     * @param mem_pkt The memory packet created from the outside world pkt
     * @param mem_intr The memory interface to access
     * @return Time when the command was issued
     *
     */
    virtual Tick doBurstAccess(MemPacket* mem_pkt, MemInterface* mem_intr);

    /**
     * When a packet reaches its "readyTime" in the response Q,
     * use the "access()" method in AbstractMemory to actually
     * create the response packet, and send it back to the outside
     * world requestor.
     *
     * @param pkt The packet from the outside world
     * @param static_latency Static latency to add before sending the packet
     * @param mem_intr the memory interface to access
     */
    virtual void accessAndRespond(PacketPtr pkt, Tick static_latency,
                                                MemInterface* mem_intr);

    /**
     * Determine if there is a packet that can issue.
     *
     * @param pkt The packet to evaluate
     */
    virtual bool packetReady(MemPacket* pkt, MemInterface* mem_intr);

    /**
     * Calculate the minimum delay used when scheduling a read-to-write
     * transision.
     * @param return minimum delay
     */
    virtual Tick minReadToWriteDataGap();

    /**
     * Calculate the minimum delay used when scheduling a write-to-read
     * transision.
     * @param return minimum delay
     */
    virtual Tick minWriteToReadDataGap();

    /**
     * The memory schduler/arbiter - picks which request needs to
     * go next, based on the specified policy such as FCFS or FR-FCFS
     * and moves it to the head of the queue.
     * Prioritizes accesses to the same rank as previous burst unless
     * controller is switching command type.
     *
     * @param queue Queued requests to consider
     * @param extra_col_delay Any extra delay due to a read/write switch
     * @param mem_intr the memory interface to choose from
     * @return an iterator to the selected packet, else queue.end()
     */
    virtual MemPacketQueue::iterator chooseNext(MemPacketQueue& queue,
        Tick extra_col_delay, MemInterface* mem_intr);

    /**
     * For FR-FCFS policy reorder the read/write queue depending on row buffer
     * hits and earliest bursts available in memory
     *
     * @param queue Queued requests to consider
     * @param extra_col_delay Any extra delay due to a read/write switch
     * @return an iterator to the selected packet, else queue.end()
     */
    virtual std::pair<MemPacketQueue::iterator, Tick>
    chooseNextFRFCFS(MemPacketQueue& queue, Tick extra_col_delay,
                    MemInterface* mem_intr);

    /**
     * Calculate burst window aligned tick
     *
     * @param cmd_tick Initial tick of command
     * @return burst window aligned tick
     */
    Tick getBurstWindow(Tick cmd_tick);

    /**
     * Used for debugging to observe the contents of the queues.
     */
    void printQs() const;

    /**
     * Burst-align an address.
     *
     * @param addr The potentially unaligned address
     * @param mem_intr The DRAM interface this pkt belongs to
     *
     * @return An address aligned to a memory burst
     */
    virtual Addr burstAlign(Addr addr, MemInterface* mem_intr) const;

    /**
     * Check if mem pkt's size is sane
     *
     * @param mem_pkt memory packet
     * @param mem_intr memory interface
     * @return An address aligned to a memory burst
     */
    virtual bool pktSizeCheck(MemPacket* mem_pkt,
                              MemInterface* mem_intr) const;

    /**
     * The controller's main read and write queues,
     * with support for QoS reordering
     */
    std::vector<MemPacketQueue> readQueue;
    std::vector<MemPacketQueue> writeQueue;

    /**
     * To avoid iterating over the write queue to check for
     * overlapping transactions, maintain a set of burst addresses
     * that are currently queued. Since we merge writes to the same
     * location we never have more than one address to the same burst
     * address.
     */
    std::unordered_set<Addr> isInWriteQueue;

    /**
     * Response queue where read packets wait after we're done working
     * with them, but it's not time to send the response yet. The
     * responses are stored separately mostly to keep the code clean
     * and help with events scheduling. For all logical purposes such
     * as sizing the read queue, this and the main read queue need to
     * be added together.
     */
    std::deque<MemPacket*> respQueue;

    /**
     * Holds count of commands issued in burst window starting at
     * defined Tick. This is used to ensure that the command bandwidth
     * does not exceed the allowable media constraints.
     */
    std::unordered_multiset<Tick> burstTicks;

    /**
+    * Create pointer to interface of the actual memory media when connected
+    */
    MemInterface* dram;

    virtual AddrRangeList getAddrRanges();

    /**
     * The following are basic design parameters of the memory
     * controller, and are initialized based on parameter values.
     * The rowsPerBank is determined based on the capacity, number of
     * ranks and banks, the burst size, and the row buffer size.
     */
    uint32_t readBufferSize;
    uint32_t writeBufferSize;
    uint32_t writeHighThreshold;
    uint32_t writeLowThreshold;
    const uint32_t minWritesPerSwitch;
    const uint32_t minReadsPerSwitch;
    uint32_t writesThisTime;
    uint32_t readsThisTime;

    /**
     * Memory controller configuration initialized based on parameter
     * values.
     */
    enums::MemSched memSchedPolicy;

    /**
     * Pipeline latency of the controller frontend. The frontend
     * contribution is added to writes (that complete when they are in
     * the write buffer) and reads that are serviced the write buffer.
     */
    const Tick frontendLatency;

    /**
     * Pipeline latency of the backend and PHY. Along with the
     * frontend contribution, this latency is added to reads serviced
     * by the memory.
     */
    const Tick backendLatency;

    /**
     * Length of a command window, used to check
     * command bandwidth
     */
    const Tick commandWindow;

    /**
     * Till when must we wait before issuing next RD/WR burst?
     */
    Tick nextBurstAt;

    Tick prevArrival;

    /**
     * The soonest you have to start thinking about the next request
     * is the longest access time that can occur before
     * nextBurstAt. Assuming you need to precharge, open a new row,
     * and access, it is tRP + tRCD + tCL.
     */
    Tick nextReqTime;

    struct CtrlStats : public statistics::Group
    {
        CtrlStats(MemCtrl &ctrl);

        void regStats() override;

        MemCtrl &ctrl;

        // All statistics that the model needs to capture
        statistics::Scalar readReqs;
        statistics::Scalar writeReqs;
        statistics::Scalar readBursts;
        statistics::Scalar writeBursts;
        statistics::Scalar servicedByWrQ;
        statistics::Scalar mergedWrBursts;
        statistics::Scalar neitherReadNorWriteReqs;
        // Average queue lengths
        statistics::Average avgRdQLen;
        statistics::Average avgWrQLen;

        statistics::Scalar numRdRetry;
        statistics::Scalar numWrRetry;
        statistics::Vector readPktSize;
        statistics::Vector writePktSize;
        statistics::Vector rdQLenPdf;
        statistics::Vector wrQLenPdf;
        statistics::Histogram rdPerTurnAround;
        statistics::Histogram wrPerTurnAround;

        statistics::Scalar bytesReadWrQ;
        statistics::Scalar bytesReadSys;
        statistics::Scalar bytesWrittenSys;
        // Average bandwidth
        statistics::Formula avgRdBWSys;
        statistics::Formula avgWrBWSys;

        statistics::Scalar totGap;
        statistics::Formula avgGap;

        // per-requestor bytes read and written to memory
        statistics::Vector requestorReadBytes;
        statistics::Vector requestorWriteBytes;

        // per-requestor bytes read and written to memory rate
        statistics::Formula requestorReadRate;
        statistics::Formula requestorWriteRate;

        // per-requestor read and write serviced memory accesses
        statistics::Vector requestorReadAccesses;
        statistics::Vector requestorWriteAccesses;

        // per-requestor read and write total memory access latency
        statistics::Vector requestorReadTotalLat;
        statistics::Vector requestorWriteTotalLat;

        // per-requestor raed and write average memory access latency
        statistics::Formula requestorReadAvgLat;
        statistics::Formula requestorWriteAvgLat;
        
        ////////////////////////////
        //
        //for enc
        //
        ////////////////////////////
        //+++++++++++++++++++++++++++++++++ wxr
        //exception situtation record
        statistics::Scalar EncPreNotEqual;
        

        //basic status
        statistics::Scalar EncReadReq;
        statistics::Scalar EncWriteReq;
        statistics::Scalar EncTotGap;
        statistics::Formula EncAvgTotGap;
        statistics::Formula EncTotTime;

        statistics::Scalar EncEncryptTime;
        statistics::Scalar EncDecryptTime;
        statistics::Scalar EncHasUpdata;
        statistics::Scalar EncCacheAccessTime;

        statistics::Scalar EncHashOverlapRDMAC;

        statistics::Scalar EncBeginCycle;
        statistics::Scalar EncEndCycle;

        statistics::Scalar EncTotInst;
        statistics::Scalar EncEndTimeOut;
        statistics::Scalar EncBeginTimeout;

        //enc cache status
        statistics::Scalar EncCacheRd;
        statistics::Scalar EncCacheWr;
        statistics::Scalar EncCacheRd_hit;
        statistics::Scalar EncCacheWr_hit;
        statistics::Formula EncCache_hitRate;

        //mac cache status
        statistics::Scalar MacCacheRd;
        statistics::Scalar MacCacheWr;
        statistics::Scalar MacCacheRd_hit;
        statistics::Scalar MacCacheWr_hit;
        statistics::Formula MacCache_hitRate;

        //different cycles
        statistics::Scalar EncRDCNTC;
        statistics::Scalar EncWBLIMB;

        statistics::Scalar EncReset;
        statistics::Scalar EncReset_cycle;
        statistics::Scalar EncReset_Mem_RdTime;
        statistics::Scalar EncReset_Mem_WrTime;
        statistics::Scalar EncReset_Mem_HashTime;
        statistics::Scalar EncReset_Mem_EncryptTime;
        statistics::Scalar EncReset_Mem_DecryptTime;
        statistics::Scalar EncLimbUpdata;

        //////////////////////
        //
        //consider the parallel proccessing, the bias will be added
        //
        //////////////////////
        statistics::Scalar EncReadAesBias;
        statistics::Scalar EncPrefetchHashOverlap;

        //prediction status
        statistics::Scalar EncPredictionSuccess;
        statistics::Scalar EncPredictionRecvNew;
        statistics::Scalar EncPredictionSaveAes;
        statistics::Scalar EncPredictionAesSuccess;
        statistics::Scalar EncPredictionNorAesSuccess;
        statistics::Scalar EncPredictionSuccessTree;
        statistics::Scalar EncPrefetchWaitport;

        //this is for write prediction
        statistics::Scalar EncWritePredictionNoneed;
        statistics::Scalar EncPredictionWriteSuccess;
        statistics::Scalar EncWritePredictionBias;
        statistics::Formula EncSuccessRate;

        //counte reset situation
        statistics::Scalar EncResetAesBias;     
        statistics::Scalar EncResetHashBias;

        statistics::Scalar EncReadAllLatency;
        statistics::Scalar EncL0Reset;
        //hash group status
        statistics::Scalar EncHashGroupNum;
        statistics::Scalar EncHashGroupBusyNum;
        statistics::Scalar EncHashGroupBusyMax;
        statistics::Formula EncHashGroupAvg;

        statistics::Scalar EncPrefetchCycle;
        statistics::Scalar EncPrefetchTime;
        statistics::Scalar EncPreWaitCaled; //when find the TeePredictionTable, the tags all matched but the result is caling

        statistics::Scalar EncPrefetchGapPreTime;

        //we set this tag to see the every level node miss
        //16GB ==> (256MB; 8MB; 512KB; 32KB; 2KB;)->off-chip (128B; 64B)->on-chip
        statistics::Scalar EncTreeL0ReadTime;
        statistics::Scalar EncTreeL0MissTime;
        statistics::Scalar EncTreeL1ReadTime;
        statistics::Scalar EncTreeL1MissTime;
        statistics::Scalar EncTreeL2ReadTime;
        statistics::Scalar EncTreeL2MissTime;
        statistics::Scalar EncTreeL3ReadTime;
        statistics::Scalar EncTreeL3MissTime;
        statistics::Scalar EncTreeL4ReadTime;
        statistics::Scalar EncTreeL4MissTime;
        statistics::Scalar EncTreeNoCheck;

        statistics::Scalar EncWTreeL0ReadTime;
        statistics::Scalar EncWTreeL0MissTime;
        statistics::Scalar EncWTreeL1ReadTime;
        statistics::Scalar EncWTreeL1MissTime;
        statistics::Scalar EncWTreeL2ReadTime;
        statistics::Scalar EncWTreeL2MissTime;
        statistics::Scalar EncWTreeL3ReadTime;
        statistics::Scalar EncWTreeL3MissTime;
        statistics::Scalar EncWTreeL4ReadTime;
        statistics::Scalar EncWTreeL4MissTime;
        statistics::Scalar EncWTreeNoCheck;

        statistics::Scalar EncWTree2HASHTIME;
        statistics::Scalar EncWriteHashTime;
        //--------------------------------- wxr
        
    };

    CtrlStats stats;

    void EncS2stats()
    {

      stats.EncPreNotEqual = EncS.EncPreNotEqual;
      

      
      stats.EncReadReq = EncS.EncReadReq;
      stats.EncWriteReq = EncS.EncWriteReq;
      stats.EncTotGap = EncS.EncTotGap;

      stats.EncEncryptTime = EncS.EncEncryptTime*encryptC;
      stats.EncDecryptTime = EncS.EncDecryptTime*encryptC;
      stats.EncHasUpdata = EncS.EncHasUpdata;
      stats.EncCacheAccessTime = EncS.EncCacheAccessTime*cacheC;

      stats.EncHashOverlapRDMAC = EncS.EncHashOverlapRDMAC;

      stats.EncEndCycle = curCycle();

      
      stats.EncCacheRd = EncS.EncCacheRd;
      stats.EncCacheWr = EncS.EncCacheWr;
      stats.EncCacheRd_hit = EncS.EncCacheRd_hit;
      stats.EncCacheWr_hit = EncS.EncCacheWr_hit;
      
      stats.MacCacheRd = EncS.MacCacheRd;
      stats.MacCacheWr = EncS.MacCacheWr;
      stats.MacCacheRd_hit = EncS.MacCacheRd_hit;
      stats.MacCacheWr_hit = EncS.MacCacheWr_hit;

      
      stats.EncRDCNTC = EncS.EncRDCNTC;
      stats.EncWBLIMB = EncS.EncWBLIMB;

      stats.EncReset = EncS.EncReset;
      stats.EncReset_cycle = EncS.EncReset_cycle;
      stats.EncReset_Mem_RdTime = EncS.EncReset_Mem_RdTime;
      stats.EncReset_Mem_WrTime = EncS.EncReset_Mem_WrTime;
      stats.EncReset_Mem_HashTime = EncS.EncReset_Mem_HashTime*hashC;
      stats.EncReset_Mem_EncryptTime = EncS.EncReset_Mem_EncryptTime*encryptC;
      stats.EncReset_Mem_DecryptTime = EncS.EncReset_Mem_DecryptTime*encryptC;
      stats.EncLimbUpdata = EncS.EncLimbUpdata;

      
      
      stats.EncReadAesBias = EncS.EncReadAesBias;
      stats.EncPrefetchHashOverlap = EncS.EncPrefetchHashOverlap;

      
      stats.EncPredictionSuccess = EncS.EncPredictionSuccess;
      stats.EncPredictionRecvNew = EncS.EncPredictionRecvNew;
      stats.EncPredictionSaveAes = EncS.EncPredictionSaveAes;
      stats.EncPredictionAesSuccess= EncS.EncPredictionAesSuccess;
      stats.EncPredictionNorAesSuccess= EncS.EncPredictionNorAesSuccess;
      stats.EncPredictionSuccessTree = EncS.EncPredictionSuccessTree;
      stats.EncPrefetchWaitport = EncS.EncPrefetchWaitPort;
      stats.EncPrefetchCycle = EncS.EncPrefetchCycle;

      stats.EncWritePredictionNoneed = EncS.EncWritePredictionNoneed;
      stats.EncPredictionWriteSuccess = EncS.EncPredictionWriteSuccess;
      stats.EncWritePredictionBias = EncS.EncWritePredictionBias;

      stats.EncResetAesBias = EncS.EncResetAesBias;     
      stats.EncResetHashBias = EncS.EncResetHashBias;

      
      stats.EncHashGroupNum = EncS.EncHashGroupNum;
      stats.EncHashGroupBusyNum = EncS.EncHashGroupBusyNum;
      stats.EncHashGroupBusyMax= EncS.EncHashGroupBusyMax;

      stats.EncPrefetchGapPreTime = EncS.EncPrefetchGapPreTime;
    }


    void cache2EncS()
    {
      EncS.EncCacheRd = encCache.EncS.tRD;
      EncS.EncCacheWr = encCache.EncS.tWR;
      EncS.EncCacheRd_hit = encCache.EncS.tRDhit;
      EncS.EncCacheWr_hit = encCache.EncS.tWRhit;

      EncS.MacCacheRd = macCache.EncS.tRD;
      EncS.MacCacheWr = macCache.EncS.tWR;
      EncS.MacCacheRd_hit = macCache.EncS.tRDhit;
      EncS.MacCacheWr_hit = macCache.EncS.tWRhit;
    }

    /**
     * Upstream caches need this packet until true is returned, so
     * hold it for deletion until a subsequent call
     */
    std::unique_ptr<Packet> pendingDelete;

    /**
     * Select either the read or write queue
     *
     * @param is_read The current burst is a read, select read queue
     * @return a reference to the appropriate queue
     */
    std::vector<MemPacketQueue>& selQueue(bool is_read)
    {
        return (is_read ? readQueue : writeQueue);
    };

    virtual bool respQEmpty()
    {
        return respQueue.empty();
    }

    /**
     * Checks if the memory interface is already busy
     *
     * @param mem_intr memory interface to check
     * @return a boolean indicating if memory is busy
     */
    virtual bool memBusy(MemInterface* mem_intr);

    /**
     * Will access memory interface and select non-deterministic
     * reads to issue
     * @param mem_intr memory interface to use
     */
    virtual void nonDetermReads(MemInterface* mem_intr);

    /**
     * Will check if all writes are for nvm interface
     * and nvm's write resp queue is full. The generic mem_intr is
     * used as the same function can be called for a dram interface,
     * in which case dram functions will eventually return false
     * @param mem_intr memory interface to use
     * @return a boolean showing if nvm is blocked with writes
     */
    virtual bool nvmWriteBlock(MemInterface* mem_intr);

    /**
     * Remove commands that have already issued from burstTicks
     */
    virtual void pruneBurstTick();

  public:

    MemCtrl(const MemCtrlParams &p);

    /**
     * Ensure that all interfaced have drained commands
     *
     * @return bool flag, set once drain complete
     */
    virtual bool allIntfDrained() const;

    DrainState drain() override;

    /**
     * Check for command bus contention for single cycle command.
     * If there is contention, shift command to next burst.
     * Check verifies that the commands issued per burst is less
     * than a defined max number, maxCommandsPerWindow.
     * Therefore, contention per cycle is not verified and instead
     * is done based on a burst window.
     *
     * @param cmd_tick Initial tick of command, to be verified
     * @param max_cmds_per_burst Number of commands that can issue
     *                           in a burst window
     * @return tick for command issue without contention
     */
    virtual Tick verifySingleCmd(Tick cmd_tick, Tick max_cmds_per_burst,
                                bool row_cmd);

    /**
     * Check for command bus contention for multi-cycle (2 currently)
     * command. If there is contention, shift command(s) to next burst.
     * Check verifies that the commands issued per burst is less
     * than a defined max number, maxCommandsPerWindow.
     * Therefore, contention per cycle is not verified and instead
     * is done based on a burst window.
     *
     * @param cmd_tick Initial tick of command, to be verified
     * @param max_multi_cmd_split Maximum delay between commands
     * @param max_cmds_per_burst Number of commands that can issue
     *                           in a burst window
     * @return tick for command issue without contention
     */
    virtual Tick verifyMultiCmd(Tick cmd_tick, Tick max_cmds_per_burst,
                        Tick max_multi_cmd_split = 0);

    /**
     * Is there a respondEvent scheduled?
     *
     * @return true if event is scheduled
     */
    virtual bool respondEventScheduled(uint8_t pseudo_channel = 0) const
    {
        assert(pseudo_channel == 0);
        return respondEvent.scheduled();
    }

    /**
     * Is there a read/write burst Event scheduled?
     *
     * @return true if event is scheduled
     */
    virtual bool requestEventScheduled(uint8_t pseudo_channel = 0) const
    {
        assert(pseudo_channel == 0);
        return nextReqEvent.scheduled();
    }

    /**
     * restart the controller
     * This can be used by interfaces to restart the
     * scheduler after maintainence commands complete
     * @param Tick to schedule next event
     * @param pseudo_channel pseudo channel number for which scheduler
     * needs to restart, will always be 0 for controllers which control
     * only a single channel
     */
    virtual void restartScheduler(Tick tick, uint8_t pseudo_channel = 0)
    {
        assert(pseudo_channel == 0);
        schedule(nextReqEvent, tick);
    }

    /**
     * Check the current direction of the memory channel
     *
     * @param next_state Check either the current or next bus state
     * @return True when bus is currently in a read state
     */
    bool inReadBusState(bool next_state) const;

    /**
     * Check the current direction of the memory channel
     *
     * @param next_state Check either the current or next bus state
     * @return True when bus is currently in a write state
     */
    bool inWriteBusState(bool next_state) const;

    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

    virtual void init() override;
    virtual void startup() override;
    virtual void drainResume() override;

  protected:

    virtual Tick recvAtomic(PacketPtr pkt);
    virtual Tick recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &backdoor);
    virtual void recvFunctional(PacketPtr pkt);
    virtual bool recvTimingReq(PacketPtr pkt);

    bool recvFunctionalLogic(PacketPtr pkt, MemInterface* mem_intr);
    Tick recvAtomicLogic(PacketPtr pkt, MemInterface* mem_intr);

};

} // namespace memory
} // namespace gem5

#endif //__MEM_CTRL_HH__
