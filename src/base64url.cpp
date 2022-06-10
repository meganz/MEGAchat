#include "base64url.h"
#include <stdexcept>

static char b64enctable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
std::string base64urlencode(const void *data, size_t inlen)
{
    std::string encoded_data;
    encoded_data.reserve(((inlen+2) / 3) * 4);
    for (size_t i = 0; i < inlen;)
    {
        uint8_t octet_a = i < inlen ? static_cast<const char*>(data)[i++] : 0;
        uint8_t octet_b = i < inlen ? static_cast<const char*>(data)[i++] : 0;
        uint8_t octet_c = i < inlen ? static_cast<const char*>(data)[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded_data+= b64enctable[(triple >> 18) & 0x3F];
        encoded_data+= b64enctable[(triple >> 12) & 0x3F];
        encoded_data+= b64enctable[(triple >> 6) & 0x3F];
        encoded_data+= b64enctable[triple & 0x3F];
    }
    int mod = static_cast<int>(inlen % 3);
    if (mod)
    {
        encoded_data.resize(encoded_data.size() - static_cast<size_t>((3 - mod)));
    }
    return encoded_data;
}

static const unsigned char b64dectable[] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,62, 255, 62,255, 63,
    52,  53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,255,255,255,
    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15,  16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255, 63,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41,  42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

size_t base64urldecode(const char* str, size_t len, void* bin, size_t binlen)
{
    if (binlen < (len*3)/4)
        throw std::runtime_error("base64urldecode: Insufficient output buffer space");
    auto mod = len % 4;
    if ((mod != 0) && (mod < 2))
        throw std::runtime_error("Incorrect size of base64 string, size mod 4 must be at least 2");

    const unsigned char* last = (const unsigned char*)str+len-1;
    const unsigned char* in = (const unsigned char*)str;
    unsigned char* out = (unsigned char*)bin;
    for(;in <= last;)
    {
        unsigned char one = b64dectable[*in++];
        if (one > 63)
            throw std::runtime_error(std::string("Invalid char "+std::to_string(*(in-1))+ " in base64 stream at offset ") + std::to_string(((char*)(in-1)-str)));

        unsigned char two = b64dectable[*in++];
        if (two > 63)
            throw std::runtime_error(std::string("Invalid char "+std::to_string(*(in-1))+ " in base64 stream at offset ") + std::to_string(((char*)(in-1)-str)));

        *out++ = static_cast<unsigned char>((one << 2) | (two >> 4));
        if (in > last)
            break;

        unsigned char three = b64dectable[*in++];
        if (three > 63)
            throw std::runtime_error(std::string("Invalid char "+std::to_string(*(in-1))+ " in base64 stream at offset ") + std::to_string(((char*)(in-1)-str)));
        *out++ = static_cast<unsigned char>((two << 4) | (three >> 2));

        if (in > last)
            break;

        unsigned char four = b64dectable[*in++];
        if (four > 63)
            throw std::runtime_error(std::string("Invalid char "+std::to_string(*(in-1))+ " in base64 stream at offset ") + std::to_string(((char*)(in-1)-str)));

        *out++ = static_cast<unsigned char>((three << 6) | four);
    }
    return static_cast<size_t>(out-(unsigned char*)bin);
}
