/*
 * Copyright (c) 2012-2013 Yunshan Networks
 * All right reserved.
 *
 * Author Name: Song Zhen
 * Date: 2014-01-08
 *
 * Function: Encryption Algorithm
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <pthread.h>
#include <openssl/aes.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include "lc_aes.h"

#define LICENSE_KEY   "1234567890123456"

int AES_LOG_DEFAULT_LEVEL = LOG_DEBUG;
int *AES_LOG_LEVEL_P = &AES_LOG_DEFAULT_LEVEL;

char *AES_LOGLEVEL[8]={
    "NONE",
    "ALERT",
    "CRIT",
    "ERR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG"
};

#define AES_LOG(level, format, ...)  \
    if(level <= *AES_LOG_LEVEL_P){\
        syslog(level, "[tid=%lu] %s %s: " format, \
            pthread_self(), AES_LOGLEVEL[level], __FUNCTION__, ##__VA_ARGS__);\
    }

void base64_encode(unsigned char *out_string, int *out_len, const char *in_string, int in_len)
{
    BIO *bmem = NULL;
    BIO *b64 = NULL;
    BUF_MEM *bptr = NULL;
    if ((NULL == out_string) || (NULL == in_string))
    {
        AES_LOG(LOG_ERR, "%s: InputPara is NULL\n", __FUNCTION__);
        return;
    }

    b64 = BIO_new(BIO_f_base64());
    if (NULL == b64)
    {
        AES_LOG(LOG_ERR, "%s: b64 malloc failed\n", __FUNCTION__);
        return;
    }
    bmem = BIO_new(BIO_s_mem());
    if (NULL == bmem)
    {
        AES_LOG(LOG_ERR, "%s: bmem malloc failed\n", __FUNCTION__);
        BIO_free(b64);
        return;
    }
    b64 = BIO_push(b64, bmem);

    BIO_write(b64, in_string, in_len);
    (void) BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);
    if (bptr->length > *out_len)
    {
        *out_len = bptr->length;
        AES_LOG(LOG_ERR, "%s: Encode len out of range\n", __FUNCTION__);
        BIO_free_all(b64);
        return;
    }

    memcpy(out_string, bptr->data, bptr->length - 1);
    out_string[bptr->length - 1] = 0;
    *out_len = bptr->length - 1;
    BIO_free_all(b64);
    return;
}

void base64_decode(unsigned char* out_string, int *out_len, const char* in_string, int in_len)
{
    BIO * b64 = NULL;
    BIO * bmem = NULL;
    char decode_in_str[BASE64_ENCODE_MAX_LEN + 1];

    if(NULL == out_string || NULL == in_string)
    {
        AES_LOG(LOG_ERR, "%s: InputPara is NULL\n", __FUNCTION__);
        return;
    }

    /*Openssl demand to have '\n' to end the string.*/
    memset(decode_in_str, 0, in_len + 1);
    decode_in_str[in_len] = '\n';
    memcpy(decode_in_str, in_string, in_len);
    memset(out_string, 0, *out_len);

    b64 = BIO_new(BIO_f_base64());
    if (NULL == b64)
    {
        AES_LOG(LOG_ERR, "%s: b64 malloc failed\n", __FUNCTION__);
        return;
    }
    bmem = BIO_new_mem_buf(decode_in_str, in_len+1);
    if (NULL == bmem)
    {
        AES_LOG(LOG_ERR, "%s: bmem malloc failed\n", __FUNCTION__);
        BIO_free(b64);
        return;
    }
    bmem = BIO_push(b64, bmem);
    *out_len = BIO_read(bmem, out_string, *out_len);
    BIO_free_all(bmem);
    return;
}

void aes_box_encrypt(char* des_string, int *des_len, const char* source_string)
{
    int len =0;
    AES_KEY aes;
    unsigned char key[AES_BLOCK_SIZE];
    unsigned char iv[AES_BLOCK_SIZE];

    if(NULL == des_string || NULL == source_string)
    {
        AES_LOG(LOG_ERR, "%s: InputPara is NULL\n", __FUNCTION__);
        return;
    }

    //Generate own AES Key
    memset(key, 0, sizeof(AES_KEY));
    memcpy(key, LICENSE_KEY, AES_BLOCK_SIZE);

    // Set encryption iv
    memset(iv, '0', AES_BLOCK_SIZE);

    // Set encryption key
    if (AES_set_encrypt_key(key, 128, &aes) < 0)
    {
        AES_LOG(LOG_ERR, "%s: Set encryption key failed\n", __FUNCTION__);
        return;
    }

    if (strlen(source_string) % AES_BLOCK_SIZE == 0)
    {
        len = strlen(source_string);
    }
    else
    {
        len = (strlen(source_string) / AES_BLOCK_SIZE + 1) * AES_BLOCK_SIZE;
    }
    *des_len = len;

    AES_cbc_encrypt((const unsigned char*)source_string,
                    (unsigned char*)des_string,
                    len, &aes, iv, AES_ENCRYPT);
}

