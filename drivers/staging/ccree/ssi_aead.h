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

/* \file ssi_aead.h
   ARM CryptoCell AEAD Crypto API
 */

#ifndef __SSI_AEAD_H__
#define __SSI_AEAD_H__

#include <linux/kernel.h>
#include <crypto/algapi.h>
#include <crypto/ctr.h>


/* mac_cmp - HW writes 8 B but all bytes hold the same value */
#define ICV_CMP_SIZE 8
#define CCM_CONFIG_BUF_SIZE (AES_BLOCK_SIZE*3)
#define MAX_MAC_SIZE MAX(SHA256_DIGEST_SIZE, AES_BLOCK_SIZE)


/* defines for AES GCM configuration buffer */
#define GCM_BLOCK_LEN_SIZE 8

#define GCM_BLOCK_RFC4_IV_OFFSET    	4  
#define GCM_BLOCK_RFC4_IV_SIZE  	    8  /* IV size for rfc's */
#define GCM_BLOCK_RFC4_NONCE_OFFSET 	0  
#define GCM_BLOCK_RFC4_NONCE_SIZE   	4  



/* Offsets into AES CCM configuration buffer */
#define CCM_B0_OFFSET 0
#define CCM_A0_OFFSET 16
#define CCM_CTR_COUNT_0_OFFSET 32
/* CCM B0 and CTR_COUNT constants. */
#define CCM_BLOCK_NONCE_OFFSET 1  /* Nonce offset inside B0 and CTR_COUNT */
#define CCM_BLOCK_NONCE_SIZE   3  /* Nonce size inside B0 and CTR_COUNT */
#define CCM_BLOCK_IV_OFFSET    4  /* IV offset inside B0 and CTR_COUNT */
#define CCM_BLOCK_IV_SIZE      8  /* IV size inside B0 and CTR_COUNT */

enum aead_ccm_header_size {
	ccm_header_size_null = -1,
	ccm_header_size_zero = 0,
	ccm_header_size_2 = 2,
	ccm_header_size_6 = 6,
	ccm_header_size_max = INT32_MAX
};

struct aead_req_ctx {
	/* Allocate cache line although only 4 bytes are needed to
	*  assure next field falls @ cache line 
	*  Used for both: digest HW compare and CCM/GCM MAC value */
	uint8_t mac_buf[MAX_MAC_SIZE] ____cacheline_aligned;
	uint8_t ctr_iv[AES_BLOCK_SIZE] ____cacheline_aligned;

	//used in gcm 
	uint8_t gcm_iv_inc1[AES_BLOCK_SIZE] ____cacheline_aligned;
	uint8_t gcm_iv_inc2[AES_BLOCK_SIZE] ____cacheline_aligned;
	uint8_t hkey[AES_BLOCK_SIZE] ____cacheline_aligned;
	struct {
		uint8_t lenA[GCM_BLOCK_LEN_SIZE] ____cacheline_aligned;
		uint8_t lenC[GCM_BLOCK_LEN_SIZE] ;
	} gcm_len_block;

	uint8_t ccm_config[CCM_CONFIG_BUF_SIZE] ____cacheline_aligned;
	unsigned int hw_iv_size ____cacheline_aligned; /*HW actual size input*/
	uint8_t backup_mac[MAX_MAC_SIZE]; /*used to prevent cache coherence problem*/
	uint8_t *backup_iv; /*store iv for generated IV flow*/
	uint8_t *backup_giv; /*store iv for rfc3686(ctr) flow*/
	dma_addr_t mac_buf_dma_addr; /* internal ICV DMA buffer */
	dma_addr_t ccm_iv0_dma_addr; /* buffer for internal ccm configurations */
	dma_addr_t icv_dma_addr; /* Phys. address of ICV */

	//used in gcm 
	dma_addr_t gcm_iv_inc1_dma_addr; /* buffer for internal gcm configurations */
	dma_addr_t gcm_iv_inc2_dma_addr; /* buffer for internal gcm configurations */
	dma_addr_t hkey_dma_addr; /* Phys. address of hkey */
	dma_addr_t gcm_block_len_dma_addr; /* Phys. address of gcm block len */
	bool is_gcm4543;

	uint8_t *icv_virt_addr; /* Virt. address of ICV */
	struct async_gen_req_ctx gen_ctx;
	struct ssi_mlli assoc;
	struct ssi_mlli src;
	struct ssi_mlli dst;
	struct scatterlist* srcSgl;
	struct scatterlist* dstSgl;
	unsigned int srcOffset;
	unsigned int dstOffset;
	enum ssi_req_dma_buf_type assoc_buff_type;
	enum ssi_req_dma_buf_type data_buff_type;
	struct mlli_params mlli_params;
	unsigned int cryptlen;
	struct scatterlist ccm_adata_sg;
	enum aead_ccm_header_size ccm_hdr_size;
	unsigned int req_authsize;
	enum drv_cipher_mode cipher_mode;
	bool is_icv_fragmented;
	bool is_single_pass;
	bool plaintext_authenticate_only; //for gcm_rfc4543
};

int ssi_aead_alloc(struct ssi_drvdata *drvdata);
int ssi_aead_free(struct ssi_drvdata *drvdata);

#endif /*__SSI_AEAD_H__*/
