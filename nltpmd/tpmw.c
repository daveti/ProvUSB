/*
 * tpmw.c
 * Source file for tpmw
 *
 * Feb 23, 2014
 * Added support for nltpmd and comment things for AT
 * daveti
 *
 * TPM worker (tpmw) is used to pass the AT request/reply
 * to the local TPM (tcsd) and generate the corresponding
 * AT reply or msg validation.
 * NOTE: NOT thread-safe!
 * Sep 16, 2013
 * root@davejingtian.org
 * http://davejingtian.org
 *
 * Updated on Aug 18, 2014
 * abdul@cs.uoregon.edu
 */

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <trousers/tss.h>
#include <trousers/trousers.h>
#include "tpmw.h"

/* Global definition for TPM info and resource */
static unsigned char tpmw_nonce[TPMW_NONCE_LEN];
static unsigned char tpmw_pcr_mask[TPMW_PCR_NUM_MAX];
static unsigned char tpmw_pcr_value[TPMW_PCR_NUM_MAX][TPMW_PCR_LEN];

/* To accelerate the speed of talking to TPM
 * Only 1 context will be created and will be kept
 * alive while tpmd/arpsecd is running.
 * -daveti
 */
static int                  debug_enabled = 0;
static TSS_HCONTEXT         hContext;
static TSS_HTPM             hTPM;
static TSS_HKEY             hIdentKey; 
static unsigned char        *tpmw_pcr_hash_buf;
static unsigned char        *tpmw_aik_pub_key;
static int                  tpmw_aik_pub_key_len;

/* Timer related variables and definitions */
static int                  timer_enabled = 0;
static FILE                 *timer_files[10]; 
#define QUOTE_FILE          0
#define PCR_READ_FILE       1
#define GET_AIK_FILE        2


/* Init the tpmw with TPM */
int tpmw_init_tpm()
{
    TSS_HKEY        hSRK;
    TSS_RESULT      result;
    TSS_UUID        SRK_UUID = TSS_UUID_SRK;
    TSS_UUID        AIK_UUID = TPMW_TSS_UUID_AIK;
    TSS_HPOLICY     hSrkPolicy;
    UINT32          pulPubKeyLength;
    BYTE            *prgbPubKey;
    FILE            *f_out;
    char            out_fname[] = "aik_pub.key";

    if (timer_enabled)
        tpmw_timer_files(1);

    /* Trousers preamble */
    result = Tspi_Context_Create(&hContext);
    if (result != TSS_SUCCESS) {
        printf ("Tspi_Context_Create failed [%s]\n",
                Trspi_Error_String(result));
        return -1;
    }

    result = Tspi_Context_Connect(hContext, NULL);
    if (result != TSS_SUCCESS) {
        printf ("Tspi_Context_Connect failed [%s]\n",
                Trspi_Error_String(result));
        goto close;
    }

    result = Tspi_Context_GetTpmObject(hContext, &hTPM);
    if (result != TSS_SUCCESS) {
        printf ("Tspi_Context_GetTpmObject failed [%s]\n",
                Trspi_Error_String(result));
        goto close;
    }

    result = Tspi_Context_LoadKeyByUUID(hContext,
            TSS_PS_TYPE_SYSTEM, SRK_UUID, &hSRK);
    if (result != TSS_SUCCESS) {
        printf ("Tspi_Context_LoadKeyByUUID for SRK failed [%s]\n",
                Trspi_Error_String(result));
        goto close;
    }

    result = Tspi_GetPolicyObject(hSRK, TSS_POLICY_USAGE, &hSrkPolicy);
    if (result != TSS_SUCCESS) {
        printf ("Tspi_GetPolicyObject for SRK failed [%s]\n",
                Trspi_Error_String(result));
        goto close;
    }

    result = Tspi_Policy_SetSecret(hSrkPolicy, TSS_SECRET_MODE_PLAIN, 20, (BYTE *)TPMW_SRK_PASSWD);
    if (result != TSS_SUCCESS) {
        printf ("Tspi_Policy_SetSecret for SRK failed [%s]\n",
                Trspi_Error_String(result));
        goto close;
    }

    /* Load the AIK into the context if this is for tpmd */
    result = Tspi_Context_LoadKeyByUUID(hContext,
            TSS_PS_TYPE_SYSTEM, AIK_UUID, &hIdentKey);
    if (result != TSS_SUCCESS) {
        printf ("Tspi_Context_LoadKeyByUUID for AIK failed [%s]\n",
                Trspi_Error_String(result));
        goto close;
    }

    /* Get the pub key */
    if (timer_enabled)
        tpmw_timer(TIMER_START, GET_AIK_FILE);
    result = Tspi_Key_GetPubKey(hIdentKey, &pulPubKeyLength, &prgbPubKey);
    if (timer_enabled)
        tpmw_timer(TIMER_STOP, GET_AIK_FILE);

   if (result != TSS_SUCCESS)
    {
        printf("Tspi_Key_GetPubKey failed [%s]\n",
                Trspi_Error_String(result));
        goto close;
    }

    if (debug_enabled == 1) {

        /* Output the pub key of AIK */
        tpmw_display_uchar(prgbPubKey, pulPubKeyLength, "tpmw - AIK pub key:");

        /* Save AIK pub key to a file */
        if ((f_out = fopen (out_fname, "wb")) == NULL) {
            printf ("tpmw - Unable to open %s for output\n", out_fname);
            exit (1);
        }
        printf("tpmw - writing AIK public key to the file %s\n", out_fname);
        fwrite (prgbPubKey, 1, pulPubKeyLength, f_out);
        fflush (f_out);
        fclose (f_out);
    }

    if (pulPubKeyLength != TPMW_AIK_PKEY_LEN)
        printf("tpmw - AIK public key legth is not 284\n");

    /* Copy the pub key into global buffer */
    tpmw_aik_pub_key_len = pulPubKeyLength;
    tpmw_aik_pub_key = malloc(pulPubKeyLength);  
    memcpy(tpmw_aik_pub_key, prgbPubKey, pulPubKeyLength); 
    //Tspi_Context_FreeMemory(hContext, prgbPubKey);

    return 0;
close:
    tpmw_close_tpm();
    return -1;
}

