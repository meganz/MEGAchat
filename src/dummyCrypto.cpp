#include "dummyCrypto.h"
#include <stdexcept>
#include <assert.h>
#include <memory>
#include "ITypesImpl.h"

using namespace std;

namespace rtcModule {

typedef rtcModule::IString IString;
char hexDigitToInt(char code)
{
    if ((code > 47) && (code < 58))
        return code-48;
    else if (code > 96 && code < 103)
        return code-97+10;
    else if (code > 64 && code < 71)
        return code-65+10;
    else
        throw std::runtime_error("Non-hex char");
}


string xorEnc(const char* msg, size_t msglen, const char* key, size_t keylen)
{
  const char* int2hex = "0123456789abcdef";
  string result;
  size_t j = 0;
  for (size_t i = 0; i < msglen; i++)
  {
      char code = msg[i] ^ key[j++];
      if (j >= keylen)
          j = 0;
      result+=int2hex[code>>4];
      result+=int2hex[code&0x0f];
  }
  return result;
}

string xorDec(const char* msg, size_t msglen, const char* key, size_t keylen)
{
    string result;
    size_t j = 0;
    if (msglen & 1)
        throw runtime_error("Not a proper hex string");
    for (size_t i=0; i<msglen; i+=2)
    {
        char code = (hexDigitToInt(msg[i]) << 4)|hexDigitToInt(msg[i+1]);
        code ^= key[j++];
        if (j >= keylen)
            j = 0;
        result+=code;
    }
    return result;
}

string makeRandomString(int len)
{
    if (len < 1)
        return "";
    string result;
    result.reserve(len);
    for (int i=0; i<len; i++)
    {
        char ch = (rand() % 75)+'0';
        if ((ch > '9') && (ch < 'A'))
            ch+=7; //move to A-Z range
        else if ((ch > 'Z') && (ch < 'a'))
            ch += 6;
        result+=ch;
    }
    return result;
}

IString* DummyCrypto::generateMac(const CString& data, const CString& key)
{
    if (!data || !key)
        return nullptr;
    return new IString_string(xorEnc(data.c_str(), data.size(), key.c_str(), key.size()));
}

IString* DummyCrypto::decryptMessage(const CString& msg)
{
    if (!msg)
        return nullptr;
    return new IString_string(xorDec(msg.c_str(), msg.size(), mOwnJid.c_str(), mOwnJid.size()));
}

IString* DummyCrypto::encryptMessageForJid(const CString& msg, const CString& bareJid)
{
    if (mKeysLoaded.find(string(bareJid.c_str(), bareJid.size())) == mKeysLoaded.end())
        return nullptr;
    return new IString_string(xorEnc(msg.c_str(), msg.size(), bareJid.c_str(), bareJid.size()));
}

void DummyCrypto::preloadCryptoForJid(const CString& jid, void* userp, void(*cb)(void*, const CString&))
{
    assert(jid);
    mKeysLoaded.insert(string(jid.c_str(), jid.size()));
    cb(userp, nullptr);
}

IString* DummyCrypto::scrambleJid(const CString& jid)
{
    return new IString_string(string(jid.c_str(), jid.size()));
}
IString* DummyCrypto::generateFprMacKey()
{
    return new IString_string(makeRandomString(16));
}

IString* DummyCrypto::generateRandomString(size_t size)
{
    return new IString_string(makeRandomString(size));
}

}
