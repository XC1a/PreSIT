/*
 * @Author: Xinrui Wang 
 * @Date: 2023-04-04 12:27:49 
 * @Last Modified by: Xinrui Wang
 * @Last Modified time: 2023-04-06 18:05:44
 */

//this file provide a library class
//to handle kinds of encrytions
#ifndef ENCRYPT_HH
#define ENCRYPT_HH

#include <string>
#include <vector>

namespace gem5{

class AES
{
//now this class only support AES128
private:
    int delay_cycle=80;
public:
    typedef unsigned char    byte;
    static const int KEY_SIZE = 16;    //    密钥长度为128位
    static const int N_ROUND = 11;
    byte plainText[16];    //    明文
    byte state[16];    //    当前分组。
    byte cipherKey[16];    //    密钥
    byte roundKey[N_ROUND][16];    //轮密钥
    byte cipherText[16];    //密文
    byte SBox[16][16];    //    S盒
    byte InvSBox[16][16];    //    逆S盒   
    void EncryptionProcess();   
    void DecryptionProcess();
    void Round(const int& round);
    void InvRound(const int& round);
    void FinalRound();
    void InvFinalRound();
    void KeyExpansion();
    void AddRoundKey(const int& round);   
    void SubBytes();   
    void InvSubBytes();
    void ShiftRows();   
    void InvShiftRows();
    void MixColumns();   
    void InvMixColumns();
    void BuildSBox();
    void BuildInvSBox();
    void InitialState(const byte* text);
    void InitialCipherText();   
    void InitialplainText();       
    byte GFMultplyByte(const byte& left, const byte& right);
    const byte* GFMultplyBytesMatrix(const byte* left, const byte* right);
public:   
    AES();   
    AES(int delay);   
    const byte* Cipher(byte* text, byte* key, const int& keySize);   
    const byte* InvCipher(byte* text, byte* key, const int& keySize);
    void Cipher512(byte* text_src, byte* text_dst ,byte* key, const int& keySize);   
    void InvCipher512(byte* text_src, byte* text_dst, byte* key, const int& keySize);
};


class Sha256 {
//getInstance.getHexMessageDigest() to hash
public:
    //! 获取单例
    inline static Sha256 &getInstance()
    {
        static Sha256 instance;
        return instance;
    }

    /** 
     * @brief: 使用SHA256算法，加密输入信息（获取数字指纹）
     * @param[in] message: 输入信息
     * @return: 摘要（数字指纹）
    */
    std::vector<uint8_t> encrypt(std::vector<uint8_t> message) const;

    /** 
     * @brief: 获取十六进制表示的信息摘要（数字指纹）
     * @param[in] message: 输入信息
     * @return: 十六进制表示的信息摘要（数字指纹）
    */
    std::string getHexMessageDigest(const std::string &message) const;

protected:
    /// SHA256算法中定义的6种逻辑运算 ///

    inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) const noexcept
    {
        return (x & y) ^ ((~x) & z);
    }

    inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) const noexcept
    {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    inline uint32_t bigSigma0(uint32_t x) const noexcept
    {
        return (x >> 2 | x << 30) ^ (x >> 13 | x << 19) ^ (x >> 22 | x << 10);
    }

    inline uint32_t bigSigma1(uint32_t x) const noexcept
    {
        return (x >> 6 | x << 26) ^ (x >> 11 | x << 21) ^ (x >> 25 | x << 7);
    }

    inline uint32_t smallSigma0(uint32_t x) const noexcept
    {
        return (x >> 7 | x << 25) ^ (x >> 18 | x << 14) ^ (x >> 3);
    }

    inline uint32_t smallSigma1(uint32_t x) const noexcept
    {
        return (x >> 17 | x << 15) ^ (x >> 19 | x << 13) ^ (x >> 10);
    }

    /** 
     * @brief: SHA256算法对输入信息的预处理，包括“附加填充比特”和“附加长度值”
            附加填充比特: 在报文末尾进行填充，先补第一个比特为1，然后都补0，直到长度满足对512取模后余数是448。需要注意的是，信息必须进行填充。
            附加长度值: 用一个64位的数据来表示原始消息（填充前的消息）的长度，并将其补到已经进行了填充操作的消息后面。
     * @param[in][out] message: 待处理的信息
    */
    void preprocessing(std::vector<uint8_t> &message) const;

    /** 
     * @brief: 将信息分解成连续的64Byte大小的数据块
     * @param[in] message: 输入信息，长度为64Byte的倍数
     * @return: 输出数据块
    */
    std::vector<std::vector<uint8_t>> breakTextInto64ByteChunks(const std::vector<uint8_t> &message) const;

    /** 
     * @brief: 由64Byte大小的数据块，构造出64个4Byte大小的字。
            构造算法：前16个字直接由数据块分解得到，其余的字由如下迭代公式得到：
            W[t] = smallSigma1(W[t-2]) + W[t-7] + smallSigma0(W[t-15]) + W[t-16]
     * @param[in] chunk: 输入数据块，大小为64Byte
     * @return: 输出字
    */
    std::vector<uint32_t> structureWords(const std::vector<uint8_t> &chunk) const;

    /** 
     * @breif: 基于64个4Byte大小的字，进行64次循环加密
     * @param[in] words: 64个4Byte大小的字
     * @param[in][out] message_digest: 信息摘要
    */
    void transform(const std::vector<uint32_t> &words, std::vector<uint32_t> &message_digest) const;

    /** 
     * @brief: 输出最终的哈希值（数字指纹）
     * @param[in] input: 步长为32bit的哈希值
     * @return: 步长为8bit的哈希值
    */
    std::vector<uint8_t> produceFinalHashValue(const std::vector<uint32_t> &input) const;

private:
    /* 单例模式 */
    Sha256() = default;

    Sha256(const Sha256 &) = delete;
    Sha256 &operator=(const Sha256 &) = delete;

    Sha256(Sha256 &&) = delete;
    Sha256 &operator=(Sha256 &&) = delete;

    ~Sha256() = default;


    // 在SHA256算法中的初始信息摘要，这些常量是对自然数中前8个质数的平方根的小数部分取前32bit而来
    static const std::vector<uint32_t> initial_message_digest_;

    // 在SHA256算法中，用到64个常量，这些常量是对自然数中前64个质数的立方根的小数部分取前32bit而来
    static const std::vector<uint32_t> add_constant_;
};






}


#endif