#ifndef __LC_AES_H__
#define __LC_AES_H__

#define AES_ENCODE_MIN_LEN       30
#define AES_ENCODE_MAX_LEN       96
#define BASE64_ENCODE_MIN_LEN    44
#define BASE64_ENCODE_MAX_LEN    128

int aes_cbc_128_encrypt(char* dst_string, const char* src_string);
int aes_cbc_128_decrypt(char* dst_string, const char* src_string);

#endif
