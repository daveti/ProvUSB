/*
 * krsa.c
 * Kernel module for RSA verification
 *
 * NOTE: this implementation is based on David Howell's
 * Asymmetric public key framework but get rid of the public
 * key framework by manipulating the binary data directly.
 * This code is initally designed for porting kernel RSA
 * implementation to kernel 3.5 Gumstix and is used to
 * verify the TPM Quote using the TPM AIK public key.
 *
 * Testing env:
 * Linux ubuntu14-crypto 3.13.0-32-generic #57-Ubuntu SMP
 * Tue Jul 15 03:51:08 UTC 2014 x86_64 x86_64 x86_64 GNU/Linux
 *
 * Cautions:
 * 1. This version does not support IRQ context!
 * 2. This version does not contain export!
 * 3. mpi_read_raw_data() is not available on kernel 3.5!
 *
 * Nov 26, 2014
 * root@davejingtian.org
 * http://davejingtian.org
 */
#include <linux/module.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/mpi.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <crypto/sha.h>
#include <crypto/md5.h>
#include <linux/krsa.h>


/* Typedefs */
#define pr_fmt(fmt) "Krsa: "fmt
#define kenter(FMT, ...) \
	pr_info("==> %s("FMT")\n", __func__, ##__VA_ARGS__)
#define kleave(FMT, ...) \
	pr_info("<== %s()"FMT"\n", __func__, ##__VA_ARGS__)

/* Ported from include/crypto/hash_info.h */
#define RMD128_DIGEST_SIZE      16
#define RMD160_DIGEST_SIZE      20
#define RMD256_DIGEST_SIZE      32
#define RMD320_DIGEST_SIZE      40
/* not defined in include/crypto/ */
#define WP512_DIGEST_SIZE       64
#define WP384_DIGEST_SIZE       48
#define WP256_DIGEST_SIZE       32
/* not defined in include/crypto/ */
#define TGR128_DIGEST_SIZE 16
#define TGR160_DIGEST_SIZE 20
#define TGR192_DIGEST_SIZE 24

/* Ported from include/crypto/public_key.h */
/* asymmetric key implementation supports only up to SHA224 */
#define PKEY_HASH__LAST         (HASH_ALGO_SHA224 + 1)

/* Ported from crypto/hash_info.c */
const char *const hash_algo_name[HASH_ALGO__LAST] = {
	[HASH_ALGO_MD4]         = "md4",
	[HASH_ALGO_MD5]         = "md5",
	[HASH_ALGO_SHA1]        = "sha1",
	[HASH_ALGO_RIPE_MD_160] = "rmd160",
	[HASH_ALGO_SHA256]      = "sha256",
	[HASH_ALGO_SHA384]      = "sha384",
	[HASH_ALGO_SHA512]      = "sha512",
	[HASH_ALGO_SHA224]      = "sha224",
	[HASH_ALGO_RIPE_MD_128] = "rmd128",
	[HASH_ALGO_RIPE_MD_256] = "rmd256",
	[HASH_ALGO_RIPE_MD_320] = "rmd320",
	[HASH_ALGO_WP_256]      = "wp256",
	[HASH_ALGO_WP_384]      = "wp384",
	[HASH_ALGO_WP_512]      = "wp512",
	[HASH_ALGO_TGR_128]     = "tgr128",
	[HASH_ALGO_TGR_160]     = "tgr160",
	[HASH_ALGO_TGR_192]     = "tgr192",
};
 
const int hash_digest_size[HASH_ALGO__LAST] = {
	[HASH_ALGO_MD4]         = MD5_DIGEST_SIZE,
	[HASH_ALGO_MD5]         = MD5_DIGEST_SIZE,
	[HASH_ALGO_SHA1]        = SHA1_DIGEST_SIZE,
	[HASH_ALGO_RIPE_MD_160] = RMD160_DIGEST_SIZE,
	[HASH_ALGO_SHA256]      = SHA256_DIGEST_SIZE,
	[HASH_ALGO_SHA384]      = SHA384_DIGEST_SIZE,
	[HASH_ALGO_SHA512]      = SHA512_DIGEST_SIZE,
	[HASH_ALGO_SHA224]      = SHA224_DIGEST_SIZE,
	[HASH_ALGO_RIPE_MD_128] = RMD128_DIGEST_SIZE,
	[HASH_ALGO_RIPE_MD_256] = RMD256_DIGEST_SIZE,
	[HASH_ALGO_RIPE_MD_320] = RMD320_DIGEST_SIZE,
	[HASH_ALGO_WP_256]      = WP256_DIGEST_SIZE,
	[HASH_ALGO_WP_384]      = WP384_DIGEST_SIZE,
	[HASH_ALGO_WP_512]      = WP512_DIGEST_SIZE,
	[HASH_ALGO_TGR_128]     = TGR128_DIGEST_SIZE,
	[HASH_ALGO_TGR_160]     = TGR160_DIGEST_SIZE,
	[HASH_ALGO_TGR_192]     = TGR192_DIGEST_SIZE,
};

/* Ported from crypto/asymmetric_key/rsa.c */
/*
 * Hash algorithm OIDs plus ASN.1 DER wrappings [RFC4880 sec 5.2.2].
 */
static const u8 RSA_digest_info_MD5[] = {
	0x30, 0x20, 0x30, 0x0C, 0x06, 0x08,
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x02, 0x05, /* OID */
	0x05, 0x00, 0x04, 0x10
};

static const u8 RSA_digest_info_SHA1[] = {
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05,
	0x2B, 0x0E, 0x03, 0x02, 0x1A,
	0x05, 0x00, 0x04, 0x14
};

static const u8 RSA_digest_info_RIPE_MD_160[] = {
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05,
	0x2B, 0x24, 0x03, 0x02, 0x01,
	0x05, 0x00, 0x04, 0x14
};

static const u8 RSA_digest_info_SHA224[] = {
	0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09,
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04,
	0x05, 0x00, 0x04, 0x1C
};

static const u8 RSA_digest_info_SHA256[] = {
	0x30, 0x31, 0x30, 0x0d, 0x06, 0x09,
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01,
	0x05, 0x00, 0x04, 0x20
};

static const u8 RSA_digest_info_SHA384[] = {
	0x30, 0x41, 0x30, 0x0d, 0x06, 0x09,
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02,
	0x05, 0x00, 0x04, 0x30
};

static const u8 RSA_digest_info_SHA512[] = {
	0x30, 0x51, 0x30, 0x0d, 0x06, 0x09,
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03,
	0x05, 0x00, 0x04, 0x40
};

static const struct {
	const u8 *data;
	size_t size;
} RSA_ASN1_templates[PKEY_HASH__LAST] = {
#define _(X) { RSA_digest_info_##X, sizeof(RSA_digest_info_##X) }
	[HASH_ALGO_MD5]         = _(MD5),
	[HASH_ALGO_SHA1]        = _(SHA1),
	[HASH_ALGO_RIPE_MD_160] = _(RIPE_MD_160),
	[HASH_ALGO_SHA256]      = _(SHA256),
	[HASH_ALGO_SHA384]      = _(SHA384),
	[HASH_ALGO_SHA512]      = _(SHA512),
	[HASH_ALGO_SHA224]      = _(SHA224),
#undef _
};

/* Krsa context */
struct krsa_ctx;

/* Global vars */
static int krsa_debug = 1;
static int krsa_debug_step = 0;
static int krsa_debug_digest_step = 0;
//static struct krsa_ctx rsa_ctx;
static const char tpm_quote_sig[KRSA_TPM_QUOTE_SIG_LEN] =
"\x45\x57\x13\xff\x3f\x07\x4b\x3a\x40\x67\x17\x2a\x93\x86\xf4\x04"
"\x28\x1c\x40\xaa\xbe\x3c\x3f\xe7\xbe\x43\xd3\xc5\x41\xab\x98\xc9"
"\x83\x24\x75\x6b\xd0\xbd\x51\x0c\xbc\x09\xf7\x06\xab\x74\x63\x71"
"\x9d\xf5\x02\x29\xf4\x13\xc2\x51\x5a\x52\xd9\x11\xfe\x01\x7c\xaa"
"\x45\xff\x02\x83\x50\xbb\xcd\x99\xc1\x0e\xfd\x2d\x68\x5c\x56\x8d"
"\x4c\x97\x60\x08\xbb\x49\x22\x76\xb5\x8a\xc6\xc1\xe1\x55\xb5\x2c"
"\xc7\x41\x52\xa4\xb7\x9f\xee\x79\x9e\x1f\x01\x82\x32\x4a\x93\x0f"
"\x17\x45\xbd\xda\xf5\x34\xc4\x35\x46\xdd\x0b\xd8\x04\x25\xce\x2a"
"\x1f\x6a\x74\x37\x33\xb8\xfa\x5a\x32\xb4\x77\x79\x4a\xaf\xcf\x94"
"\xc3\xcb\x04\x41\x77\x5b\xc8\xfd\x5a\x36\x1c\x24\x60\x44\x6d\x56"
"\xb2\x26\x64\xef\xbb\x14\x20\x8e\xb1\xdc\xc1\x65\xc3\xcf\x63\xb0"
"\xf7\xbc\xd2\xcb\xd1\x6e\x72\x31\x06\xd2\xc8\x39\x34\x22\x2f\xc1"
"\x16\x4e\xa4\x52\xd0\x15\x2b\xec\x2c\x3d\x24\xa2\xbf\x2b\x47\x05"
"\x01\xea\x92\x9e\x0a\x29\xda\xe1\x0f\x66\x93\xc2\x0c\x5f\x4a\xa6"
"\x54\x33\x98\x40\xf7\x46\x9e\x59\x52\x39\x0f\x8a\x18\xef\xeb\x4f"
"\x33\xaf\x2a\xaf\xff\x32\x9f\x13\xf2\xd2\xf6\x42\x68\x85\xbb\x08"
;
static const char tpm_quote_valid[KRSA_TPM_QUOTE_VALID_LEN] =
"\x01\x01\x00\x00\x51\x55\x4f\x54\xb0\xbc\x37\x52\x01\x21\x46\x77"
"\xe8\x4b\xb1\xee\x7c\x4f\xb1\xb3\xbd\x01\x6e\x90\x01\x01\x01\x01"
"\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
;
static const char tpm_aik_pub_key_n[KRSA_TPM_AIK_PUB_KEY_N_LEN] =
"\xc4\x61\x13\x5f\xa6\x0f\xa8\x0a\x8a\xeb\x4b\x35\xa3\x8f"
"\x48\xe8\x7e\x51\xf4\x2b\x6f\x3b\x9e\x68\xdf\x51\xee\xa1\x4e"
"\x87\x77\x71\x51\x2c\x95\x75\x35\x68\xd2\x14\x4e\x29\xef\x0e"
"\x01\x91\x58\x18\x83\x2f\x6b\x6b\xbd\xf1\xf6\x03\x77\x4c\x96"
"\xff\x40\x78\x56\x66\x97\xea\xde\x2a\x0d\x69\xa4\x09\xd7\x29"
"\x3f\x40\x7d\xcd\x5f\xb4\x42\x9d\xcb\x22\x95\x8f\x31\x22\x19"
"\x2e\x7d\xfa\xfa\x40\x41\xb3\xe4\x14\x7d\x7e\xcd\x5f\xde\x1d"
"\x67\x35\xd1\xbd\xe3\xe7\x00\x12\xd0\x82\xe6\x31\x94\xe8\xff"
"\x45\xc4\xb8\x69\xfc\x0e\x8d\xd2\x90\x89\x0d\x44\xe9\xf7\xe5"
"\xf4\x93\xee\x19\x63\x3a\xfc\x9f\x0d\xee\x96\x1f\xac\xfb\x9f"
"\xcd\x6e\x00\xd0\x75\x86\xd1\x0f\x3b\xba\x71\x08\x28\xf9\xd5"
"\x7f\x33\xed\x5d\x2e\x11\x93\x26\x84\x1a\x0d\x94\x36\xc4\x04"
"\x67\xbb\xf5\x92\xdd\xf5\x26\x5e\x45\xe8\x52\x7d\xcb\x13\x8b"
"\x06\x78\x81\x33\x13\x9a\x4b\xfe\x4e\xe3\x24\xae\xd2\xc2\x3c"
"\xb9\x2a\xef\x97\x34\x2f\x5b\x8b\x3d\x96\xe3\x94\x9e\x32\x08"
"\x39\x44\x2f\x09\x15\x88\x8f\x9e\xed\x74\xbc\xbe\x3f\xc4\x98"
"\x53\x5d\xff\x80\xb0\x06\xb3\xed\x09\x12\xc0\xce\x90\x87\x8a"
"\x6a\x29"
;
static const char tpm_aik_pub_key_e[KRSA_TPM_AIK_PUB_KEY_E_LEN] =
"\x01\x00\x01"
;

/* Helpers */
/* Based on kernel/module_signing.c */
static int krsa_make_digest(struct krsa_ctx *ctx, enum hash_algo hash)
{
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	size_t digest_size;
	int ret;

	if (krsa_debug)
		pr_info("==>%s()\n", __func__);

	/* Allocate the hashing algorithm we're going to need and find out how
	 * big the hash operational data will be.
	 */
	tfm = crypto_alloc_shash(hash_algo_name[hash], 0, 0);
	if (IS_ERR(tfm))
		return (PTR_ERR(tfm) == -ENOENT) ? ERR_PTR(-ENOPKG) : ERR_CAST(tfm);

	digest_size = crypto_shash_digestsize(tfm);

	if (krsa_debug)
		pr_info("hash [%d], digest_size [%d]\n", hash, digest_size);

	ret = -ENOMEM;
//daveti
	//ctx->digest = kmalloc(digest_size, GFP_KERNEL);
	ctx->digest = kmalloc(digest_size, GFP_ATOMIC);
	if (!ctx->digest)
		goto error_no_digest;

	ctx->digest_algo = hash;
	ctx->digest_size = digest_size;

	/* daveti: NOTE, desc will use the following memory for digest! */
//daveti
	//desc = kzalloc(sizeof(*desc)+digest_size, GFP_KERNEL);
	desc = kzalloc(sizeof(*desc)+digest_size, GFP_ATOMIC);
	if (!desc)
		goto error_no_desc;

	desc->tfm = tfm;
	/* daveti: no sleep
	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	*/
	desc->flags = 0;

	if (krsa_debug_digest_step) {
		pr_info("Stop before crypto_shash_init()\n");
		return 0;
	}

	ret = crypto_shash_init(desc);
	if (ret < 0)
		goto error;

        if (krsa_debug_digest_step) {
                pr_info("Stop before print_hex_dump() for valid\n");
                return 0;
        }

	if (krsa_debug)
		print_hex_dump(KERN_INFO, "valid_", DUMP_PREFIX_ADDRESS, 16, 1,
				ctx->valid, ctx->valid_size, 1);

	if (krsa_debug_digest_step) {
		pr_info("Stop before crypto_shash_finup()\n");
		return 0;
	}

	ret = crypto_shash_finup(desc, ctx->valid, ctx->valid_size, ctx->digest);
	if (ret < 0)
		goto error;

	if (krsa_debug_digest_step) {
		pr_info("Stop before crypto_free_shash()\n");
		return 0;
	}

	crypto_free_shash(tfm);
	kfree(desc);

	if (krsa_debug)
		pr_info("<==%s() = ok\n", __func__);

	return 0;

error:
	kfree(desc);
error_no_desc:
	kfree(ctx->digest);
error_no_digest:
	crypto_free_shash(tfm);
	pr_err("<==%s() = %d\n", __func__, ret);
	return ret;
}

/*
 * Extract an MPI array from the signature data.  This represents the actual
 * signature.  Each raw MPI is prefaced by a BE 2-byte value indicating the
 * size of the MPI in bytes.
 *
 * RSA signatures only have one MPI, so currently we only read one.
 *
 * daveti: NOT used!
 */
static int krsa_extract_mpi_sig(struct krsa_ctx *ctx,
				const void *data, size_t len)
{
	size_t nbytes;
	MPI mpi;

	if (len < 3)
		return -EBADMSG;
	nbytes = ((const u8 *)data)[0] << 8 | ((const u8 *)data)[1];
	data += 2;
	len -= 2;
	if (len != nbytes)
		return -EBADMSG;

	mpi = mpi_read_raw_data(data, nbytes);
	if (!mpi)
		return -ENOMEM;
	ctx->sig_mpi = mpi;
	return 0;
}

static void krsa_ctx_destory(struct krsa_ctx *ctx)
{
	if (!ctx)
		return;
	if (ctx->digest)
		kfree(ctx->digest);
	if (ctx->sig)
		kfree(ctx->sig);
	if (ctx->valid)
		kfree(ctx->valid);
	if (ctx->sig_mpi)
		mpi_free(ctx->sig_mpi);
	if (ctx->rsa_pub_key.e)
		mpi_free(ctx->rsa_pub_key.e);
	if (ctx->rsa_pub_key.n)
		mpi_free(ctx->rsa_pub_key.n);
}

static int krsa_ctx_init(struct krsa_ctx *ctx,
			u8 *sig, unsigned int sig_len,
			u8 *valid, unsigned int valid_len,
			u8 *rsa_n, unsigned int rsa_n_len,
			u8 *rsa_e, unsigned int rsa_e_len)
{
	MPI mpi;

	if (krsa_debug) {
		print_hex_dump(KERN_INFO, "sig_", DUMP_PREFIX_ADDRESS, 16, 1,
				sig, sig_len, 1);
		print_hex_dump(KERN_INFO, "valid_", DUMP_PREFIX_ADDRESS, 16, 1,
				valid, valid_len, 1);
		print_hex_dump(KERN_INFO, "rsa_n_", DUMP_PREFIX_ADDRESS, 16, 1,
				rsa_n, rsa_n_len, 1);
		print_hex_dump(KERN_INFO, "rsa_e_", DUMP_PREFIX_ADDRESS, 16, 1,
				rsa_e, rsa_e_len, 1);
	}

	/* Init */
	memset(ctx, 0x0, sizeof(*ctx));

	/* Init signature */
//daveti
	//ctx->sig = kmalloc(sig_len, GFP_KERNEL);
	ctx->sig = kmalloc(sig_len, GFP_ATOMIC);
	if (!ctx->sig) {
		pr_err("kmalloc for ctx sig failed\n");
		return -ENOMEM;
	}
	memcpy(ctx->sig, sig, sig_len);
	ctx->sig_size = sig_len;
	mpi = mpi_read_raw_data(sig, sig_len);
	if (!mpi) {
		pr_err("mpi_read_raw_data for sig failed\n");
		goto error;
	}
	ctx->sig_mpi = mpi;

	/* Init valid data */
//daveti
	//ctx->valid = kmalloc(valid_len, GFP_KERNEL);
	ctx->valid = kmalloc(valid_len, GFP_ATOMIC);
	if (!ctx->valid) {
		pr_err("kmalloc for ctx valid failed\n");
		goto error;
	}
	memcpy(ctx->valid, valid, valid_len);
	ctx->valid_size = valid_len;

	/* Init RSA public key - n */
	mpi = mpi_read_raw_data(rsa_n, rsa_n_len);
	if (!mpi) {
		pr_err("mpi_read_raw_data for rsa.n failed\n");
		goto error;
	}
	ctx->rsa_pub_key.n = mpi;

	/* Init RSA public key - e */
	mpi = mpi_read_raw_data(rsa_e, rsa_e_len);
	if (!mpi) {
		pr_err("mpi_read_raw_data for rsa.e failed\n");
		goto error;
	}
	ctx->rsa_pub_key.e = mpi;

	return 0;

error:
	krsa_ctx_destory(ctx);
	memset(ctx, 0x0, sizeof(*ctx));
	return -ENOMEM;
}

/* RSA implementations */
/* Ported from crypto/asymmetric_key/rsa.c */
/*
 * RSAVP1() function [RFC3447 sec 5.2.2]
 */
static int RSAVP1(MPI n, MPI e, MPI s, MPI *_m)
{
	MPI m;
	int ret;

	/* (1) Validate 0 <= s < n */
	if (mpi_cmp_ui(s, 0) < 0) {
		kleave(" = -EBADMSG [s < 0]");
		return -EBADMSG;
	}
	if (mpi_cmp(s, n) >= 0) {
		kleave(" = -EBADMSG [s >= n]");
		return -EBADMSG;
	}

	m = mpi_alloc(0);
	if (!m)
		return -ENOMEM;

	/* (2) m = s^e mod n */
	ret = mpi_powm(m, s, e, n);
	if (ret < 0) {
		mpi_free(m);
		return ret;
	}

	*_m = m;
	return 0;
}

/*
 * Integer to Octet String conversion [RFC3447 sec 4.1]
 */
static int RSA_I2OSP(MPI x, size_t xLen, u8 **_X)
{
	unsigned X_size, x_size;
	int X_sign;
	u8 *X;

	/* Make sure the string is the right length.  The number should begin
	 * with { 0x00, 0x01, ... } so we have to account for 15 leading zero
	 * bits not being reported by MPI.
	 */
	x_size = mpi_get_nbits(x);
	if (krsa_debug)
		pr_info("size(x)=%u xLen*8=%zu\n", x_size, xLen * 8);
	if (x_size != xLen * 8 - 15)
		return -ERANGE;

	X = mpi_get_buffer(x, &X_size, &X_sign);
	if (!X)
		return -ENOMEM;
	if (X_sign < 0) {
		kfree(X);
		return -EBADMSG;
	}
	if (X_size != xLen - 1) {
		kfree(X);
		return -EBADMSG;
	}

	*_X = X;
	return 0;
}

/*
 * Perform the RSA signature verification.
 * @H: Value of hash of data and metadata
 * @EM: The computed signature value
 * @k: The size of EM (EM[0] is an invalid location but should hold 0x00)
 * @hash_size: The size of H
 * @asn1_template: The DigestInfo ASN.1 template
 * @asn1_size: Size of asm1_template[]
 */
static int RSA_verify(const u8 *H, const u8 *EM, size_t k, size_t hash_size,
			const u8 *asn1_template, size_t asn1_size)
{
	unsigned PS_end, T_offset, i;

	kenter(",,%zu,%zu,%zu", k, hash_size, asn1_size);

	if (k < 2 + 1 + asn1_size + hash_size)
		return -EBADMSG;

	/* Decode the EMSA-PKCS1-v1_5 */
	if (EM[1] != 0x01) {
		kleave(" = -EBADMSG [EM[1] == %02u]", EM[1]);
		return -EBADMSG;
	}

	T_offset = k - (asn1_size + hash_size);
	PS_end = T_offset - 1;
	if (EM[PS_end] != 0x00) {
		kleave(" = -EBADMSG [EM[T-1] == %02u]", EM[PS_end]);
		return -EBADMSG;
	}

	for (i = 2; i < PS_end; i++) {
		if (EM[i] != 0xff) {
			kleave(" = -EBADMSG [EM[PS%x] == %02u]", i - 2, EM[i]);
			return -EBADMSG;
		}
	}

	if (crypto_memneq(asn1_template, EM + T_offset, asn1_size) != 0) {
		kleave(" = -EBADMSG [EM[T] ASN.1 mismatch]");
		return -EBADMSG;
	}

	if (crypto_memneq(H, EM + T_offset + asn1_size, hash_size) != 0) {
		if (krsa_debug) {
			print_hex_dump(KERN_INFO, "H_", DUMP_PREFIX_ADDRESS, 16, 1,
                                H, hash_size, 1);
			print_hex_dump(KERN_INFO, "EMTa_", DUMP_PREFIX_ADDRESS, 16, 1,
				(EM+T_offset+asn1_size), hash_size, 1);
		}
		kleave(" = -EKEYREJECTED [EM[T] hash mismatch]");
		return -EKEYREJECTED;
	}

	kleave(" = 0");
	return 0;
}

/* Final wrapper 
 * Perform the verification step [RFC3447 sec 8.2.2].
 */
int krsa_verify(struct krsa_ctx *ctx)
{
	size_t tsize;
	int ret;

	/* Variables as per RFC3447 sec 8.2.2 */
	const u8 *H = ctx->digest;
	u8 *EM = NULL;
	MPI m = NULL;
	size_t k;

	kenter("");

	if (!RSA_ASN1_templates[ctx->digest_algo].data)
		return -ENOTSUPP;

	/* (1) Check the signature size against the public key modulus size */
	k = mpi_get_nbits(ctx->rsa_pub_key.n);
	tsize = mpi_get_nbits(ctx->sig_mpi);

	/* According to RFC 4880 sec 3.2, length of MPI is computed starting
	 * from most significant bit.  So the RFC 3447 sec 8.2.2 size check
	 * must be relaxed to conform with shorter signatures - so we fail here
	 * only if signature length is longer than modulus size.
	 */
	if (krsa_debug)
		pr_info("step 1: k=%zu size(S)=%zu\n", k, tsize);
	if (k < tsize) {
		ret = -EBADMSG;
		goto error;
	}

	/* Round up and convert to octets */
	k = (k + 7) / 8;

	/* (2b) Apply the RSAVP1 verification primitive to the public key */
	ret = RSAVP1(ctx->rsa_pub_key.n, ctx->rsa_pub_key.e, ctx->sig_mpi, &m);
	if (ret < 0)
		goto error;

	/* (2c) Convert the message representative (m) to an encoded message
	 *      (EM) of length k octets.
	 *
	 *      NOTE!  The leading zero byte is suppressed by MPI, so we pass a
	 *      pointer to the _preceding_ byte to RSA_verify()!
	 */
	ret = RSA_I2OSP(m, k, &EM);
	if (ret < 0)
		goto error;

	ret = RSA_verify(H, EM - 1, k, ctx->digest_size,
			RSA_ASN1_templates[ctx->digest_algo].data,
			RSA_ASN1_templates[ctx->digest_algo].size);

error:
	kfree(EM);
	mpi_free(m);
	kleave(" = %d", ret);
	return ret;
}
EXPORT_SYMBOL(krsa_verify);

int krsa_init(struct krsa_ctx *ctx,
		char *tpm_quote_sig, int sig_len,
		char *tpm_quote_valid, int valid_len,
		char *tpm_aik_pub_key_n, int pubkey_n_len,
		char *tpm_aik_pub_key_e, int pubkey_e_len)
{
	int ret;

	pr_info("Entering: %s\n", __func__);

	/* Init */
	ret = krsa_ctx_init(ctx,
			(u8 *)tpm_quote_sig, sig_len,
			(u8 *)tpm_quote_valid, valid_len,
			(u8 *)tpm_aik_pub_key_n, pubkey_n_len,
			(u8 *)tpm_aik_pub_key_e, pubkey_e_len);
	if (ret) {
		pr_err("RSA ctx init failed\n");
		return ret;
	}

	if (krsa_debug_step) {
		pr_info("Stop before krsa_make_digest()\n");
		return 0;
	}

	/* Digest */
	ret = krsa_make_digest(ctx, HASH_ALGO_SHA1);
	if (ret) {
		pr_err("SHA1 digest failed\n");
		return ret;
	}
	if (krsa_debug)
		print_hex_dump(KERN_INFO, "digest_", DUMP_PREFIX_ADDRESS, 16, 1,
				ctx->digest, ctx->digest_size, 1);

	if (krsa_debug_step) {
		pr_info("Stop before krsa_verify()\n");
		return 0;
	}

	if (krsa_debug)
		pr_info("%s() done\n", __func__);

	return 0;
}
EXPORT_SYMBOL(krsa_init);

void krsa_exit(struct krsa_ctx *ctx)
{
	pr_info("exiting krsa module\n");
	krsa_ctx_destory(ctx);
}
EXPORT_SYMBOL(krsa_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Krsa Module");
MODULE_AUTHOR("daveti");
