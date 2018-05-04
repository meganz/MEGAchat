#ifndef BASE64_H
#define BASE64_H
#include <string>

std::string base64urlencode(const void *data, size_t inlen);
size_t base64urldecode(const char* str, size_t len, void* bin, size_t binlen);
#endif // BASE64_H

