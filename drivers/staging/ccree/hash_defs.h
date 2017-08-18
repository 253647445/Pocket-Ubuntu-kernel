/*
 * Copyright (C) 2012-2017 ARM Limited or its affiliates.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef  _HASH_DEFS_H__
#define  _HASH_DEFS_H__

#include "cc_crypto_ctx.h"

/* this files provides definitions required for hash engine drivers */
#ifndef CC_CONFIG_HASH_SHA_512_SUPPORTED
#define SEP_HASH_LENGTH_WORDS		2
#else
#define SEP_HASH_LENGTH_WORDS		4
#endif

#ifdef BIG__ENDIAN
#define OPAD_CURRENT_LENGTH 0x40000000, 0x00000000 , 0x00000000, 0x00000000
#define HASH_LARVAL_MD5  0x76543210, 0xFEDCBA98, 0x89ABCDEF, 0x01234567
#define HASH_LARVAL_SHA1 0xF0E1D2C3, 0x76543210, 0xFEDCBA98, 0x89ABCDEF, 0x01234567
#define HASH_LARVAL_SHA224 0XA44FFABE, 0XA78FF964, 0X11155868, 0X310BC0FF, 0X39590EF7, 0X17DD7030, 0X07D57C36, 0XD89E05C1
#define HASH_LARVAL_SHA256 0X19CDE05B, 0XABD9831F, 0X8C68059B, 0X7F520E51, 0X3AF54FA5, 0X72F36E3C, 0X85AE67BB, 0X67E6096A
#define HASH_LARVAL_SHA384 0X1D48B547, 0XA44FFABE, 0X0D2E0CDB, 0XA78FF964, 0X874AB48E, 0X11155868, 0X67263367, 0X310BC0FF, 0XD8EC2F15, 0X39590EF7, 0X5A015991, 0X17DD7030, 0X2A299A62, 0X07D57C36, 0X5D9DBBCB, 0XD89E05C1
#define HASH_LARVAL_SHA512 0X19CDE05B, 0X79217E13, 0XABD9831F, 0X6BBD41FB, 0X8C68059B, 0X1F6C3E2B, 0X7F520E51, 0XD182E6AD, 0X3AF54FA5, 0XF1361D5F, 0X72F36E3C, 0X2BF894FE, 0X85AE67BB, 0X3BA7CA84, 0X67E6096A, 0X08C9BCF3
#else
#define OPAD_CURRENT_LENGTH 0x00000040, 0x00000000, 0x00000000, 0x00000000
#define HASH_LARVAL_MD5  0x10325476, 0x98BADCFE, 0xEFCDAB89, 0x67452301
#define HASH_LARVAL_SHA1 0xC3D2E1F0, 0x10325476, 0x98BADCFE, 0xEFCDAB89, 0x67452301
#define HASH_LARVAL_SHA224 0xbefa4fa4, 0x64f98fa7, 0x68581511, 0xffc00b31, 0xf70e5939, 0x3070dd17, 0x367cd507, 0xc1059ed8
#define HASH_LARVAL_SHA256 0x5be0cd19, 0x1f83d9ab, 0x9b05688c, 0x510e527f, 0xa54ff53a, 0x3c6ef372, 0xbb67ae85, 0x6a09e667
#define HASH_LARVAL_SHA384 0X47B5481D, 0XBEFA4FA4, 0XDB0C2E0D, 0X64F98FA7, 0X8EB44A87, 0X68581511, 0X67332667, 0XFFC00B31, 0X152FECD8, 0XF70E5939, 0X9159015A, 0X3070DD17, 0X629A292A, 0X367CD507, 0XCBBB9D5D, 0XC1059ED8
#define HASH_LARVAL_SHA512 0x5be0cd19, 0x137e2179, 0x1f83d9ab, 0xfb41bd6b, 0x9b05688c, 0x2b3e6c1f, 0x510e527f, 0xade682d1, 0xa54ff53a, 0x5f1d36f1, 0x3c6ef372, 0xfe94f82b, 0xbb67ae85, 0x84caa73b, 0x6a09e667, 0xf3bcc908
#endif

enum HashConfig1Padding {
	HASH_PADDING_DISABLED = 0,
	HASH_PADDING_ENABLED = 1,
	HASH_DIGEST_RESULT_LITTLE_ENDIAN = 2,
	HASH_CONFIG1_PADDING_RESERVE32 = INT32_MAX,
};

enum HashCipherDoPadding {
	DO_NOT_PAD = 0,
	DO_PAD = 1,
	HASH_CIPHER_DO_PADDING_RESERVE32 = INT32_MAX,
};

typedef struct SepHashPrivateContext {
	/* The current length is placed at the end of the context buffer because the hash 
	   context is used for all HMAC operations as well. HMAC context includes a 64 bytes 
	   K0 field.  The size of struct drv_ctx_hash reserved field is  88/184 bytes depend if t
	   he SHA512 is supported ( in this case teh context size is 256 bytes).
	   The size of struct drv_ctx_hash reseved field is 20 or 52 depend if the SHA512 is supported.
	   This means that this structure size (without the reserved field can be up to 20 bytes ,
	   in case sha512 is not suppported it is 20 bytes (SEP_HASH_LENGTH_WORDS define to 2 ) and in the other
	   case it is 28 (SEP_HASH_LENGTH_WORDS define to 4) */
	uint32_t reserved[(sizeof(struct drv_ctx_hash)/sizeof(uint32_t)) - SEP_HASH_LENGTH_WORDS - 3];
	uint32_t CurrentDigestedLength[SEP_HASH_LENGTH_WORDS];
	uint32_t KeyType;
	uint32_t dataCompleted;
	uint32_t hmacFinalization;
	/* no space left */
} SepHashPrivateContext_s;

#endif /*_HASH_DEFS_H__*/

