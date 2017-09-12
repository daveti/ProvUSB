/*
 * tpmw.h
 * Header file for tpmw
 * Feb 23, 2014
 * Add support for nltpmd and comment AT protocol related
 * daveti
 * Sep 16, 2013
 * root@davejingtian.org
 * http://davejingtian.org
 *
 * Updated on Aug 18, 2014
 * abdul@cs.uoregon.edu
 */

#ifndef TPMW_INCLUDE
#define TPMW_INCLUDE

#include <trousers/tss.h>
#include <trousers/trousers.h>
#include "nlm.h"
#include <sys/time.h>
#include <time.h>

/* TPM local definitions
NOTE: this should be configurable in future.
But now, just let it be:)
NOTE: the SRK passwd are shared between arpsecd and tpmd.
Have to make 2 builds if the passwds are different...
(I know, it sounds stupid. But - K.I.S.S.)
*/
#define TPMW_SRK_PASSWD         "00000000000000000000"
#define TPMW_TSS_UUID_AIK       {0, 0, 0, 0, 0, {0, 0, 0, 0, 2, 0}}
#define TPMW_PCR_NUM_MAX        24
#define TPMW_PCR_BYTE           2
#define TPMW_PCR_LEN            20
#define TPMW_PCR_MASK_LEN       3
#define TPMW_NONCE_LEN          20
#define TPMW_AIK_PKEY_LEN       284
#define TPMW_PCR_DIGEST_LEN     20
#define TPMW_NUM_PER_LINE       20
#define TIMER_START             0
#define TIMER_STOP              1

/* Init the tpmw with TPM */
int tpmw_init_tpm(void);

/* Close the TPM */
void tpmw_close_tpm(void);

/* Clear the global records */
void tpmw_clear_global_records(void);

/* Main method to process request */
int tpmw_req_handler(nlmsgt *rep, nlmsgt *req, int fake);

/* Get the quote using AIK */
TSS_VALIDATION *tpmw_get_quote_with_aik(void);

/* Generate the reply based on validation struct */
void tpmw_generate_rep(nlmsgt *rep, TSS_VALIDATION *valid);

/* Generate the nonce locally */
int tpmw_generate_nonce(unsigned char *nonce);

/* Display the PCRs */
void tpmw_display_pcrs();

/* Display the TSS validation structure */
void tpmw_display_validation(TSS_VALIDATION *valid);

/* Display the uchar with better format */
void tpmw_display_uchar(unsigned char *src, int len, char *header);

/* Get the PCR value based on the PCR mask */
int tpmw_get_pcr_value(void);

/* Get the AIK public key */
unsigned char *tpmw_get_aik_pkey(void);

/* Generate the fake reply - for UT */
void tpmw_generate_fake_rep(nlmsgt *rep);

/* Measure the execution time of a function call */
void tpmw_timer(int action, int func);

/* Handle timer files */
void tpmw_timer_files(int action);

#endif
