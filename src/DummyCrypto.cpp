#include "DummyCrypto.h"
#include <stdexcept>
#include <assert.h>
#include <memory>

using namespace std;

namespace rtcModule {

typedef rtcModule::ICryptoFunctions::IString IString;
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


string xorEnc(const string& str, const string& key)
{
  const char* int2hex = "0123456789abcdef";
  string result;
  size_t j = 0;
  size_t len = str.size();
  size_t keylen = key.size();
  for (size_t i = 0; i < len; i++)
  {
      char code = str[i] ^ key[j++];
      if (j >= keylen)
          j = 0;
      result+=int2hex[code>>4];
      result+=int2hex[code&0x0f];
  }
  return result;
}

string xorDec(const string& str, const string& key)
{
    string result;
    size_t len = str.size();
    size_t j = 0;
    if (len & 1)
        throw runtime_error("Not a proper hex string");
    size_t keylen = key.size();
    for (size_t i=0; i<len; i+=2)
    {
        char code = (hexDigitToInt(str[i]) << 4)|hexDigitToInt(str[i+1]);
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

IString* DummyCrypto::generateMac(const char* data, const char* key)
{
    if (!data || !key)
        return nullptr;
    return new StringImpl(xorEnc(data, key));
}

IString* DummyCrypto::decryptMessage(const char* msg)
{
    if (!msg)
        return nullptr;
    return new StringImpl(xorDec(msg, mOwnJid));
}

IString* DummyCrypto::encryptMessageForJid(const char* msg, const char* bareJid)
{
    if (mKeysLoaded.find(bareJid) == mKeysLoaded.end())
        return nullptr;
    return new StringImpl(xorEnc(msg, bareJid));
}

void DummyCrypto::preloadCryptoForJid(const char* jid, void* userp, void(*cb)(void*, const char*))
{
    assert(jid);
    mKeysLoaded.insert(jid);
    cb(userp, nullptr);
}

IString* DummyCrypto::scrambleJid(const char* jid)
{
    return new StringImpl(jid);
}
IString* DummyCrypto::generateFprMacKey()
{
    return new StringImpl(makeRandomString(16));
}

IString* DummyCrypto::generateRandomString(size_t size)
{
    return new StringImpl(makeRandomString(size));
}

}