/* Close the TPM */
void tpmw_close_tpm(void)
{
    if (tpmw_pcr_hash_buf != NULL) 
        free(tpmw_pcr_hash_buf);
    if (tpmw_aik_pub_key != NULL)
        free(tpmw_aik_pub_key);

    Tspi_Context_FreeMemory(hContext, NULL);
    Tspi_Context_Close(hContext);

    if (timer_enabled)
        tpmw_timer_files(0);
}

/* Clear the global records */
void tpmw_clear_global_records(void)
{
    int i;

    memset(tpmw_nonce, 0, TPMW_NONCE_LEN);
    memset(tpmw_pcr_mask, 0, TPMW_PCR_NUM_MAX);
    for (i = 0; i < TPMW_PCR_NUM_MAX; i++)
    {
        memset(tpmw_pcr_value[i], 0, TPMW_PCR_LEN);
    }
}

/* Main method to process request  - tpmd */
int tpmw_req_handler(nlmsgt *rep, nlmsgt *req, int fake)
{
    TSS_VALIDATION *valid;
    TPM_QUOTE_INFO *quote;

    /* Check if this is UT */
    if (fake == 1)
    {
        printf("tpmw - reply will be faked\n");
        tpmw_generate_fake_rep(rep);
        return 0;
    }

    /* Clear the global records */
    tpmw_clear_global_records();

    /* Get the PCR mask from the AT request */
    bit_mask_to_byte(tpmw_pcr_mask, req->request + NLM_NONCE_LEN, TPMW_PCR_MASK_LEN);
    //memcpy(tpmw_pcr_mask, req->request, TPMW_PCR_NUM_MAX);

    /* Display the PCR mask for debug */
    if (debug_enabled == 1)
        tpmw_display_uchar(tpmw_pcr_mask, TPMW_PCR_NUM_MAX, "tpmw - got PCR:");

    /* Get the nonce from the AT request */
    memcpy(tpmw_nonce, req->request, TPMW_NONCE_LEN);

    /* Display the nonce for debug */
    if (debug_enabled == 1)
        tpmw_display_uchar(tpmw_nonce, TPMW_NONCE_LEN, "tpmw - got nonce:");

    /* Display the PCR value for debug */
    if (debug_enabled == 1)
        if (tpmw_get_pcr_value() == 0)
            tpmw_display_pcrs();

    /* Get the quote using AIK */
    valid = tpmw_get_quote_with_aik();
    if (valid == NULL)
    {
        printf("tpmw - Error on tpmw_get_quote_with_aik\n");
        return -1;
    }

    /* Display the validation structure for debug */
    if (debug_enabled == 1)
    {
        /* Get the digest of PCRs value */
        quote = (TPM_QUOTE_INFO *)valid->rgbData;
        tpmw_display_uchar(quote->compositeHash.digest, TPMW_NONCE_LEN, "PCRs value digest:");
        tpmw_display_validation(valid);

        /* Verify the digest locally */
        /* This may be needed - daveti */
    }

    /* Write the reply back */
    tpmw_generate_rep(rep, valid);

    /* Free the valid struct */
    //Tspi_Context_FreeMemory(hContext, valid->rgbValidationData);
    //Tspi_Context_FreeMemory(hContext, valid->rgbData);
    free(valid);

    return 0;
}

