/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef SHA256_H
#define SHA256_H

/*************************** HEADER FILES ***************************/
#include <stddef.h>

/****************************** MACROS ******************************/
#define SHA256_BLOCK_SIZE 32            // SHA256 outputs a 32 byte digest

/**************************** DATA TYPES ****************************/
typedef unsigned char BYTE;             // 8-bit byte
typedef unsigned int  WORD;             // 32-bit word, change to "long" for 16-bit machines

#if 1
#include "mbedtls/sha256.h"

#define SHA256_CTX      mbedtls_sha256_context
#define sha256_init     mbedtls_sha256_init
#define sha256_update   mbedtls_sha256_update
#define sha256_final    mbedtls_sha256_finish
#else
typedef struct {
	BYTE data[64];
	WORD datalen;
	unsigned long long bitlen;
	WORD state[8];
} SHA256_CTX;

/*********************** FUNCTION DECLARATIONS **********************/
void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
void sha256_final(SHA256_CTX *ctx, BYTE hash[]);
#endif

#endif   // SHA256_H