void aes_box_decrypt(char* des_string, const char* source_string, int src_len)
{
    AES_KEY aes;
    unsigned char key[AES_BLOCK_SIZE];
    unsigned char iv[AES_BLOCK_SIZE];

    if(NULL == des_string || NULL == source_string)
    {
        AES_LOG(LOG_ERR, "%s: InputPara is NULL\n", __FUNCTION__);
        return;
    }

    //Generate own AES Key
    memset(key, 0, AES_BLOCK_SIZE);
    memcpy(key, LICENSE_KEY, AES_BLOCK_SIZE);

    //Set encryption iv
    memset(iv, '0', AES_BLOCK_SIZE);

    //Set encryption key
    if (AES_set_decrypt_key(key, 128, &aes) < 0)
    {
        AES_LOG(LOG_ERR, "%s: Set encryption key failed\n", __FUNCTION__);
        return;
    }

    AES_cbc_encrypt((const unsigned char*)source_string,
                    (unsigned char*)des_string,
                    src_len, &aes, iv, AES_DECRYPT);
}

int aes_cbc_128_encrypt(char* dst_string, const char* src_string)
{
    int ret = 0;
    int aes_src_len    = 0;
    int base64_src_len = 0;
    int base64_dst_len = BASE64_ENCODE_MAX_LEN;
    char dst_aes_string[AES_ENCODE_MAX_LEN];

    AES_LOG(LOG_DEBUG, "%s: Enter\n", __FUNCTION__);

    if(NULL == dst_string || NULL == src_string)
    {
        AES_LOG(LOG_ERR, "%s: InputPara is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    aes_src_len = strlen(src_string);
    if (AES_ENCODE_MIN_LEN > aes_src_len || AES_ENCODE_MAX_LEN < aes_src_len)
    {
        AES_LOG(LOG_ERR, "%s: InputPara len is out of range\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(dst_aes_string, 0, AES_ENCODE_MAX_LEN);
    aes_box_encrypt(dst_aes_string, &base64_src_len, src_string);

    base64_encode((unsigned char*)dst_string, &base64_dst_len,
                  dst_aes_string, base64_src_len);
    if (0 == base64_dst_len || BASE64_ENCODE_MAX_LEN < base64_dst_len)
    {
        AES_LOG(LOG_ERR, "%s: Encode operation failed\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

error:
    AES_LOG(LOG_DEBUG, "%s: End\n", __FUNCTION__);
    return ret;
}

int aes_cbc_128_decrypt(char* dst_string, const char* src_string)
{
    int ret = 0;
    int base64_src_len = 0;
    int base64_dst_len = AES_ENCODE_MAX_LEN;
    char src_aes_string[BASE64_ENCODE_MAX_LEN];

    AES_LOG(LOG_DEBUG, "%s: Enter\n", __FUNCTION__);

    if(NULL == dst_string || NULL == src_string)
    {
        AES_LOG(LOG_ERR, "%s: InputPara is NULL\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    base64_src_len = strlen(src_string);
    if(BASE64_ENCODE_MIN_LEN > base64_src_len || BASE64_ENCODE_MAX_LEN < base64_src_len)
    {
        AES_LOG(LOG_ERR, "%s: InputPara len is out of range\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    memset(src_aes_string, 0, BASE64_ENCODE_MAX_LEN);
    base64_decode((unsigned char*)src_aes_string, &base64_dst_len,
                  src_string, base64_src_len);
    if (0 == base64_dst_len || AES_ENCODE_MIN_LEN > base64_dst_len)
    {
        AES_LOG(LOG_ERR, "%s: Decode operation failed\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

    aes_box_decrypt(dst_string, src_aes_string, base64_dst_len);
    if (0 == strlen(dst_string))
    {
        AES_LOG(LOG_ERR, "%s: Decrypt operation failed\n", __FUNCTION__);
        ret = -1;
        goto error;
    }

error:
    AES_LOG(LOG_DEBUG, "%s: End\n", __FUNCTION__);
    return ret;
}
