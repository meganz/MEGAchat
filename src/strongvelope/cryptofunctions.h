/*
 * cryptofunctions.h
 *
 *  Created on: 18/11/2015
 *      Author: admin2
 */
#include <sodium.h>
#include <cryptopp/sha.h>
#include <cryptopp/hmac.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/osrng.h>
#include <iostream>
#include <ctime>

namespace strongvelope
{

/** Key types */
enum KeyType { kKeyTypeEd25519, kKeyTypeCu25519 };

static inline void
getPubKeyFromPrivKey(const StaticBuffer& privKey, KeyType keyType, Key<32>& output)
{
    if (keyType == kKeyTypeEd25519)
    {
        byte sk[crypto_sign_SECRETKEYBYTES];
        assert(output.dataSize() <= crypto_sign_PUBLICKEYBYTES);
        crypto_sign_seed_keypair(output.ubuf(), sk, privKey.ubuf());
        output.setDataSize(crypto_sign_PUBLICKEYBYTES);
    }
    else if (keyType == kKeyTypeCu25519)
    {
        assert(output.dataSize() >= crypto_scalarmult_BYTES);
        crypto_scalarmult_base(output.ubuf(), privKey.ubuf());
        output.setDataSize(crypto_scalarmult_BYTES);
    }
    else
    {
        assert(false && "Unknown key type");
    }
}


static inline void hmac_sha256_bytes(const StaticBuffer& plain,
                                  const StaticBuffer& key, const StaticBuffer& output)
{
    CryptoPP::HMAC<CryptoPP::SHA256> hmac(key.ubuf(), key.dataSize());
    hmac.CalculateDigest(output.ubuf(), plain.ubuf(), plain.dataSize());
}

static inline void aesECBEncrypt(const SendKey& text, const SendKey& key,
    StaticBuffer& output)
{    
    assert(key.dataSize() == CryptoPP::AES::BLOCKSIZE);
    assert(text.dataSize() == CryptoPP::AES::BLOCKSIZE);
    assert(output.dataSize() == CryptoPP::AES::BLOCKSIZE);
    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption aesencryption(key.ubuf(), key.dataSize());
    aesencryption.ProcessData(output.ubuf(), text.ubuf(), text.dataSize());
}

static inline void aesECBDecrypt(const StaticBuffer& cipherText,
    const SendKey& key, SendKey& output)
{
    assert(key.dataSize() == CryptoPP::AES::BLOCKSIZE);
    assert(cipherText.dataSize() == CryptoPP::AES::BLOCKSIZE);
    assert(output.dataSize() == CryptoPP::AES::BLOCKSIZE);
    CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption aesdecryption(key.ubuf(), key.dataSize());
    aesdecryption.ProcessData(output.ubuf(),
        cipherText.ubuf(), cipherText.dataSize());
}

//used only for legacy keys
static inline void aesCBCDecrypt(const StaticBuffer& cipherText,
        const StaticBuffer& derivedKey, const StaticBuffer& iv, Buffer& output)
{
    assert(iv.dataSize() == CryptoPP::AES::BLOCKSIZE);
    assert(derivedKey.dataSize() == CryptoPP::AES::BLOCKSIZE);
    assert(cipherText.dataSize() % CryptoPP::AES::BLOCKSIZE == 0);

    CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption aesdec(derivedKey.ubuf(), derivedKey.dataSize(), iv.ubuf());
    aesdec.ProcessData((unsigned char*)output.writePtr(0, cipherText.dataSize()), cipherText.ubuf(), cipherText.dataSize());
}

//CTR mode is used for message content

//can't use binary buffers here, libsodium doesn't support them for CTR mode
static inline std::string aesCTREncrypt(const std::string& text,
                        const StaticBuffer& derivedkey, const StaticBuffer& iv)
{
    assert(iv.dataSize() == CryptoPP::AES::BLOCKSIZE);
    assert(derivedkey.dataSize() == CryptoPP::AES::BLOCKSIZE);
    CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption encryptor;
    std::string cipher;
    encryptor.SetKeyWithIV(derivedkey.ubuf(), derivedkey.dataSize(), iv.ubuf());
    // The StreamTransformationFilter adds padding
    //  as required. ECB and CBC Mode must be padded
    //  to the block size of the cipher. CTR does not.
    CryptoPP::StringSource s(text, true,
        new CryptoPP::StreamTransformationFilter(encryptor,
            new CryptoPP::StringSink(cipher)
        ) // StreamTransformationFilter
    ); // StringSource
    return cipher;
}

static inline std::string aesCTRDecrypt(const std::string& ciphertext,
                            const StaticBuffer& derivedkey, const StaticBuffer& iv)
{
    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption decryptor;
    std::string text;
    assert(iv.dataSize() == CryptoPP::AES::BLOCKSIZE);
    assert(derivedkey.dataSize() == CryptoPP::AES::BLOCKSIZE);
    decryptor.SetKeyWithIV(derivedkey.ubuf(), derivedkey.dataSize(), iv.ubuf());
    // The StreamTransformationFilter adds padding
    //  as required. ECB and CBC Mode must be padded
    //  to the block size of the cipher. CTR does not.
    CryptoPP::StringSource s(ciphertext, true,
        new CryptoPP::StreamTransformationFilter(decryptor,
            new CryptoPP::StringSink(text)
        ) // StreamTransformationFilter
    ); // StringSource
    return text;
}

}