/* Get the quote using AIK */
TSS_VALIDATION *tpmw_get_quote_with_aik(void)
{
    TSS_RESULT result;
    TSS_HPCRS hPcrComposite;
    TSS_VALIDATION valid;
    TSS_VALIDATION *rtn = NULL;
    int i;

    /* Create the PCR Composite object for quote */
    result = Tspi_Context_CreateObject(hContext,
            TSS_OBJECT_TYPE_PCRS,
            0,
            &hPcrComposite);
    if (result != TSS_SUCCESS)
    {
        printf("Tspi_Context_CreateObject failed for PCR Composite [%s]\n",
                Trspi_Error_String(result));
        return NULL;
    }

    /* Set the quoted PCR index */
    for (i = 0; i < TPMW_PCR_NUM_MAX; i++)
    {
        if (tpmw_pcr_mask[i] == 1)
        {
            result = Tspi_PcrComposite_SelectPcrIndex(hPcrComposite, i);

            if (result != TSS_SUCCESS)
            {
                printf("Tspi_PcrComposite_SelectPcrIndex failed for index [%d] [%s]\n",
                        i, Trspi_Error_String(result));
                rtn = NULL;
                goto close;
            }
        }
    }

    /* Set the input for validation struct */
    valid.ulExternalDataLength = TPMW_NONCE_LEN;
    valid.rgbExternalData = tpmw_nonce;

    if (timer_enabled) 
        tpmw_timer(TIMER_START, QUOTE_FILE);

    /* Do the damn quote */
    result = Tspi_TPM_Quote(hTPM,                           /* in */
            hIdentKey,                      /* in */
            hPcrComposite,                 /* in */
            &valid);        /* in, out */

    if (timer_enabled) 
        tpmw_timer(TIMER_STOP, QUOTE_FILE);

    if (result != TSS_SUCCESS)
    {
        printf("Tspi_TPM_Quote failed [%s]\n", Trspi_Error_String(result));
        rtn = NULL;
        goto close;
    }

    /* Save the validation */
    rtn = (TSS_VALIDATION *)malloc(sizeof(TSS_VALIDATION));
    memcpy(rtn, &valid, sizeof(TSS_VALIDATION));

close:
    Tspi_Context_CloseObject(hContext, hPcrComposite);
    return rtn;
}

/* Generate the reply based on validation struct */
void tpmw_generate_rep(nlmsgt *rep, TSS_VALIDATION *valid)
{
    /* Make the header */
    rep->opcode = NLM_MSG_REP;

    /* Copy the data */
    if (valid->ulDataLength != NLM_VALID_LEN)
        printf("tpmw - Error: data len [%d] is different with "
                "AT reply data len [%d]\n",
                valid->ulDataLength,
                NLM_VALID_LEN);
    memcpy(rep->reply, valid->rgbData, NLM_VALID_LEN);

    /* Copy the signature */
    if (valid->ulValidationDataLength != NLM_SIG_LEN)
        printf("tpmw - Error: signature len [%d] is different "
                "with AT reply signature len [%d]\n",
                valid->ulValidationDataLength,
                NLM_SIG_LEN);
    memcpy((rep->reply+NLM_VALID_LEN), valid->rgbValidationData, NLM_SIG_LEN);
}

/* Generate the nonce locally */
int tpmw_generate_nonce(unsigned char *nonce)
{
    TSS_RESULT result;
    BYTE *prgbRandomData;

    result = Tspi_TPM_GetRandom(hTPM,
            TPMW_NONCE_LEN,
            &prgbRandomData);
    if (result != TSS_SUCCESS)
    {
        printf("Tspi_TPM_GetRandom failed [%s]\n",
                Trspi_Error_String(result));
        return -1;
    }

    memcpy(nonce, prgbRandomData, TPMW_NONCE_LEN);
    return 0;
}

