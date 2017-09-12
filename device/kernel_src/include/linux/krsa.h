/*
 * krsa.h
 * Kernel RSA implementation header file
 *
 * Jan 6, 2015
 * daveti
 * root@davejingtian.org
 * http://davejingtian.org
 */
#ifndef _LINUX_KRSA_H
#define _LINUX_KRSA_H

#include <linux/mpi.h>

#define KRSA_TPM_QUOTE_SIG_LEN          256     /* bytes */
#define KRSA_TPM_QUOTE_VALID_LEN        48      /* bytes */
#define KRSA_TPM_AIK_PUB_KEY_N_LEN      256     /* bytes */
#define KRSA_TPM_AIK_PUB_KEY_E_LEN      3       /* bytes */

/* Ported from include/uapi/linux/hash_info.h */
enum hash_algo {
	HASH_ALGO_MD4,
	HASH_ALGO_MD5,
	HASH_ALGO_SHA1,
	HASH_ALGO_RIPE_MD_160,
	HASH_ALGO_SHA256,
	HASH_ALGO_SHA384,
	HASH_ALGO_SHA512,
	HASH_ALGO_SHA224,
	HASH_ALGO_RIPE_MD_128,
	HASH_ALGO_RIPE_MD_256,
	HASH_ALGO_RIPE_MD_320,
	HASH_ALGO_WP_256,
	HASH_ALGO_WP_384,
	HASH_ALGO_WP_512,
	HASH_ALGO_TGR_128,
	HASH_ALGO_TGR_160,
	HASH_ALGO_TGR_192,
	HASH_ALGO__LAST
};

/* Krsa context */
struct krsa_ctx {
	enum hash_algo	digest_algo : 8; /* Hash algorithm */
	u8 *sig;
	unsigned int sig_size;	/* Note that we assume smaller signature */
	u8 *valid;
	unsigned int valid_size;	/* Note that we assume smaller validation data */
	u8 *digest;
	unsigned int digest_size;
	MPI sig_mpi;	/* MPI for the signature */
	struct {
		MPI n;
		MPI e;
	} rsa_pub_key;	/* MPI for RSA public key */
};

/* Krsa API to outside */
int krsa_init(struct krsa_ctx *ctx,
                char *tpm_quote_sig, int sig_len,
                char *tpm_quote_valid, int valid_len,
                char *tpm_aik_pub_key_n, int pubkey_n_len,
                char *tpm_aik_pub_key_e, int pubkey_e_len);
int krsa_verify(struct krsa_ctx *ctx);
void krsa_exit(struct krsa_ctx *ctx);

#endif