/* Display the PCRs */
void tpmw_display_pcrs()
{
    int i;

    printf("tpmw - PCRs:\n");
    for(i = 0; i < TPMW_PCR_NUM_MAX; i++)
    {
        if (tpmw_pcr_mask[i] == 1)
        {
            printf("PCR-%02d: ", i);
            tpmw_display_uchar(tpmw_pcr_value[i], TPMW_PCR_LEN, NULL);
        }
    }
}

/* Display the TSS validation structure */
void tpmw_display_validation(TSS_VALIDATION *valid)
{
    printf("Validation struct:\n");
    printf("ulExternalDataLength = %u\n", valid->ulExternalDataLength);
    tpmw_display_uchar(valid->rgbExternalData, valid->ulExternalDataLength, "ExternalData:");
    printf("ulDataLength = %u\n", valid->ulDataLength);
    tpmw_display_uchar(valid->rgbData, valid->ulDataLength, "Data:");
    printf("ulValidationDataLength = %u\n", valid->ulValidationDataLength);
    tpmw_display_uchar(valid->rgbValidationData, valid->ulValidationDataLength, "ValidationData:");
}

/* Display the uchar with better format */
void tpmw_display_uchar(unsigned char *src, int len, char *header)
{
    int i;
    int new_line;

    if (header != NULL)
        printf("%s\n", header);

    for (i = 0; i < len; i++)
    {
        if ((i+1) % TPMW_NUM_PER_LINE != 0)
        {
            printf("%02x ", src[i]);
            new_line = 0;
        }
        else
        {
            printf("%02x\n", src[i]);
            new_line = 1;
        }
    }

    if (new_line == 0)
        printf("\n");
}

/* Get the PCR value based on the PCR mask */
int tpmw_get_pcr_value(void)
{
    int i;
    UINT32 ulPcrLen;
    BYTE *rgbPcrValue;
    TSS_RESULT result;

    for(i = 0; i < TPMW_PCR_NUM_MAX; i++)
    {
        if (tpmw_pcr_mask[i] == 1)
        {
            if (timer_enabled) 
                tpmw_timer(TIMER_START, PCR_READ_FILE);

            result = Tspi_TPM_PcrRead(hTPM, i, &ulPcrLen, &rgbPcrValue);

            if (timer_enabled) 
                tpmw_timer(TIMER_STOP, PCR_READ_FILE);

            if (result != TSS_SUCCESS)
            {
                printf("Tspi_TPM_PcrRead failed for PCR [%u] [%s]\n",
                        i, Trspi_Error_String(result));
                return -1;
            }

            /* Copy the value into static mem */
            if (ulPcrLen != TPMW_PCR_LEN)
            {
                printf("daveti: Is this possible?\n");
                return -1;
            }

            memcpy(tpmw_pcr_value[i], rgbPcrValue, TPMW_PCR_LEN);
            //Tspi_Context_FreeMemory(hContext, rgbPcrValue);
        }
    }
    return 0;
}

/* Get AIK pub key */
unsigned char *tpmw_get_aik_pkey(void)
{
    return tpmw_aik_pub_key;
}

/* Generate a fake reply - for UT */
void tpmw_generate_fake_rep(nlmsgt *rep)
{
    /* Hard code the reply to be all zeros */
    memset(rep, 0, sizeof(nlmsgt));
    rep->opcode = 2;
}

/* Measure the execution time of a function call */
void tpmw_timer(int action, int func)
{
    static double time_spent;
    static struct timeval start, stop;

    if (action == TIMER_START)
    {
        gettimeofday(&start, NULL);
    }
    else if (action == TIMER_STOP)
    {
        gettimeofday(&stop, NULL);
        time_spent = (double)(stop.tv_usec - start.tv_usec) / 1000000 + 
            (double)(stop.tv_sec - start.tv_sec);
        fprintf(timer_files[func], "%f\n", time_spent);
        fflush(timer_files[func]);
    }
}

/* Handle timer files */
void tpmw_timer_files(int action)
{
    if (action)
    {
        timer_files[0] = fopen("quote_time.txt", "a");
        timer_files[1] = fopen("pcrread_time.txt", "a");
        timer_files[2] = fopen("getaik_time.txt", "a");
    }
    else
    {
        fclose(timer_files[0]);
        fclose(timer_files[1]);
        fclose(timer_files[2]);
    }
}

