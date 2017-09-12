/*
 * f_mass_storage.c -- Mass Storage USB Composite Function
 *
 * Copyright (C) 2003-2008 Alan Stern
 * Copyright (C) 2009 Samsung Electronics
 *                    Author: Michal Nazarewicz <mina86@mina86.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The Mass Storage Function acts as a USB Mass Storage device,
 * appearing to the host as a disk drive or as a CD-ROM drive.  In
 * addition to providing an example of a genuinely useful composite
 * function for a USB device, it also illustrates a technique of
 * double-buffering for increased throughput.
 *
 * Function supports multiple logical units (LUNs).  Backing storage
 * for each LUN is provided by a regular file or a block device.
 * Access for each LUN can be limited to read-only.  Moreover, the
 * function can indicate that LUN is removable and/or CD-ROM.  (The
 * later implies read-only access.)
 *
 * MSF is configured by specifying a fsg_config structure.  It has the
 * following fields:
 *
 *	nluns		Number of LUNs function have (anywhere from 1
 *				to FSG_MAX_LUNS which is 8).
 *	luns		An array of LUN configuration values.  This
 *				should be filled for each LUN that
 *				function will include (ie. for "nluns"
 *				LUNs).  Each element of the array has
 *				the following fields:
 *	->filename	The path to the backing file for the LUN.
 *				Required if LUN is not marked as
 *				removable.
 *	->ro		Flag specifying access to the LUN shall be
 *				read-only.  This is implied if CD-ROM
 *				emulation is enabled as well as when
 *				it was impossible to open "filename"
 *				in R/W mode.
 *	->removable	Flag specifying that LUN shall be indicated as
 *				being removable.
 *	->cdrom		Flag specifying that LUN shall be reported as
 *				being a CD-ROM.
 *	->nofua		Flag specifying that FUA flag in SCSI WRITE(10,12)
 *				commands for this LUN shall be ignored.
 *
 *	lun_name_format	A printf-like format for names of the LUN
 *				devices.  This determines how the
 *				directory in sysfs will be named.
 *				Unless you are using several MSFs in
 *				a single gadget (as opposed to single
 *				MSF in many configurations) you may
 *				leave it as NULL (in which case
 *				"lun%d" will be used).  In the format
 *				you can use "%d" to index LUNs for
 *				MSF's with more than one LUN.  (Beware
 *				that there is only one integer given
 *				as an argument for the format and
 *				specifying invalid format may cause
 *				unspecified behaviour.)
 *	thread_name	Name of the kernel thread process used by the
 *				MSF.  You can safely set it to NULL
 *				(in which case default "file-storage"
 *				will be used).
 *
 *	vendor_name
 *	product_name
 *	release		Information used as a reply to INQUIRY
 *				request.  To use default set to NULL,
 *				NULL, 0xffff respectively.  The first
 *				field should be 8 and the second 16
 *				characters or less.
 *
 *	can_stall	Set to permit function to halt bulk endpoints.
 *				Disabled on some USB devices known not
 *				to work correctly.  You should set it
 *				to true.
 *
 * If "removable" is not set for a LUN then a backing file must be
 * specified.  If it is set, then NULL filename means the LUN's medium
 * is not loaded (an empty string as "filename" in the fsg_config
 * structure causes error).  The CD-ROM emulation includes a single
 * data track and no audio tracks; hence there need be only one
 * backing file per LUN.
 *
 *
 * MSF includes support for module parameters.  If gadget using it
 * decides to use it, the following module parameters will be
 * available:
 *
 *	file=filename[,filename...]
 *			Names of the files or block devices used for
 *				backing storage.
 *	ro=b[,b...]	Default false, boolean for read-only access.
 *	removable=b[,b...]
 *			Default true, boolean for removable media.
 *	cdrom=b[,b...]	Default false, boolean for whether to emulate
 *				a CD-ROM drive.
 *	nofua=b[,b...]	Default false, booleans for ignore FUA flag
 *				in SCSI WRITE(10,12) commands
 *	luns=N		Default N = number of filenames, number of
 *				LUNs to support.
 *	stall		Default determined according to the type of
 *				USB device controller (usually true),
 *				boolean to permit the driver to halt
 *				bulk endpoints.
 *
 * The module parameters may be prefixed with some string.  You need
 * to consult gadget's documentation or source to verify whether it is
 * using those module parameters and if it does what are the prefixes
 * (look for FSG_MODULE_PARAMETERS() macro usage, what's inside it is
 * the prefix).
 *
 *
 * Requirements are modest; only a bulk-in and a bulk-out endpoint are
 * needed.  The memory requirement amounts to two 16K buffers, size
 * configurable by a parameter.  Support is included for both
 * full-speed and high-speed operation.
 *
 * Note that the driver is slightly non-portable in that it assumes a
 * single memory/DMA buffer will be useable for bulk-in, bulk-out, and
 * interrupt-in endpoints.  With most device controllers this isn't an
 * issue, but there may be some with hardware restrictions that prevent
 * a buffer from being used by more than one endpoint.
 *
 *
 * The pathnames of the backing files and the ro settings are
 * available in the attribute files "file" and "ro" in the lun<n> (or
 * to be more precise in a directory which name comes from
 * "lun_name_format" option!) subdirectory of the gadget's sysfs
 * directory.  If the "removable" option is set, writing to these
 * files will simulate ejecting/loading the medium (writing an empty
 * line means eject) and adjusting a write-enable tab.  Changes to the
 * ro setting are not allowed when the medium is loaded or if CD-ROM
 * emulation is being used.
 *
 * When a LUN receive an "eject" SCSI request (Start/Stop Unit),
 * if the LUN is removable, the backing file is released to simulate
 * ejection.
 *
 *
 * This function is heavily based on "File-backed Storage Gadget" by
 * Alan Stern which in turn is heavily based on "Gadget Zero" by David
 * Brownell.  The driver's SCSI command interface was based on the
 * "Information technology - Small Computer System Interface - 2"
 * document from X3T9.2 Project 375D, Revision 10L, 7-SEP-93,
 * available at <http://www.t10.org/ftp/t10/drafts/s2/s2-r10l.pdf>.
 * The single exception is opcode 0x23 (READ FORMAT CAPACITIES), which
 * was based on the "Universal Serial Bus Mass Storage Class UFI
 * Command Specification" document, Revision 1.0, December 14, 1998,
 * available at
 * <http://www.usb.org/developers/devclass_docs/usbmass-ufi10.pdf>.
 */

/*
 *				Driver Design
 *
 * The MSF is fairly straightforward.  There is a main kernel
 * thread that handles most of the work.  Interrupt routines field
 * callbacks from the controller driver: bulk- and interrupt-request
 * completion notifications, endpoint-0 events, and disconnect events.
 * Completion events are passed to the main thread by wakeup calls.  Many
 * ep0 requests are handled at interrupt time, but SetInterface,
 * SetConfiguration, and device reset requests are forwarded to the
 * thread in the form of "exceptions" using SIGUSR1 signals (since they
 * should interrupt any ongoing file I/O operations).
 *
 * The thread's main routine implements the standard command/data/status
 * parts of a SCSI interaction.  It and its subroutines are full of tests
 * for pending signals/exceptions -- all this polling is necessary since
 * the kernel has no setjmp/longjmp equivalents.  (Maybe this is an
 * indication that the driver really wants to be running in userspace.)
 * An important point is that so long as the thread is alive it keeps an
 * open reference to the backing file.  This will prevent unmounting
 * the backing file's underlying filesystem and could cause problems
 * during system shutdown, for example.  To prevent such problems, the
 * thread catches INT, TERM, and KILL signals and converts them into
 * an EXIT exception.
 *
 * In normal operation the main thread is started during the gadget's
 * fsg_bind() callback and stopped during fsg_unbind().  But it can
 * also exit when it receives a signal, and there's no point leaving
 * the gadget running when the thread is dead.  At of this moment, MSF
 * provides no way to deregister the gadget when thread dies -- maybe
 * a callback functions is needed.
 *
 * To provide maximum throughput, the driver uses a circular pipeline of
 * buffer heads (struct fsg_buffhd).  In principle the pipeline can be
 * arbitrarily long; in practice the benefits don't justify having more
 * than 2 stages (i.e., double buffering).  But it helps to think of the
 * pipeline as being a long one.  Each buffer head contains a bulk-in and
 * a bulk-out request pointer (since the buffer can be used for both
 * output and input -- directions always are given from the host's
 * point of view) as well as a pointer to the buffer and various state
 * variables.
 *
 * Use of the pipeline follows a simple protocol.  There is a variable
 * (fsg->next_buffhd_to_fill) that points to the next buffer head to use.
 * At any time that buffer head may still be in use from an earlier
 * request, so each buffer head has a state variable indicating whether
 * it is EMPTY, FULL, or BUSY.  Typical use involves waiting for the
 * buffer head to be EMPTY, filling the buffer either by file I/O or by
 * USB I/O (during which the buffer head is BUSY), and marking the buffer
 * head FULL when the I/O is complete.  Then the buffer will be emptied
 * (again possibly by USB I/O, during which it is marked BUSY) and
 * finally marked EMPTY again (possibly by a completion routine).
 *
 * A module parameter tells the driver to avoid stalling the bulk
 * endpoints wherever the transport specification allows.  This is
 * necessary for some UDCs like the SuperH, which cannot reliably clear a
 * halt on a bulk endpoint.  However, under certain circumstances the
 * Bulk-only specification requires a stall.  In such cases the driver
 * will halt the endpoint and set a flag indicating that it should clear
 * the halt in software during the next device reset.  Hopefully this
 * will permit everything to work correctly.  Furthermore, although the
 * specification allows the bulk-out endpoint to halt when the host sends
 * too much data, implementing this would cause an unavoidable race.
 * The driver will always use the "no-stall" approach for OUT transfers.
 *
 * One subtle point concerns sending status-stage responses for ep0
 * requests.  Some of these requests, such as device reset, can involve
 * interrupting an ongoing file I/O operation, which might take an
 * arbitrarily long time.  During that delay the host might give up on
 * the original ep0 request and issue a new one.  When that happens the
 * driver should not notify the host about completion of the original
 * request, as the host will no longer be waiting for it.  So the driver
 * assigns to each ep0 request a unique tag, and it keeps track of the
 * tag value of the request associated with a long-running exception
 * (device-reset, interface-change, or configuration-change).  When the
 * exception handler is finished, the status-stage response is submitted
 * only if the current ep0 request tag is equal to the exception request
 * tag.  Thus only the most recently received ep0 request will get a
 * status-stage response.
 *
 * Warning: This driver source file is too long.  It ought to be split up
 * into a header file plus about 3 separate .c files, to handle the details
 * of the Gadget, USB Mass Storage, and SCSI protocols.
 */


/* #define VERBOSE_DEBUG */
/* #define DUMP_MSGS */

#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/limits.h>
#include <linux/rwsem.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/freezer.h>
#include <linux/utsname.h>
/* daveti: add support for:
 * kernel timer, random, crypto, netlink, list,
 * sock, endian and krsa
 */
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/list.h>
#include <linux/netlink.h>
#include <linux/time.h>
#include <net/sock.h>
//#include <asm/byteorder.h>
#include <linux/krsa.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/composite.h>

#include "gadget_chips.h"

/* daveti: for ProvUSB */
#include "provusb.h"


/*------------------------------------------------------------------------*/

#define FSG_DRIVER_DESC		"Mass Storage Function"
#define FSG_DRIVER_VERSION	"2009/09/11"

static const char fsg_string_interface[] = "Mass Storage";

#define FSG_NO_DEVICE_STRINGS    1
#define FSG_NO_OTG               1
#define FSG_NO_INTR_EP           1

#include "storage_common.c"

/*-------------------------------------------------------------------------*/

/* Added support for the trusted devices
 * 1. ADSC commands support
 * 2. TPM attestation support
 * 3. Netlink socket support
 * 4. Timer support
 * 5. Provenancd daemon support
 * 6. TIM (TPM Information Management) support
 * Last update by daveti
 * Jul 6, 2014
 *
 * 7. Replace the spin_lock with spin_lock_irqsave to see
 * if the scheduling while atomic bug could be fixed...
 * 8. Reverse 7 for now.
 * Sep 12, 2015
 *
 * root@davejingtian.org
 * http://davejingtian.org
 */

/* Tiny FSM for the TPM attesation */
#define FSG_TRUSTED_DEV_TPM_ATT_NONE		0
#define FSG_TRUSTED_DEV_TPM_ATT_INIT		1
#define FSG_TRUSTED_DEV_TPM_ATT_AIK		2
#define FSG_TRUSTED_DEV_TPM_ATT_QUOTE		3	
#define FSG_TRUSTED_DEV_TPM_ATT_SUCCESS		4	
#define FSG_TRUSTED_DEV_TPM_ATT_FAILURE		5
#define FSG_TRUSTED_DEV_TPM_ATT_TIMEOUT		6
/* TIM DB size in the kernel */
#define FSG_TRUSTED_DEV_TIM_ENTRY_NUM_MAX	10 // max number of TIM entries
/* TPM attestation related defs */
#define FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN	284 // bytes: AIK public key length
#define FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_A_DEF	0xff // PCR 0~7
#define FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_B_DEF  0x00 // PCR 8~15
#define FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_C_DEF  0x00 // PCR 16~23
#define FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_LEN	3  // bytes (24 bits)
#define FSG_TRUSTED_DEV_TPM_ATT_NONCE_LEN	20 // bytes
#define FSG_TRUSTED_DEV_TPM_ATT_PCR_DIGEST_LEN	20 // bytes
#define FSG_TRUSTED_DEV_TPM_ATT_QUOTE_SIG_LEN	256// bytes
#define FSG_TRUSTED_DEV_TPM_ATT_QUOTE_PRE_LEN	8 // bytes
#define FSG_TRUSTED_DEV_TPM_ATT_QUOTE_TIMER	10 // seconds
/* ADSC debug request used to find the MxPS */
#define FSG_TRUSTED_DEV_ADSC_DEBUG_REQ1_LEN	30 // bytes
#define FSG_TRUSTED_DEV_ADSC_DEBUG_REQ2_LEN	32 // bytes
#define FSG_TRUSTED_DEV_ADSC_DEBUG_REQ3_LEN	64 // bytes
#define FSG_TRUSTED_DEV_ADSC_DEBUG_REQ4_LEN	128// bytes
#define FSG_TRUSTED_DEV_ADSC_DEBUG_REQ5_LEN	256// bytes
#define FSG_TRUSTED_DEV_ADSC_DEBUG_REQ6_LEN	512// bytes
/* The max length of msg EP0 supports */
#define FSG_TRUSTED_DEV_ADSC_MAX_PKT_SIZE	256// bytes
/* TPM attestation msg length */
#define FSG_TRUSTED_DEV_TPM_ATT_QUOTE_VALID_LEN	(FSG_TRUSTED_DEV_TPM_ATT_QUOTE_PRE_LEN + \
						FSG_TRUSTED_DEV_TPM_ATT_PCR_DIGEST_LEN + \
						FSG_TRUSTED_DEV_TPM_ATT_NONCE_LEN) // 48 bytes
#define FSG_TRUSTED_DEV_TPM_ATT_INIT_LEN	(FSG_TRUSTED_DEV_TPM_ATT_NONCE_LEN + \
						FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_LEN) // 23 bytes
#define FSG_TRUSTED_DEV_TPM_ATT_QUOTE_LEN	(FSG_TRUSTED_DEV_TPM_ATT_QUOTE_VALID_LEN + \
						FSG_TRUSTED_DEV_TPM_ATT_QUOTE_SIG_LEN) // 304 bytes
#define FSG_TRUSTED_DEV_TPM_ATT_LAST_AIK_LEN	(FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN % \
						FSG_TRUSTED_DEV_ADSC_MAX_PKT_SIZE) // 284%64=28 bytes
#define FSG_TRUSTED_DEV_TPM_ATT_LAST_QUOTE_LEN	(FSG_TRUSTED_DEV_TPM_ATT_QUOTE_LEN % \
						FSG_TRUSTED_DEV_ADSC_MAX_PKT_SIZE) // 304%64=48 bytes
/* Defs for the netlink socket */
#define FSG_TRUSTED_DEV_NETLINK_SOCK			31
#define FSG_TRUSTED_DEV_NETLINK_ADD_TIM_ENTRY		0 // Add a TIM entry into the TIM DB
#define FSG_TRUSTED_DEV_NETLINK_DEL_TIM_ENTRY		1 // Delete a TIM entry from the TIM DB
#define FSG_TRUSTED_DEV_NETLINK_GET_TIM_ENTRY_NUM	2 // Return the number of entries wthin the TIM DB
#define FSG_TRUSTED_DEV_NETLINK_GET_TIM_ENTRY		3 // Retrieve the TIM entry given the index
/* Filter for kernel logging */
#define FSG_TRUSTED_DEV_KERNEL_LOGGING			"prov-usb-kl"

/* Flag for UT */
static int fsg_trusted_dev_ut = 1; /* fixed nonce from the device */
/* Flag for debug */
static int fsg_trusted_dev_debug = 0;
/* Flag for ADSC debug cooperating with the host side */
static int fsg_trusted_dev_adsc_debug = 0;
/* Global counter for the MxPs (256-byte) ADSC */
static int fsg_trusted_dev_adsc_debug_mxps_num = 0;
/* Internal debugging */
static int fsg_trusted_dev_force_success = 0;
static int fsg_trusted_dev_byte_order_debug = 0;
/* Hardcode the first PCR digest for arpsec03 */
static const char fsg_trusted_tpm_pcr_digest_arpsec03[FSG_TRUSTED_DEV_TPM_ATT_PCR_DIGEST_LEN] =
"\xb0\xbc\x37\x52\x01\x21\x46\x77\xe8\x4b\xb1\xee\x7c\x4f\xb1\xb3\xbd\x01\x6e\x90";
/* Global skb related for provusb_nl_send */
static int fsg_trusted_dev_skb_debug = 0;
static int fsg_trusted_dev_global_skb = 0;	/* Enable global skb (with default allocation in init) */
static int fsg_trusted_dev_global_skb_call = 0;	/* Allocate the skb in the provusb_nl_send */
static struct sk_buff *provusb_nl_send_global_skb;
/* Dirty hacks  */
static int fsg_trusted_dev_kernel_log = 1;

/* TIM (TPM Information Management) structure */
struct fsg_trusted_tim {
	/* TIM entry index */
	unsigned int index;
	/* TPM PCR mask - [ABC]
	daveti: currently the global static config would be used
	to avoid sending multiple nonce and pcr mask to the host
	if the device has more than 1 TIM in the DB...
	u8 tim_pcr_mask[FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_LEN];
	*/
	/* NOTE:
 	daveti: may have to reuse the key object if the asymmetric
	crypto framwork is ported into this kernel 3.5...Currently
	the RSA verification part is still under discussion!
	Jul 16, 2014
	*/
	/* TPM PCR digest golden value */
	u8 tim_pcr_digest[FSG_TRUSTED_DEV_TPM_ATT_PCR_DIGEST_LEN];
	/* TPM AIK public key */
	u8 tim_aik_pub[FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN];
	/* Above should be updated by the provd
 	 * in the user space.
 	 */
	/* Below are updated by the host machine
	 * and used by the RSA verification
	 * So we do NOT need spin lock here!
	 * Jan 13, 2015
	 * daveti
	 */
	u8 sig[KRSA_TPM_QUOTE_SIG_LEN];		// signature from the host machine
	u8 valid[KRSA_TPM_QUOTE_VALID_LEN];	// validation data based on tim_pcr_digest
	u8 rsa_n[KRSA_TPM_AIK_PUB_KEY_N_LEN];	// parsed from tim_aik_pub
	u8 rsa_e[KRSA_TPM_AIK_PUB_KEY_E_LEN];	// parsed from tim_aik_pub
	/* For simple policy control
	 * using security level from high to low (0->2)
	 * Aug 26, 2015
	 * daveti
	 */
	u8 sec_level;
	/* List */
	struct fsg_trusted_tim *next;
};

/* A global struct for the trusted device */
struct fsg_trusted_dev {
	/* Spinlock for the kernel timer thread */
	spinlock_t lock;
	/* TPM attestation state */
	u8 tpm_att_state;
	/* Global TPM PCR mask */
	u8 tpm_att_pcr_mask[FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_LEN];
	/* 20-byte nonce currently in use if availabe */
	u8 tpm_att_nonce[FSG_TRUSTED_DEV_TPM_ATT_NONCE_LEN];
	/* 284-byte AIK public key from the host machine */
	u8 tpm_att_aik[FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN];
	/* 48+256 quote from the host machine */
	u8 tpm_att_quote[FSG_TRUSTED_DEV_TPM_ATT_QUOTE_LEN];
	/* Kernel timer for TPM quote */
	struct timer_list tpm_att_quote_timer;
	/* TIM DB */
	struct fsg_trusted_tim tim_db;
	/* Current TIM in use */
	struct fsg_trusted_tim *tim;
	/* Current TIM DB size */
	u8 tim_db_entry_num;
	/* Current avaiable TIM index */
	u8 tim_entry_index;
	/* Netlink socket to communicate with provenance daemon */
	struct sock *provd_nl_sock;
	/* provd pid */
	pid_t provd_pid;
	/* Config - AIK pub key file */
	char *key_file;
	/* Config - force verification failure */
	u8 force_fail;
	/* for ProvUSB perf */
	u8 provusb_disable;
	/* RSA context */
	struct krsa_ctx ctx;
	/* Block-level policy control */
	struct list_head blk_pol_ctrl;
};

/* The main struct for the trusted device control */
struct fsg_trusted_dev fsg_trusted_dev_ctrl;

/* Helper function for endian handling for key file */
static inline void fsg_trusted_dev_array_be16_to_cpu(u16 *arr, int num)
{
	int i;
	u8 aik[FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN];
	u16 *aikp;

	if (fsg_trusted_dev_byte_order_debug) {
		/* Save the key into stack buf */
		memcpy(aik, (u8 *)arr, FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN);
		printk(KERN_INFO "daveti: original key dump:\n");
		print_hex_dump(KERN_INFO, "", DUMP_PREFIX_NONE, 16, 1, aik, 
				FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN, 0);
		/* le16_to_cpu */
		aikp = (u16 *)aik;
		for (i = 0; i < num; i++, aikp++)
			*aikp = le16_to_cpu(*aikp);
		printk(KERN_INFO "daveti: le16_to_cpu:\n");
                print_hex_dump(KERN_INFO, "", DUMP_PREFIX_NONE, 16, 1, aik,
                                FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN, 0);

		/* cpu_to_le16 */
                aikp = (u16 *)aik;
		memcpy(aik, (u8 *)arr, FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN);
                for (i = 0; i < num; i++, aikp++)
                        *aikp = cpu_to_le16(*aikp);
		printk(KERN_INFO "daveti: cpu_to_le16:\n");
                print_hex_dump(KERN_INFO, "", DUMP_PREFIX_NONE, 16, 1, aik,
                                FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN, 0);

		/* be16_to_cpu */
                aikp = (u16 *)aik;
		memcpy(aik, (u8 *)arr, FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN);
                for (i = 0; i < num; i++, aikp++)
                        *aikp = be16_to_cpu(*aikp);
		printk(KERN_INFO "daveti: be16_to_cpu:\n");
                print_hex_dump(KERN_INFO, "", DUMP_PREFIX_NONE, 16, 1, aik,
                                FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN, 0);

		/* cpu_to_be16 */
                aikp = (u16 *)aik;
		memcpy(aik, (u8 *)arr, FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN);
                for (i = 0; i < num; i++, aikp++)
                        *aikp = cpu_to_be16(*aikp);
		printk(KERN_INFO "daveti: cpu_to_be16:\n");
                print_hex_dump(KERN_INFO, "", DUMP_PREFIX_NONE, 16, 1, aik,
                                FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN, 0);
	}

	/* Pick up the right API we should use here */
	for (i = 0; i < num; i++, arr++)
		*arr = be16_to_cpu(*arr);
}

/* Helper function for the last TPM quote segment */
static inline int fsg_trusted_dev_is_last_tpm_quote_segment(int len)
{
	switch (len)
	{
		case FSG_TRUSTED_DEV_TPM_ATT_LAST_AIK_LEN:
		case FSG_TRUSTED_DEV_TPM_ATT_LAST_QUOTE_LEN:
			return 1;

		default:
			return 0;
	}
}

/* Helper function for TPM quote request */
static inline int fsg_trusted_dev_is_tpm_quote_req(int len)
{
	switch (len)
	{
		//case FSG_TRUSTED_DEV_ADSC_MAX_PKT_SIZE:
		case FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN:
		case FSG_TRUSTED_DEV_TPM_ATT_QUOTE_LEN:
		case FSG_TRUSTED_DEV_TPM_ATT_LAST_AIK_LEN:
		case FSG_TRUSTED_DEV_TPM_ATT_LAST_QUOTE_LEN:
			return 1;

		default:
			return 0;
	}
}

/* Helper function for ADSC debug request */
static inline int fsg_trusted_dev_is_adsc_debug_req(int len)
{
	switch (len)
	{
		case FSG_TRUSTED_DEV_ADSC_DEBUG_REQ1_LEN:
		case FSG_TRUSTED_DEV_ADSC_DEBUG_REQ2_LEN:
		case FSG_TRUSTED_DEV_ADSC_DEBUG_REQ4_LEN:
		case FSG_TRUSTED_DEV_ADSC_DEBUG_REQ6_LEN:
			return 1;

		case FSG_TRUSTED_DEV_ADSC_DEBUG_REQ3_LEN:
                        if (len == FSG_TRUSTED_DEV_ADSC_MAX_PKT_SIZE) {
                                /* Only the first is debug msg */
                                if (fsg_trusted_dev_adsc_debug_mxps_num == 0)
                                        return 1;
                                else
                                        return 0;
                        }
                        return 1;

		case FSG_TRUSTED_DEV_ADSC_DEBUG_REQ5_LEN:
			if (len == FSG_TRUSTED_DEV_ADSC_MAX_PKT_SIZE) {
				/* Only the first 2 is debug msg */
				if (fsg_trusted_dev_adsc_debug_mxps_num < 2)
					return 1;
				else
					return 0;
			}
			return 1;

		default:
			return 0;
	}
}

/* TPM quote timer handler */
void fsg_trusted_dev_tpm_quote_timer_handler(unsigned long arg)
{
	unsigned long flags;

	if (fsg_trusted_dev_debug)
		printk(KERN_INFO "daveti: into [%s]\n", __FUNCTION__);

	struct fsg_trusted_dev *ctrl = (struct fsg_trusted_dev *)arg;
	
	/* Check the state without lock */
	switch (ctrl->tpm_att_state)
	{
		case FSG_TRUSTED_DEV_TPM_ATT_NONE:
			/* The host does not support TPM attestation */
			break;

		case FSG_TRUSTED_DEV_TPM_ATT_INIT:
			/* Timeout */
			printk(KERN_INFO "daveti: TPM ATT timer timeouts - no quote from the host\n");
			spin_lock(&ctrl->lock);
			//spin_lock_irqsave(&ctrl->lock, flags);
			ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_TIMEOUT;
			spin_unlock(&ctrl->lock);
			//spin_unlock_irqrestore(&ctrl->lock, flags);
			break;

		case FSG_TRUSTED_DEV_TPM_ATT_AIK:
		case FSG_TRUSTED_DEV_TPM_ATT_QUOTE:
		case FSG_TRUSTED_DEV_TPM_ATT_SUCCESS:
		case FSG_TRUSTED_DEV_TPM_ATT_FAILURE:
		case FSG_TRUSTED_DEV_TPM_ATT_TIMEOUT:
			/* Do not care these states */
			break;

		default:
			printk(KERN_ERR "daveti: unknown TPM ATT state [%d]\n",
				ctrl->tpm_att_state);
			break;
	}
}

/* Find the corresponding TIM given the AIK */
static struct fsg_trusted_tim *fsg_trusted_dev_find_matching_aik(struct fsg_trusted_dev *ctrl)
{
	if (fsg_trusted_dev_debug)
		pr_info("daveti: into [%s]\n", __func__);

	struct fsg_trusted_tim *tim = &(ctrl->tim_db);

	/* Go thru the TIM DB */
	do {
		/* At least, we have got one tim */
		if (memcmp(ctrl->tpm_att_aik, tim->tim_aik_pub, FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN) == 0)
			return tim;
		else
			tim = tim->next;
	} while (tim != NULL);
	
	return NULL;
}

/* Construct the validation data from the PCR digest + nonce */
static int fsg_trusted_dev_build_valid_data(struct fsg_trusted_dev *ctrl)
{
	/* The format of validation data is
 	 * TPM Quote perfix header	- 8 bytes
 	 * TPM PCR digest		- 20 bytes
 	 * TPM attestation nonce	- 20 bytes
 	 * 		in total	- 48 bytes
 	 * Jan 15, 2015
 	 * daveti
 	 */
	struct fsg_trusted_tim *tim = ctrl->tim;

	if (fsg_trusted_dev_debug)
		pr_info("daveti: into [%s]\n", __func__);

	if (!tim) {
		pr_err("daveti: null tim\n");
		return -1;
	}

	char valid_prefix[FSG_TRUSTED_DEV_TPM_ATT_QUOTE_PRE_LEN] = "\x01\x01\x00\x00\x51\x55\x4f\x54";

	/* Build the validation data */
	memcpy(tim->valid, valid_prefix, FSG_TRUSTED_DEV_TPM_ATT_QUOTE_PRE_LEN);
	memcpy(tim->valid+FSG_TRUSTED_DEV_TPM_ATT_QUOTE_PRE_LEN,
		tim->tim_pcr_digest, FSG_TRUSTED_DEV_TPM_ATT_PCR_DIGEST_LEN);
	memcpy(tim->valid+FSG_TRUSTED_DEV_TPM_ATT_QUOTE_PRE_LEN+FSG_TRUSTED_DEV_TPM_ATT_PCR_DIGEST_LEN,
		ctrl->tpm_att_nonce, FSG_TRUSTED_DEV_TPM_ATT_NONCE_LEN);

	return 0;
}

/* Parse the loaded AIK pub key into RSA friendly format */
static int fsg_trusted_dev_parse_aik_pub_key(struct fsg_trusted_tim *tim)
{
	if (fsg_trusted_dev_debug)
		pr_info("daveti: into [%s]\n", __func__);

	/* Note: I am doing hackish and dirty things here
	 * Rather than finding a general way to parse the AIK pub key file
	 * generated from tqt, I manually parse it...(God forgive me)
	 * Jan 13, 2015
	 * daveti
	 */

	if (!tim) {
		pr_err("daveti: null tim\n");
		return -1;
	}

	memcpy(tim->rsa_e, tim->tim_aik_pub+3, KRSA_TPM_AIK_PUB_KEY_E_LEN);
	memcpy(tim->rsa_n, tim->tim_aik_pub+28, KRSA_TPM_AIK_PUB_KEY_N_LEN);

	return 0;
}

/* Load the key file for trusted dev */
static int fsg_trusted_dev_load_key_file(struct fsg_trusted_dev *ctrl)
{
        struct file                     *filp = NULL;
        int                             rc = -EINVAL;
        struct inode                    *inode = NULL;
        loff_t                          pos = 0;
	static mm_segment_t		fs;
	char 				*filename = ctrl->key_file;

	if (fsg_trusted_dev_debug)
		printk(KERN_INFO "daveti: into [%s]\n", __FUNCTION__);

	/* Defensive checking */
	if (filename == NULL) {
		printk(KERN_ERR "daveti: Key file does not exist\n");
		return rc;
	}

	/* Set FS */
	fs = get_fs();
	set_fs(get_ds());

	/* Open the file */
	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_ERR "daveti: unable to open key file: [%s]\n", filename);
		set_fs(fs);
		rc= -ENOENT;
		goto restore_fs;
		// return PTR_ERR(filp);
	}

	/* Make sure we can read the file */
	if (!filp->f_op || !(filp->f_op->read || filp->f_op->aio_read)) {
		printk(KERN_ERR "daveti: file not readable: [%s]\n", filename);
		goto out;
	}

	/* Check the inode */
	if (filp->f_path.dentry)
		inode = filp->f_path.dentry->d_inode;
	if (!inode || (!S_ISREG(inode->i_mode))) {
		printk(KERN_ERR "daveti: invalid file type: [%s]\n", filename);
		goto out;
	}

	/* Read the file */
	rc = filp->f_op->read(filp, ctrl->tim_db.tim_aik_pub,
				FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN, &pos);
	if (rc < 0) {
		printk(KERN_ERR "daveti: bad read with rtn [%d]\n", rc);
		rc = -EIO;
		goto out;
	}
	else if (rc != FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN) {
		printk(KERN_ERR "daveti: requested [%d] bytes but only read [%d] bytes\n",
			FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN, rc);
		rc = -EIO;
		goto out;
	}

	/* Fix the endian issue */
	/* NOTE: even though the byte order of the AIK is changed after loading
 	 * into the memory compared to the hexdump of the key file, we do NOT
 	 * do any byte-order tuning here because the AIK sent from USB ADSC cmd
 	 * shares the same byte order in memory.
 	 * Aug 7, 2014
 	 * daveti
	fsg_trusted_dev_array_be16_to_cpu((u16 *)ctrl->tim_db.tim_aik_pub,
					FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN/2);
	 */

	rc = 0;
out:
	filp_close(filp, current->files);
	return rc;

restore_fs:
	set_fs(fs);
	return rc;
}

/* Load the config for trsuted dev - def after fsg_config */

/* Update the trusted dev state based on the TPM quote/AIK verification */
/* NOTE: This function is moved into EP0 handler, where the ctrl block
 * lock has been accquited by the handler. We should NOT lock here!
 * Jan 23, 2015
 * daveti
 */
static void fsg_trusted_dev_update_state(struct fsg_trusted_dev *ctrl)
{
	if (fsg_trusted_dev_debug)
		pr_info("daveti: into [%s]\n", __func__);

	int clear_tim;
	struct fsg_trusted_tim *tim;

	/* ProvUSB: as long as the TPM attestation is not succcessful,
 	 * we need to clear the current tim in the ctrl block.
 	 * On the other hand, we should NOT clear it if the TPM
 	 * attestation is good.
 	 * Fix the kernel panic~
 	 * Sep 4, 2015
 	 * daveti
 	 */
	clear_tim = 1;

	switch (ctrl->tpm_att_state) {

	case FSG_TRUSTED_DEV_TPM_ATT_AIK:
		/* Lock at first to guarantee a good read */
		//spin_lock_irq(&ctrl->lock);

		/* AIK matching */
		tim = fsg_trusted_dev_find_matching_aik(ctrl);
		if (tim) {
			pr_info("daveti: found matching AIK\n");
			ctrl->tim = tim;
			if (fsg_trusted_dev_debug)
				pr_info("provusb: current tim is [%p]\n", ctrl->tim);
		} else {
			pr_err("daveti: no matching AIK is found - TPM Attestation failed\n");
			ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_FAILURE;
			ctrl->tim = NULL;
		}

		/* Free the lock */
		//spin_unlock_irq(&ctrl->lock);
		break;

	case FSG_TRUSTED_DEV_TPM_ATT_QUOTE:
		/* Lock at first to guarantee a good read */
		//spin_lock_irq(&ctrl->lock);

		/* Check if we have the AIK already */
		if (!ctrl->tim) {
			pr_err("daveti: no AIK from the host - TPM Attestation failed\n");
			ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_FAILURE;
			goto DONE;
		}

		/* Build the validation from the fly */
		if (fsg_trusted_dev_build_valid_data(ctrl)) {
			pr_err("daveti: validation construction failed - TPM Attestation failed\n");
			ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_FAILURE;
			goto DONE;
		}

		/* Validation data verification - PCR digest + nonce */
		if (memcmp(ctrl->tpm_att_quote, ctrl->tim->valid, KRSA_TPM_QUOTE_VALID_LEN)) {
			pr_err("daveti: validation date verification failed - TPM Attestation failed\n");
			/* Dump the valid validation data */
			print_hex_dump(KERN_INFO, "dev: ", DUMP_PREFIX_OFFSET, 16, 1,
				(u8 *)ctrl->tim->valid, KRSA_TPM_QUOTE_VALID_LEN, 0);
			/* Dump the valiation data from the host */
			print_hex_dump(KERN_INFO, "hst: ", DUMP_PREFIX_OFFSET, 16, 1,
				(u8 *)ctrl->tpm_att_quote, KRSA_TPM_QUOTE_VALID_LEN, 0);
			ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_FAILURE;
			goto DONE;
		}

		/* Check for failure forcing */
		/* Currently force_fail only works for RSA verification,
 		 * the same as force_success...I know this is not reasonbale
 		 * but I do not have a plan to change...
 		 * excepting writing down this comment...
 		 * Jan 16, 2015
 		 * daveti
 		 */
		if (ctrl->force_fail) {
			pr_info("daveti: RSA verification - force failure\n");
			ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_FAILURE;
			goto DONE;
		}

		/* Extract the signature from the TPM quote */
		memcpy(ctrl->tim->sig,
			ctrl->tpm_att_quote + KRSA_TPM_QUOTE_VALID_LEN,
			KRSA_TPM_QUOTE_SIG_LEN);
		if (fsg_trusted_dev_debug)
			print_hex_dump(KERN_INFO, "sig: ", DUMP_PREFIX_OFFSET, 16, 1,
			(u8 *)ctrl->tim->sig, KRSA_TPM_QUOTE_SIG_LEN, 0);

		/* Init the krsa ctx */
		if (krsa_init(&(ctrl->ctx),
				ctrl->tim->sig, KRSA_TPM_QUOTE_SIG_LEN,
				ctrl->tim->valid, KRSA_TPM_QUOTE_VALID_LEN,
				ctrl->tim->rsa_n, KRSA_TPM_AIK_PUB_KEY_N_LEN,
				ctrl->tim->rsa_e, KRSA_TPM_AIK_PUB_KEY_E_LEN)) {
			pr_err("daveti: krsa_init failed - TPM Attestation failed\n");
			ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_FAILURE;
			goto DONE;
		}

		/* RSA verification */
		if (krsa_verify(&(ctrl->ctx))) {
			if (fsg_trusted_dev_force_success) {
				pr_err("daveti: RSA verification failed - force success\n");
				ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_SUCCESS;
				clear_tim = 0;
			} else {
				pr_err("daveti: RSA verification failed - TPM Attestation failed\n");
				ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_FAILURE;
			}
		} else {
			pr_info("daveti: RSA verification success - TPM Attestation success\n");
			ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_SUCCESS;
			clear_tim = 0;
		}

		/* Clear the ctx for next use */
		krsa_exit(&(ctrl->ctx));
DONE:
		/* Reset the current TIM for next use */
		if (clear_tim)
			ctrl->tim = NULL;
		/* Free the lock */
		//spin_unlock_irq(&ctrl->lock);
		break;

	default:
		/*
		pr_info("daveti: no update for the current state [%d]\n",
			ctrl->tpm_att_state);
		*/
		break;
	}
}

/* Block policy control routines */
static struct provusb_block_entry *provusb_get_blk_ctrl_blk(unsigned int lba)
{
	struct provusb_block_entry *ptr;
	unsigned long flags;

	spin_lock(&fsg_trusted_dev_ctrl.lock);
	//spin_lock_irqsave(&fsg_trusted_dev_ctrl.lock, flags);
	list_for_each_entry(ptr, &fsg_trusted_dev_ctrl.blk_pol_ctrl, list) {
		if (ptr->block.lba == lba) {
			/* UNLOCK before return */
			spin_unlock(&fsg_trusted_dev_ctrl.lock);
			return ptr;
		}
	}
	spin_unlock(&fsg_trusted_dev_ctrl.lock);

	return NULL;
}

static int provusb_get_blk_ctrl_blk_sec_level(struct fsg_trusted_dev *ctrl,
						unsigned int lba)
{
        struct provusb_block_entry *ptr;

        spin_lock(&ctrl->lock);
        list_for_each_entry(ptr, &ctrl->blk_pol_ctrl, list) {
                if (ptr->block.lba == lba) {
			/* UNLOCK before return */
			spin_unlock(&ctrl->lock);
                        return ptr->block.sec_level;
		}
        }
        spin_unlock(&ctrl->lock);

        return -1;
}

static int provusb_create_blk_ctrl_blk(struct fsg_trusted_dev *ctrl,
					unsigned int lba, int sec_level)
{
        struct provusb_block_entry *entry;

	/* Alloc mem */
	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		pr_err("provusb: kmalloc failed in %s\n", __func__);
		return -1;
	}

	/* Init */
	memset(entry, 0x0, sizeof(*entry));
	entry->block.lba = lba;
	if (sec_level == -1)
		entry->block.sec_level = ctrl->tim->sec_level;
	else
		entry->block.sec_level = sec_level;

	/* Add to the tail */
	spin_lock(&ctrl->lock);
	list_add_tail(&entry->list, &ctrl->blk_pol_ctrl);
	spin_unlock(&ctrl->lock);

	return 0;
}

/* Netlink related callbacks */
/*
 * NOTE: provusb_nl_send is the unique interface to provd.
 * It is used by nl_init, log, and syn_blk currently.
 * Moreover, all these calls are sequential so far.
 * Here comes the dirty hack:
 * to accelerate the netlink send and reduce the burden of slab,
 * we use a global skb instead of dynamic allocation without locking.
 * The other reason for doing this is that the skb is from gfp_atomic -
 * we do not want to bother mempool everytime and it may fail sometime.
 * If future development could break the sequential usage of provusb_nl_send,
 * dynamic allocation should be recovered and I would not recommend mutex locking.
 * Jan 15, 2016
 * daveti
 *
 * UPDATE: global skb does NOT work due the nature of the AIO of netlink -
 * we simply could not recycle the skb for the next use until the provd recv'd the skb,
 * which we would never know...
 * Jan 16, 2016
 * daveti
 */
static int provusb_nl_send(struct fsg_trusted_dev *ctrl,
				int opcode, u8 *data, int len)
{
        struct nlmsghdr *nlh;
        struct sk_buff *skb_out;
        struct provusb_nlmsg msg_req;
        int msg_size;
        int data_len;
        int rtn;

	if (fsg_trusted_dev_debug)
		pr_info("provusb: into [%s], opcode [%d], data [%p], len [%d]\n",
			__func__, opcode, data, len);

        /* Defensive checking */
        if (!ctrl->provd_pid) {
                pr_err("provusb: provd pid is unknown yet - abort\n");
                return -1;
        }

        /* Construct the request */
        msg_size = sizeof(msg_req);
        memset(&msg_req, 0, msg_size);
        msg_req.opcode = opcode;

	/* Copy the data based on opcode */
	switch (opcode) {
	case PROVUSB_NETLINK_OP_INIT:
	case PROVUSB_NETLINK_OP_REP:
		memcpy(&msg_req.report, data, len);
		break;

	case PROVUSB_NETLINK_OP_POL:
		memcpy(&msg_req.policy, data, len);
		break;

	case PROVUSB_NETLINK_OP_BLK:
		memcpy(&msg_req.block, data, len);
		break;

	default:
		pr_err("provusb: unknonwn opcode %d\n", opcode);
		return -1;
	}

        /* Send the msg from kernel to the user */
	if (fsg_trusted_dev_global_skb) {
		if (fsg_trusted_dev_global_skb_call && (!provusb_nl_send_global_skb)) {
			/* Allocate the global skb now */
			provusb_nl_send_global_skb = nlmsg_new(msg_size, GFP_KERNEL);
			if (!provusb_nl_send_global_skb) {
				pr_err("provusb: failed to allocate global skb in call\n");
				fsg_trusted_dev_global_skb_call = 0;
				fsg_trusted_dev_global_skb = 0;
				return -1;
			} else {
				pr_info("provusb: use global skb [%p] in call\n", provusb_nl_send_global_skb);
			}
		}
		/* Use the global skb */
		skb_out = provusb_nl_send_global_skb;
		if (fsg_trusted_dev_skb_debug)
			pr_info("provusb: skb_out=%p, global_skb=%p\n",
				skb_out, provusb_nl_send_global_skb);
	} else {
		/* Dynamic allocation */
		skb_out = nlmsg_new(msg_size, GFP_KERNEL);
		if (!skb_out) {
			pr_err("provusb: failed to allocate new skb\n");
			return -1;
		}
	}

        nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
        NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
        memcpy(nlmsg_data(nlh), &msg_req, msg_size);

PROVD_SEND_AGAIN:
        rtn = nlmsg_unicast(ctrl->provd_nl_sock, skb_out, ctrl->provd_pid);
        if (rtn) {
		/* Try to send it again for temp failure */
		if (rtn == -EAGAIN)
			goto PROVD_SEND_AGAIN;
		/* Stop for other errors */
                pr_err("provusb: failed to send request to provd, rtn [%d]\n", rtn);
                rtn = -1;
		goto PROVD_SEND_CLEAN;
        }

	rtn = 0;

PROVD_SEND_CLEAN:
	if (fsg_trusted_dev_debug || fsg_trusted_dev_skb_debug)
		pr_info("provusb: sent an nlmsg request to the provd\n");

	if (fsg_trusted_dev_global_skb) {
		skb_recycle(skb_out);
	} else {
		/* Free the netlink msg only after error */
		if (rtn != 0)
			nlmsg_free(skb_out);
	}

        return rtn;
}

static int provusb_nl_handle_policy(struct provusb_nlmsg *msg)
{
	int rtn;
	int old;
	struct fsg_trusted_tim *tim;

	if (fsg_trusted_dev_debug)
		pr_info("provusb: into [%s]\n", __func__);

	/* Try to find the existing TIM */
	spin_lock(&fsg_trusted_dev_ctrl.lock);
	tim = &(fsg_trusted_dev_ctrl.tim_db);
	/* Go thru the TIM DB */
	do {
		/* At least, we have got one tim */
		if (tim->index == msg->policy.id)
			break;
		else
			tim = tim->next;
	} while (tim != NULL);
	/* Make sure the TIM has been added */
	if (tim) {
		old = tim->sec_level;
		tim->sec_level = msg->policy.sec_level;
		rtn = 0;
	} else {
		rtn = -1;
	}	
	spin_unlock(&fsg_trusted_dev_ctrl.lock);

	if (rtn)
		pr_err("provusb: failed to find TIM for id %d\n", msg->policy.id);
	else
		pr_info("provusb: updated sec level from %d to %d for id %d\n",
			old, msg->policy.sec_level, msg->policy.id);

	return rtn;
}

static int provusb_nl_handle_block(struct provusb_nlmsg *msg)
{
	int rtn;
	int old;
	struct provusb_block_entry *entry;

	if (fsg_trusted_dev_debug)
		pr_info("provusb: into [%s]\n", __func__);

	/* Try to find the existing block */
	entry = provusb_get_blk_ctrl_blk(msg->block.lba);
	if (entry) {
		/* Update the existing block ctrl block */
		spin_lock(&fsg_trusted_dev_ctrl.lock);
		old = entry->block.sec_level;
		entry->block.sec_level = msg->block.sec_level;
		spin_unlock(&fsg_trusted_dev_ctrl.lock);
		if (fsg_trusted_dev_debug)
			pr_info("provusb: updated sec level from %d to %d for lba %u\n",
				old, msg->block.sec_level, msg->block.lba);
	} else {
		/* Create a new block ctrl block */
		rtn = provusb_create_blk_ctrl_blk(&fsg_trusted_dev_ctrl,
					msg->block.lba, msg->block.sec_level);
		if (rtn)
			pr_err("provusb: provusb_create_blk_ctrl_blk failed in %s", __func__);
		else {
			if (fsg_trusted_dev_debug)
				pr_info("provusb: added new blk_ctrl_blk with sec level %d for lba %u\n",
					msg->block.sec_level, msg->block.lba);
		}
	}

	return rtn;
}

static int provusb_nl_handle_init(struct nlmsghdr *nlh)
{
        int rtn;
	struct provusb_report init_msg;

	if (fsg_trusted_dev_debug)
		pr_info("provusb: into [%s]\n", __func__); 


	if (nlh != NULL) {
		fsg_trusted_dev_ctrl.provd_pid = nlh->nlmsg_pid; /*pid of sending process */
		pr_info("provusb: provd pid [%i]\n", fsg_trusted_dev_ctrl.provd_pid);

		/* Init the msg */
		PROVUSB_INIT_RESPONSE(init_msg);

		/* Send hello to the provd */
		rtn = provusb_nl_send(&fsg_trusted_dev_ctrl,
				PROVUSB_NETLINK_OP_INIT, &init_msg, sizeof(init_msg));
		if (rtn != 0)
			pr_err("provusb: provusb_nl_send() failed\n");
		else
			pr_info("provusb: sent init msg to provd\n");
	}

        return rtn;
}

static void provusb_nl_handler(struct sk_buff *skb)
{
        struct nlmsghdr *nlh;
        struct provusb_nlmsg *provusb_nlmsg_ptr;
        int opcode;
        int rtn;

	if (fsg_trusted_dev_debug)
		pr_info("provusb: entering [%s]\n", __func__);

        /* Retrive the opcode */
        nlh = (struct nlmsghdr *)skb->data;
        provusb_nlmsg_ptr = (struct provusb_nlmsg *)nlmsg_data(nlh);
        opcode = provusb_nlmsg_ptr->opcode;
        pr_info("provusb: netlink received msg opcode: %u\n", opcode);

        switch (opcode) {

	case PROVUSB_NETLINK_OP_INIT:
		rtn = provusb_nl_handle_init(nlh);
		break;

	case PROVUSB_NETLINK_OP_POL:
		rtn = provusb_nl_handle_policy(provusb_nlmsg_ptr);
		break;

	case PROVUSB_NETLINK_OP_BLK:
		rtn = provusb_nl_handle_block(provusb_nlmsg_ptr);
		break;

	default:
		rtn = 0;
		pr_err("provusb: unknown netlink opcode: %u\n", opcode);
        }

        if (rtn < 0)
                pr_err("provusb: netlink processing failure\n");
}

/* ProvUSB logger */
static void provusb_log(struct fsg_trusted_dev *ctrl,
			int act, unsigned int lba, unsigned long long offset, unsigned int amount)
{
	int rtn;
	struct provusb_report rep;

	/* Debug a kernel panic here */
	if (fsg_trusted_dev_debug) {
		if (!ctrl)
			pr_info("provusb: %s - ctrl is NULL\n", __func__);
		else {
			pr_info("provusb: %s - ctrl [%p]\n", __func__, ctrl);
			if (!ctrl->tim)
				pr_info("provusb: %s - ctrl->tim is NULL\n", __func__);
			else
				pr_info("provusb: %s - ctrl->tim [%p]\n", __func__, ctrl->tim);
		}
	}

	/* Use kernel logging */
	if (fsg_trusted_dev_kernel_log) {
		pr_info("%s:%ld,%d,%d,%u,%llu,%u\n",
			FSG_TRUSTED_DEV_KERNEL_LOGGING,
			get_seconds(),
			ctrl->tim->index, act, lba, offset, amount);
		return;
	}

	rep.id = ctrl->tim->index;
	rep.act = act;
	rep.lba = lba;
	rep.offset = offset;
	rep.amount = amount;

	/* Send report to the provd */
	rtn = provusb_nl_send(ctrl, PROVUSB_NETLINK_OP_REP, &rep, sizeof(rep));
      	if (rtn != 0)
      		pr_err("provusb: provusb_nl_send() failed in %s\n", __func__);
	else {
		if (fsg_trusted_dev_debug)
			pr_info("provusb: sent rep msg to provd\n");
	}
}

/* ProvUSB block syn'er */
static void provusb_syn_blk(struct fsg_trusted_dev *ctrl,
				unsigned int lba, int sec)
{
	int rtn;
	struct provusb_block blk;

	blk.lba = lba;
	blk.sec_level = sec;

	/* Send the block to the provd */
	rtn = provusb_nl_send(ctrl, PROVUSB_NETLINK_OP_BLK, &blk, sizeof(blk));
	if (rtn != 0)
		pr_err("provusb: provusb_nl_send() failed in %s\n", __func__);
	else {
		if (fsg_trusted_dev_debug)
			pr_info("provusb: sent blk msg to provd\n");
	}
}

/* ProvUSB I/O Policy control */
static int provusb_policy_control_stop(struct fsg_trusted_dev *ctrl,
					int act, unsigned int lba)
{
	int i;
	int sec;
	int rtn;

	/* Bypass the blocks in the blacklist */
	for (i = 0; i < PROVUSB_POLICY_BLACKLIST_NUM; i++) {
		if (lba == blk_pol_bl[i])
			return 0;
		if (blk_pol_bl[i] == 0xffffffff)
			break;
	}

	/* ProvUSB:
 	 * Currently, we are implementing a Biba data integrity model
 	 * where 'no read down and no write up'!
 	 * Aug 27, 2015
 	 * daveti
 	 */

	switch (act) {
	/* Handle READ */
	case PROVUSB_ACTION_READ:
		sec = provusb_get_blk_ctrl_blk_sec_level(ctrl, lba);
		if (fsg_trusted_dev_debug)
			pr_info("provusb: scsi read from host [%d|%d] for block [%u|%d]\n",
				ctrl->tim->index,
				ctrl->tim->sec_level,
				lba, sec);
		/* This block has not been tainted yet */
		if (sec == -1)
			return 0;
		/* No read down */
		if (ctrl->tim->sec_level < sec)
			return 1;
		break;

	/* Handle Write */
	case PROVUSB_ACTION_WRITE:
		sec = provusb_get_blk_ctrl_blk_sec_level(ctrl, lba);
		if (fsg_trusted_dev_debug)
			pr_info("provusb: scsi write from host [%d|%d] for block [%u|%d]\n",
				ctrl->tim->index,
				ctrl->tim->sec_level,
				lba, sec);
		/* Create a new block ctrl block */
		if (sec == -1) {
			rtn = provusb_create_blk_ctrl_blk(ctrl, lba, sec);
			if (rtn) {
				pr_err("provusb: provusb_create_blk_ctrl_blk failed\n");
				return 1;
			}
			/* Sync the new block with provd */
			provusb_syn_blk(ctrl, lba, ctrl->tim->sec_level);
			return 0;
		}
		/* No write up */
		if (ctrl->tim->sec_level > sec)
			return 1;
		break;

	default:
		pr_err("provusb: unknown scsi action %d\n", act);
		return 1;
	}

	return 0;
}

/* Init the fsg_trusted_dev_ctrl */
static void fsg_trusted_dev_init(struct fsg_trusted_dev *ctrl)
{
	printk(KERN_INFO "daveti: into [%s]\n", __FUNCTION__);

	/* Init all */
	memset(ctrl, 0x0, sizeof(struct fsg_trusted_dev));
	/* Init the lock */
	spin_lock_init(&(ctrl->lock));
	/* Init the TPM attestation state */
	ctrl->tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_NONE;
	/* Init the TPM PCR mask */
	(ctrl->tpm_att_pcr_mask)[0] = FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_A_DEF;
	(ctrl->tpm_att_pcr_mask)[1] = FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_B_DEF;
	(ctrl->tpm_att_pcr_mask)[2] = FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_C_DEF;
	/* Init the TPM quote timer */
	init_timer(&ctrl->tpm_att_quote_timer);
	ctrl->tpm_att_quote_timer.function =
			fsg_trusted_dev_tpm_quote_timer_handler;
	ctrl->tpm_att_quote_timer.data = (unsigned long)(&fsg_trusted_dev_ctrl);
	/* Init the TIM DB related */
	ctrl->tim_db_entry_num = 1;
	ctrl->tim_entry_index = 1;
	/* Set the PCR digest for the first TIM
	 * Future TIMs should be updated via netlink socket
	 * from provd in the user space
	 * Jan 15, 2015
	 * daveti
	 */
	memcpy(ctrl->tim_db.tim_pcr_digest,
		fsg_trusted_tpm_pcr_digest_arpsec03,
		FSG_TRUSTED_DEV_TPM_ATT_PCR_DIGEST_LEN);
	/* Support for ProvUSB
	 * Aug 24, 2015
	 * daveti
	 */
        if (!ctrl->provd_nl_sock) {
                ctrl->provd_nl_sock = netlink_kernel_create(&init_net,
						FSG_TRUSTED_DEV_NETLINK_SOCK, 0,
						provusb_nl_handler, NULL,
						THIS_MODULE);
                if(!ctrl->provd_nl_sock)
                        pr_err("provusb: netlink socket creation failure - abort\n");
		else
			pr_info("provusb: netlink socket created\n");
        }
	/* Support for simple policy control
	 * Aug 26, 2015
	 * daveti
	 */
	INIT_LIST_HEAD(&ctrl->blk_pol_ctrl);
	/*
	 * Support for global skb
	 * Jan 15, 2016
	 * daveti
	 */
	if (fsg_trusted_dev_global_skb && (!fsg_trusted_dev_global_skb_call)) {
		provusb_nl_send_global_skb = nlmsg_new(sizeof(struct provusb_nlmsg), GFP_KERNEL);
		if (!provusb_nl_send_global_skb) {
			pr_err("provusb: failed to allocate global skb\n");
			/* Try dynamic allocation again */
			fsg_trusted_dev_global_skb = 0;
		} else {
			pr_info("provusb: using global skb for netlink send\n");
		}
	}
}

/* Free the fsg_trusted_dev_ctrl resource */
static void fsg_trusted_dev_free(struct fsg_trusted_dev *ctrl)
{
	struct provusb_block_entry *ptr, *next;

	printk(KERN_INFO "daveti: into [%s]\n", __FUNCTION__);

	/* Delete the TPM quote timer */
	del_timer_sync(&ctrl->tpm_att_quote_timer);

	/* Close the netlink socket */
	if (ctrl->provd_nl_sock)
		netlink_kernel_release(ctrl->provd_nl_sock);

	/* Free the block policy control */
	list_for_each_entry_safe(ptr, next, &ctrl->blk_pol_ctrl, list) {
		list_del(&ptr->list);
		kfree(ptr);
	}

	/* Free the global skb */
	if (fsg_trusted_dev_global_skb && provusb_nl_send_global_skb)
		nlmsg_free(provusb_nl_send_global_skb);
}

/*-------------------------------------------------------------------------*/

struct fsg_dev;
struct fsg_common;

/* FSF callback functions */
struct fsg_operations {
	/*
	 * Callback function to call when thread exits.  If no
	 * callback is set or it returns value lower then zero MSF
	 * will force eject all LUNs it operates on (including those
	 * marked as non-removable or with prevent_medium_removal flag
	 * set).
	 */
	int (*thread_exits)(struct fsg_common *common);

	/*
	 * Called prior to ejection.  Negative return means error,
	 * zero means to continue with ejection, positive means not to
	 * eject.
	 */
	int (*pre_eject)(struct fsg_common *common,
			 struct fsg_lun *lun, int num);
	/*
	 * Called after ejection.  Negative return means error, zero
	 * or positive is just a success.
	 */
	int (*post_eject)(struct fsg_common *common,
			  struct fsg_lun *lun, int num);
};

/* Data shared by all the FSG instances. */
struct fsg_common {
	struct usb_gadget	*gadget;
	struct usb_composite_dev *cdev;
	struct fsg_dev		*fsg, *new_fsg;
	wait_queue_head_t	fsg_wait;

	/* filesem protects: backing files in use */
	struct rw_semaphore	filesem;

	/* lock protects: state, all the req_busy's */
	spinlock_t		lock;

	struct usb_ep		*ep0;		/* Copy of gadget->ep0 */
	struct usb_request	*ep0req;	/* Copy of cdev->req */
	unsigned int		ep0_req_tag;

	struct fsg_buffhd	*next_buffhd_to_fill;
	struct fsg_buffhd	*next_buffhd_to_drain;
	struct fsg_buffhd	*buffhds;

	int			cmnd_size;
	u8			cmnd[MAX_COMMAND_SIZE];

	unsigned int		nluns;
	unsigned int		lun;
	struct fsg_lun		*luns;
	struct fsg_lun		*curlun;

	unsigned int		bulk_out_maxpacket;
	enum fsg_state		state;		/* For exception handling */
	unsigned int		exception_req_tag;

	enum data_direction	data_dir;
	u32			data_size;
	u32			data_size_from_cmnd;
	u32			tag;
	u32			residue;
	u32			usb_amount_left;

	unsigned int		can_stall:1;
	unsigned int		free_storage_on_release:1;
	unsigned int		phase_error:1;
	unsigned int		short_packet_received:1;
	unsigned int		bad_lun_okay:1;
	unsigned int		running:1;

	int			thread_wakeup_needed;
	struct completion	thread_notifier;
	struct task_struct	*thread_task;

	/* Callback functions. */
	const struct fsg_operations	*ops;
	/* Gadget's private data. */
	void			*private_data;

	/* ProvUSB: make sure the file-storage thread
	 * is able to 'see' the fsg_trusted_dev_ctrl.
	 * Sep 3, 2015
	 * daveti
	 */
	struct fsg_trusted_dev	*provusb_ctrl;

	/*
	 * Vendor (8 chars), product (16 chars), release (4
	 * hexadecimal digits) and NUL byte
	 */
	char inquiry_string[8 + 16 + 4 + 1];

	struct kref		ref;
};

struct fsg_config {
	unsigned nluns;
	struct fsg_lun_config {
		const char *filename;
		char ro;
		char removable;
		char cdrom;
		char nofua;
	} luns[FSG_MAX_LUNS];

	const char		*lun_name_format;
	const char		*thread_name;

	/* Callback functions. */
	const struct fsg_operations	*ops;
	/* Gadget's private data. */
	void			*private_data;

	const char *vendor_name;		/*  8 characters or less */
	const char *product_name;		/* 16 characters or less */
	u16 release;

	char			can_stall;

/* daveti: 4 trsuted dev */
	const char *key_file;	/* AIK pub key file */
	u8	force_fail;	/* Force verification failure */
	u8	provusb_disable;/* For ProvUSB perf */
};

struct fsg_dev {
	struct usb_function	function;
	struct usb_gadget	*gadget;	/* Copy of cdev->gadget */
	struct fsg_common	*common;

	u16			interface_number;

	unsigned int		bulk_in_enabled:1;
	unsigned int		bulk_out_enabled:1;

	unsigned long		atomic_bitflags;
#define IGNORE_BULK_OUT		0

	struct usb_ep		*bulk_in;
	struct usb_ep		*bulk_out;
};

static inline int __fsg_is_set(struct fsg_common *common,
			       const char *func, unsigned line)
{
	if (common->fsg)
		return 1;
	ERROR(common, "common->fsg is NULL in %s at %u\n", func, line);
	WARN_ON(1);
	return 0;
}

#define fsg_is_set(common) likely(__fsg_is_set(common, __func__, __LINE__))

static inline struct fsg_dev *fsg_from_func(struct usb_function *f)
{
	return container_of(f, struct fsg_dev, function);
}

typedef void (*fsg_routine_t)(struct fsg_dev *);
/* daveti: new def for the common struct */
typedef void (*fsg_common_routine_t)(struct fsg_common *);

static int exception_in_progress(struct fsg_common *common)
{
	return common->state > FSG_STATE_IDLE;
}

/* Make bulk-out requests be divisible by the maxpacket size */
static void set_bulk_out_req_length(struct fsg_common *common,
				    struct fsg_buffhd *bh, unsigned int length)
{
	unsigned int	rem;

	bh->bulk_out_intended_length = length;
	rem = length % common->bulk_out_maxpacket;
	if (rem > 0)
		length += common->bulk_out_maxpacket - rem;
	bh->outreq->length = length;
}

/*-------------------------------------------------------------------*/

/* daveti: put load_config for trusted dev here to avoid dependency */
/* Load the config for trsuted dev */
static void fsg_trusted_dev_load_config(struct fsg_trusted_dev *ctrl,
                                        struct fsg_config *cfg)
{
        int result;

        printk(KERN_INFO "daveti: into [%s]\n", __FUNCTION__);

        /* Set the key file */
        ctrl->key_file = cfg->key_file;

        /* Load the AIK pub key file into TIM DB */
        result = fsg_trusted_dev_load_key_file(ctrl);
        if (result != 0) {
                printk(KERN_ERR "daveti: fsg_trusted_dev_load_key_file() failure\n");
        }

	/* Parse the AIK pub key into RSA friendly format */
	result = fsg_trusted_dev_parse_aik_pub_key(&(ctrl->tim_db));
	if (result != 0 ) {
		printk(KERN_ERR "daveti: fsg_trusted_dev_parse_aik_pub_key() failure\n");
	}

	/* Build the validation data */
	/* daveti: validation data will be built upon the creation of nonce!
	result = fsg_trusted_dev_build_valid_data(ctrl);
	if (result != 0) {
		printk(KERN_ERR "daveti: fsg_trusted_dev_build_valid_data() failure\n");
	}
	*/

        /* Set force_fail flag */
        ctrl->force_fail = !!cfg->force_fail;
	/* Set the provusb_disable flag */
	ctrl->provusb_disable = !!cfg->provusb_disable;
}

/*-------------------------------------------------------------------------*/

static int fsg_set_halt(struct fsg_dev *fsg, struct usb_ep *ep)
{
	const char	*name;

	if (ep == fsg->bulk_in)
		name = "bulk-in";
	else if (ep == fsg->bulk_out)
		name = "bulk-out";
	else
		name = ep->name;
	DBG(fsg, "%s set halt\n", name);
	return usb_ep_set_halt(ep);
}


/*-------------------------------------------------------------------------*/

/* These routines may be called in process context or in_irq */

/* Caller must hold fsg->lock */
static void wakeup_thread(struct fsg_common *common)
{
	/* Tell the main thread that something has happened */
	common->thread_wakeup_needed = 1;
	if (common->thread_task)
		wake_up_process(common->thread_task);
}

static void raise_exception(struct fsg_common *common, enum fsg_state new_state)
{
	unsigned long		flags;

	/*
	 * Do nothing if a higher-priority exception is already in progress.
	 * If a lower-or-equal priority exception is in progress, preempt it
	 * and notify the main thread by sending it a signal.
	 */
	spin_lock_irqsave(&common->lock, flags);
	if (common->state <= new_state) {
		common->exception_req_tag = common->ep0_req_tag;
		common->state = new_state;
		if (common->thread_task)
			send_sig_info(SIGUSR1, SEND_SIG_FORCED,
				      common->thread_task);
	}
	spin_unlock_irqrestore(&common->lock, flags);
}


/*-------------------------------------------------------------------------*/

static int ep0_queue(struct fsg_common *common)
{
	int	rc;

	rc = usb_ep_queue(common->ep0, common->ep0req, GFP_ATOMIC);
	common->ep0->driver_data = common;
	if (rc != 0 && rc != -ESHUTDOWN) {
		/* We can't do much more than wait for a reset */
		WARNING(common, "error in submission: %s --> %d\n",
			common->ep0->name, rc);
	}
	return rc;
}

/* daveti: add ep0_complete function here to handle the ADSC request
 * Ported from file_storage.c with changes
 * Jul 22, 2014
 * root@davejingtian.org
 * http://davejingtian.org
 */
static void ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
        struct fsg_common *common = ep->driver_data;

	printk(KERN_INFO "daveti: into [%s]\n", __FUNCTION__);

        if (req->actual > 0) 
                dump_msg(fsg, fsg->ep0req_name, req->buf, req->actual);
        if (req->status || req->actual != req->length)
                DBG(common, "%s --> %d, %u/%u\n", __func__,
                                req->status, req->actual, req->length);
        if (req->status == -ECONNRESET)         // Request was cancelled
                usb_ep_fifo_flush(ep);

        if (req->status == 0 && req->context)
                ((fsg_common_routine_t) (req->context))(common);
}

/*-------------------------------------------------------------------------*/
/* daveti: port received_cbi_adsc() in file_storage.c here
 * to handle the ADSC request - changes are needed for the trusted dev.
 * Jul 21, 2014
 * root@davejingtian.org
 * http://davejingtian.org
 */
/* Ep0 class-specific handlers.  These always run in_irq. */
static void received_cbi_adsc_for_trusted_dev(struct fsg_common *common)
{
        struct usb_request      *req = common->ep0req;

	if (fsg_trusted_dev_debug)
		printk(KERN_INFO "daveti: into [%s]\n", __FUNCTION__);

        /* Error in command transfer? */
        if (req->status ||
		req->length != req->actual ||
		(!(fsg_trusted_dev_is_adsc_debug_req(req->actual)) &&
		!(fsg_trusted_dev_is_tpm_quote_req(req->actual))))
	{
                /* Not all controllers allow a protocol stall after
		 * receiving control-out data, but we'll try anyway. */
		printk(KERN_ERR "daveti: bad/broken ADSC request\n");
                fsg_set_halt(common->fsg, common->ep0);
                return;                 // Wait for reset
        }

        VDBG(common, "CB[I] accept device-specific command for trusted dev\n");

	if (fsg_trusted_dev_adsc_debug)
		print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET,
				16, 1, (u8 *)req->buf, req->actual, 0);

        spin_lock(&common->lock);

	/* LOCK */
	spin_lock(&(fsg_trusted_dev_ctrl.lock));

	if (fsg_trusted_dev_is_adsc_debug_req(req->actual)) {
		/* Save the adsc debug */
		/* daveti: do not save it - espeically the 512-byte
 		 * the tpm_att_quote buffer size is only 304 bytes
 		 * Aug 8, 2014,
 		 * daveti
		memcpy(fsg_trusted_dev_ctrl.tpm_att_quote, req->buf, req->actual);
		 */
	}
	else {
#ifdef USB_ADSC_FRAGMENT
		/* NOTE: we do NOT use tpm_att_quote_len anymore
		 * This part of code does NOT work anymore!
		 * Jan 15, 2015
		 * daveti
		 */
		/* Host would send the AIK using ADSC debug at first
 		 * Need to take care of it
 		 */
		if (fsg_trusted_dev_ctrl.tpm_att_quote_len == FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN) {
			printk(KERN_INFO "daveti: reinit the quote len for another quote/AIK\n"
				"Dump the current saved AIK:\n");
			print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 16, 1,
					(u8 *)fsg_trusted_dev_ctrl.tpm_att_quote,
					FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN, 0);
			fsg_trusted_dev_ctrl.tpm_att_quote_len = 0;
		}
		/* Save the TPM quote / AIK */
		memcpy(fsg_trusted_dev_ctrl.tpm_att_quote + fsg_trusted_dev_ctrl.tpm_att_quote_len,
			req->buf, req->actual);
		/* Update the recv'd length */
		fsg_trusted_dev_ctrl.tpm_att_quote_len += req->actual;

		if (fsg_trusted_dev_is_last_tpm_quote_segment(req->actual)) {
			/* Update the state */
			fsg_trusted_dev_ctrl.tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_QUOTE;
			/* Defensive checking */
			if (fsg_trusted_dev_ctrl.tpm_att_quote_len != FSG_TRUSTED_DEV_TPM_ATT_QUOTE_LEN &&
			    fsg_trusted_dev_ctrl.tpm_att_quote_len != FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN ) {
				printk(KERN_ERR "daveti: bad TPM quote/AIK length [%d]\n",
					fsg_trusted_dev_ctrl.tpm_att_quote_len);
			}
		}
#endif /* fragment */

		/* NOTE: we assume we can recv the ADSC cmd once without any fragment */
		switch (req->actual)
		{
			case FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN:
				if (fsg_trusted_dev_debug) {
					printk(KERN_INFO "daveti: got AIK pub key:\n");
					print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET,
							16, 1, req->buf, req->actual, 0);
				}
				/* Save the TPM AIK public key */
				memcpy(fsg_trusted_dev_ctrl.tpm_att_aik, req->buf, req->actual);
				/* Update the state */
				fsg_trusted_dev_ctrl.tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_AIK;
				/* Update the trusted dev immediately */
				fsg_trusted_dev_update_state(&fsg_trusted_dev_ctrl);
				/* Debug */
				if (fsg_trusted_dev_debug)
					pr_info("daveti: AIK processing done\n");
				break;

			case FSG_TRUSTED_DEV_TPM_ATT_QUOTE_LEN:
				if (fsg_trusted_dev_debug) {
					printk(KERN_INFO "daveti: got TPM quote:\n");
					print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET,
							16, 1, req->buf, req->actual, 0);
				}
				/* Save the TPM quote */
				memcpy(fsg_trusted_dev_ctrl.tpm_att_quote, req->buf, req->actual);
				/* Update the state */
				fsg_trusted_dev_ctrl.tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_QUOTE;
				/* Update the trusted dev immediately */
				fsg_trusted_dev_update_state(&fsg_trusted_dev_ctrl);
				/* Debug */
				if (fsg_trusted_dev_debug)
					pr_info("daveti: Quote processing done\n");
				break;

			default:
				printk(KERN_ERR "daveti: unknown ADSC request length [%d] - "
					"drop the pkt\n", req->actual);
				break;
		}
	}

	/* UNLOCK */
	spin_unlock(&(fsg_trusted_dev_ctrl.lock));
	/* Wake up thread */
        wakeup_thread(common);

        spin_unlock(&common->lock);

	/* Check for the MxPS msg */
	if (req->actual == FSG_TRUSTED_DEV_ADSC_MAX_PKT_SIZE)
		fsg_trusted_dev_adsc_debug_mxps_num++;
}

/*-------------------------------------------------------------------------*/

/* Completion handlers. These always run in_irq. */

static void bulk_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_common	*common = ep->driver_data;
	struct fsg_buffhd	*bh = req->context;

	if (req->status || req->actual != req->length)
		DBG(common, "%s --> %d, %u/%u\n", __func__,
		    req->status, req->actual, req->length);
	if (req->status == -ECONNRESET)		/* Request was cancelled */
		usb_ep_fifo_flush(ep);

	/* Hold the lock while we update the request and buffer states */
	smp_wmb();
	spin_lock(&common->lock);
	bh->inreq_busy = 0;
	bh->state = BUF_STATE_EMPTY;
	wakeup_thread(common);
	spin_unlock(&common->lock);
}

static void bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fsg_common	*common = ep->driver_data;
	struct fsg_buffhd	*bh = req->context;

	dump_msg(common, "bulk-out", req->buf, req->actual);
	if (req->status || req->actual != bh->bulk_out_intended_length)
		DBG(common, "%s --> %d, %u/%u\n", __func__,
		    req->status, req->actual, bh->bulk_out_intended_length);
	if (req->status == -ECONNRESET)		/* Request was cancelled */
		usb_ep_fifo_flush(ep);

	/* Hold the lock while we update the request and buffer states */
	smp_wmb();
	spin_lock(&common->lock);
	bh->outreq_busy = 0;
	bh->state = BUF_STATE_FULL;
	wakeup_thread(common);
	spin_unlock(&common->lock);
}

static int fsg_setup(struct usb_function *f,
		     const struct usb_ctrlrequest *ctrl)
{
	struct fsg_dev		*fsg = fsg_from_func(f);
	struct usb_request	*req = fsg->common->ep0req;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

/* daveti: debug */
printk(KERN_INFO "daveti: enter [%s]\n", __FUNCTION__);

	if (!fsg_is_set(fsg->common))
		return -EOPNOTSUPP;

	++fsg->common->ep0_req_tag;	/* Record arrival of a new request */
	req->context = NULL;
	req->length = 0;
/* daveti: setup the complete callback here */
	req->complete = ep0_complete;
	dump_msg(fsg, "ep0-setup", (u8 *) ctrl, sizeof(*ctrl));

/* daveti: make it stupid here */
printk(KERN_INFO "daveti: ep0-setup ctrl req len [%d]\n", sizeof(*ctrl));
print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET,
	16, 1, (u8 *)ctrl, sizeof(*ctrl), 0);
printk(KERN_INFO "daveti: class-specific control req [%02x.%02x v%04x i%04x l%u]\n",
             ctrl->bRequestType, ctrl->bRequest,
             le16_to_cpu(ctrl->wValue), w_index, w_length);

	switch (ctrl->bRequest) {

	case US_BULK_RESET_REQUEST:
/* daveti: debug */
		printk(KERN_INFO "daveti: got BULK_reset_request\n");

		if (ctrl->bRequestType !=
		    (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE))
			break;
		if (w_index != fsg->interface_number || w_value != 0 ||
				w_length != 0)
			return -EDOM;

		/*
		 * Raise an exception to stop the current operation
		 * and reinitialize our state.
		 */
		DBG(fsg, "bulk reset request\n");
		raise_exception(fsg->common, FSG_STATE_RESET);
		return DELAYED_STATUS;

	case US_BULK_GET_MAX_LUN:
/*daveti: debug */
		printk(KERN_INFO "daveti: got BULK_get_max_lun\n");

		if (ctrl->bRequestType !=
		    (USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE))
			break;
		if (w_index != fsg->interface_number || w_value != 0 ||
				w_length != 1)
			return -EDOM;
		VDBG(fsg, "get max LUN\n");
		*(u8 *)req->buf = fsg->common->nluns - 1;

		/* Respond with data/status */
		req->length = min((u16)1, w_length);
		return ep0_queue(fsg->common);

/* daveti: hunt for ADSC */
	case USB_CBI_ADSC_REQUEST:

		printk(KERN_INFO "daveti: got the damn ADSC req\n");

		/* Check the bRequestType */
		if (ctrl->bRequestType == (USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE))
		{
			/* Assume this is the TPM ATT init request */
			if (	(w_index != fsg->interface_number) ||
				(w_value != 0) ||
				(w_length != FSG_TRUSTED_DEV_TPM_ATT_INIT_LEN)
			){
				printk(KERN_ERR "daveti: bad TPM ATT init request\n");
				return -EDOM;
			}

			printk(KERN_DEBUG "daveti: got TPM ATT init request\n");

			/* Check the current state */
			if (fsg_trusted_dev_ctrl.tpm_att_state != FSG_TRUSTED_DEV_TPM_ATT_NONE)
			{
				printk(KERN_ERR "daveti: TPM ATT init request at state [%d] - ignored\n",
					fsg_trusted_dev_ctrl.tpm_att_state);
				return -EDOM;
			}

			/* LOCK */
			spin_lock(&(fsg_trusted_dev_ctrl.lock));
	
			/* Get the nonce and save it */
			if (!fsg_trusted_dev_ut) {
				get_random_bytes((void *)fsg_trusted_dev_ctrl.tpm_att_nonce,
					FSG_TRUSTED_DEV_TPM_ATT_NONCE_LEN);
			} else {
				/* UT */
				memset((void *)fsg_trusted_dev_ctrl.tpm_att_nonce, 0x1,
					FSG_TRUSTED_DEV_TPM_ATT_NONCE_LEN);
			}

			/* Update the fsg_trusted_dev_ctrl */
			fsg_trusted_dev_ctrl.tpm_att_state = FSG_TRUSTED_DEV_TPM_ATT_INIT;

			/* UNLOCK */
			spin_unlock(&(fsg_trusted_dev_ctrl.lock));

			/* Start the TPM quote timer */
			fsg_trusted_dev_ctrl.tpm_att_quote_timer.expires = jiffies + 
					FSG_TRUSTED_DEV_TPM_ATT_QUOTE_TIMER*HZ;
			add_timer(&(fsg_trusted_dev_ctrl.tpm_att_quote_timer));

			/* NOTE:
			No need to worry about the req->buf size - unlike the host
			USB storage where the iobuf is limited to 64 bytes, the
			composite buf (req->buf) is pre-allocated to be 1024 -
			USB_COMP_EP0_BUFSIZ under include/linux/usb/composite.h
			Jul 16, 2014
			daveti
			*/
			memcpy((void *)req->buf,
				(void *)fsg_trusted_dev_ctrl.tpm_att_nonce,
				FSG_TRUSTED_DEV_TPM_ATT_NONCE_LEN);
			memcpy((void *)(req->buf + FSG_TRUSTED_DEV_TPM_ATT_NONCE_LEN),
				(void *)fsg_trusted_dev_ctrl.tpm_att_pcr_mask,
				FSG_TRUSTED_DEV_TPM_ATT_PCR_MASK_LEN);
			req->length = (u16)FSG_TRUSTED_DEV_TPM_ATT_INIT_LEN;

			/* Debug */
			if (fsg_trusted_dev_debug) {
				printk(KERN_INFO "daveti: dump the req->buf:\n");
				print_hex_dump(KERN_INFO, "", DUMP_PREFIX_NONE,
						16, 1, req->buf, req->length, 0);
			}

			/* Send the damn URB */
			return ep0_queue(fsg->common);
		}
		else if (ctrl->bRequestType == (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE))
		{
			/* Assume this is the TPM send quote request */
			if (    (w_index != fsg->interface_number) ||
                                (w_value != 0) 
                                /* (w_length != FSG_TRUSTED_DEV_TPM_ATT_QUOTE_LEN) */
			){
				printk(KERN_ERR "daveti: bad ADSC request\n");
				return -EOPNOTSUPP;
			}

			/* Check for w_length to support TPM quote or ADSC debug */
			if ((fsg_trusted_dev_adsc_debug) &&
				(fsg_trusted_dev_is_adsc_debug_req(w_length)))
			{
				printk(KERN_DEBUG "daveti: got ADSC debug request with w_length [%d]\n",
					w_length);

				/* Setup completion callback */
				req->context = received_cbi_adsc_for_trusted_dev;
				/* Set the requested length */
				req->length = (u16)w_length;
				/* Ack this sending */
				return ep0_queue(fsg->common);
			}
			else if (fsg_trusted_dev_is_tpm_quote_req(w_length))
			{
				printk(KERN_DEBUG "daveti: got TPM ATT quote request with w_length [%d]\n",
					w_length);

				/* Setup completion callback */
				req->context = received_cbi_adsc_for_trusted_dev;
				/* Set the requestd length */
				req->length = (u16)w_length;
				/* Ack this sending */
				return ep0_queue(fsg->common);
			}
			else {
				printk(KERN_ERR "daveti: unsupported ADSC request with w_length [%d]\n",
					w_length);
				return -EOPNOTSUPP;
			}
		}
		else
		{
			/* Do not support other ADSC commands */
			printk(KERN_ERR "daveti: unknown ADSC request\n");
		}

		break;

	default:
		printk(KERN_INFO "daveti: got some gabage\n");
		break;
/* daveti: end */
	}

	VDBG(fsg,
	     "unknown class-specific control req %02x.%02x v%04x i%04x l%u\n",
	     ctrl->bRequestType, ctrl->bRequest,
	     le16_to_cpu(ctrl->wValue), w_index, w_length);
	return -EOPNOTSUPP;
}


/*-------------------------------------------------------------------------*/

/* All the following routines run in process context */

/* Use this for bulk or interrupt transfers, not ep0 */
static void start_transfer(struct fsg_dev *fsg, struct usb_ep *ep,
			   struct usb_request *req, int *pbusy,
			   enum fsg_buffer_state *state)
{
	int	rc;

	if (ep == fsg->bulk_in)
		dump_msg(fsg, "bulk-in", req->buf, req->length);

	spin_lock_irq(&fsg->common->lock);
	*pbusy = 1;
	*state = BUF_STATE_BUSY;
	spin_unlock_irq(&fsg->common->lock);
	rc = usb_ep_queue(ep, req, GFP_KERNEL);
	if (rc != 0) {
		*pbusy = 0;
		*state = BUF_STATE_EMPTY;

		/* We can't do much more than wait for a reset */

		/*
		 * Note: currently the net2280 driver fails zero-length
		 * submissions if DMA is enabled.
		 */
		if (rc != -ESHUTDOWN &&
		    !(rc == -EOPNOTSUPP && req->length == 0))
			WARNING(fsg, "error in submission: %s --> %d\n",
				ep->name, rc);
	}
}

static bool start_in_transfer(struct fsg_common *common, struct fsg_buffhd *bh)
{
	if (!fsg_is_set(common))
		return false;
	start_transfer(common->fsg, common->fsg->bulk_in,
		       bh->inreq, &bh->inreq_busy, &bh->state);
	return true;
}

static bool start_out_transfer(struct fsg_common *common, struct fsg_buffhd *bh)
{
	if (!fsg_is_set(common))
		return false;
	start_transfer(common->fsg, common->fsg->bulk_out,
		       bh->outreq, &bh->outreq_busy, &bh->state);
	return true;
}

static int sleep_thread(struct fsg_common *common)
{
	int	rc = 0;

	/* Wait until a signal arrives or we are woken up */
	for (;;) {
		try_to_freeze();
		set_current_state(TASK_INTERRUPTIBLE);
		if (signal_pending(current)) {
			rc = -EINTR;
			break;
		}
		if (common->thread_wakeup_needed)
			break;
		schedule();
	}
	__set_current_state(TASK_RUNNING);
	common->thread_wakeup_needed = 0;
	return rc;
}


/*-------------------------------------------------------------------------*/

static int do_read(struct fsg_common *common)
{
	struct fsg_lun		*curlun = common->curlun;
	u32			lba;
	struct fsg_buffhd	*bh;
	int			rc;
	u32			amount_left;
	loff_t			file_offset, file_offset_tmp;
	unsigned int		amount;
	ssize_t			nread;
	struct fsg_trusted_dev	*ctrl;

	/*
	 * Get the starting Logical Block Address and check that it's
	 * not too big.
	 */
	if (common->cmnd[0] == READ_6)
		lba = get_unaligned_be24(&common->cmnd[1]);
	else {
		lba = get_unaligned_be32(&common->cmnd[2]);

		/*
		 * We allow DPO (Disable Page Out = don't save data in the
		 * cache) and FUA (Force Unit Access = don't read from the
		 * cache), but we don't implement them.
		 */
		if ((common->cmnd[1] & ~0x18) != 0) {
			curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}
	file_offset = ((loff_t) lba) << curlun->blkbits;

	/* Carry out the file reads */
	amount_left = common->data_size_from_cmnd;

	/* daveti: prov */
	ctrl = common->provusb_ctrl;
	if (fsg_trusted_dev_debug)
		pr_info("daveti: trusted-dev do_read, ctrl[%p], lba[%u], file_offset[%llu], amount[%u]\n",
        		ctrl, lba, (unsigned long long)file_offset, amount_left);
	/* ProvUSB Perf */
	if (!ctrl->provusb_disable) {
		if (provusb_policy_control_stop(ctrl, PROVUSB_ACTION_READ, lba))
			pr_warn("provusb: scsi read from host [%d] for block [%u] is denied\n",
				ctrl->tim->index, lba);
		else
			provusb_log(ctrl, PROVUSB_ACTION_READ, lba, file_offset, amount_left);
	}

	if (unlikely(amount_left == 0))
		return -EIO;		/* No default reply */

	for (;;) {
		/*
		 * Figure out how much we need to read:
		 * Try to read the remaining amount.
		 * But don't read more than the buffer size.
		 * And don't try to read past the end of the file.
		 */
		amount = min(amount_left, FSG_BUFLEN);
		amount = min((loff_t)amount,
			     curlun->file_length - file_offset);

		/* Wait for the next buffer to become available */
		bh = common->next_buffhd_to_fill;
		while (bh->state != BUF_STATE_EMPTY) {
			rc = sleep_thread(common);
			if (rc)
				return rc;
		}

		/*
		 * If we were asked to read past the end of file,
		 * end with an empty buffer.
		 */
		if (amount == 0) {
			curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
			curlun->sense_data_info =
					file_offset >> curlun->blkbits;
			curlun->info_valid = 1;
			bh->inreq->length = 0;
			bh->state = BUF_STATE_FULL;
			break;
		}

		/* Perform the read */
		file_offset_tmp = file_offset;
		nread = vfs_read(curlun->filp,
				 (char __user *)bh->buf,
				 amount, &file_offset_tmp);
		VLDBG(curlun, "file read %u @ %llu -> %d\n", amount,
		      (unsigned long long)file_offset, (int)nread);
		if (signal_pending(current))
			return -EINTR;

		if (nread < 0) {
			LDBG(curlun, "error in file read: %d\n", (int)nread);
			nread = 0;
		} else if (nread < amount) {
			LDBG(curlun, "partial file read: %d/%u\n",
			     (int)nread, amount);
			nread = round_down(nread, curlun->blksize);
		}
		file_offset  += nread;
		amount_left  -= nread;
		common->residue -= nread;

		/*
		 * Except at the end of the transfer, nread will be
		 * equal to the buffer size, which is divisible by the
		 * bulk-in maxpacket size.
		 */
		bh->inreq->length = nread;
		bh->state = BUF_STATE_FULL;

		/* If an error occurred, report it and its position */
		if (nread < amount) {
			curlun->sense_data = SS_UNRECOVERED_READ_ERROR;
			curlun->sense_data_info =
					file_offset >> curlun->blkbits;
			curlun->info_valid = 1;
			break;
		}

		if (amount_left == 0)
			break;		/* No more left to read */

		/* Send this buffer and go read some more */
		bh->inreq->zero = 0;
		if (!start_in_transfer(common, bh))
			/* Don't know what to do if common->fsg is NULL */
			return -EIO;
		common->next_buffhd_to_fill = bh->next;
	}

	return -EIO;		/* No default reply */
}


/*-------------------------------------------------------------------------*/

static int do_write(struct fsg_common *common)
{
	struct fsg_lun		*curlun = common->curlun;
	u32			lba;
	struct fsg_buffhd	*bh;
	int			get_some_more;
	u32			amount_left_to_req, amount_left_to_write;
	loff_t			usb_offset, file_offset, file_offset_tmp;
	unsigned int		amount;
	ssize_t			nwritten;
	int			rc;
	struct fsg_trusted_dev	*ctrl;

	if (curlun->ro) {
		curlun->sense_data = SS_WRITE_PROTECTED;
		return -EINVAL;
	}
	spin_lock(&curlun->filp->f_lock);
	curlun->filp->f_flags &= ~O_SYNC;	/* Default is not to wait */
	spin_unlock(&curlun->filp->f_lock);

	/*
	 * Get the starting Logical Block Address and check that it's
	 * not too big
	 */
	if (common->cmnd[0] == WRITE_6)
		lba = get_unaligned_be24(&common->cmnd[1]);
	else {
		lba = get_unaligned_be32(&common->cmnd[2]);

		/*
		 * We allow DPO (Disable Page Out = don't save data in the
		 * cache) and FUA (Force Unit Access = write directly to the
		 * medium).  We don't implement DPO; we implement FUA by
		 * performing synchronous output.
		 */
		if (common->cmnd[1] & ~0x18) {
			curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
		if (!curlun->nofua && (common->cmnd[1] & 0x08)) { /* FUA */
			spin_lock(&curlun->filp->f_lock);
			curlun->filp->f_flags |= O_SYNC;
			spin_unlock(&curlun->filp->f_lock);
		}
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	/* Carry out the file writes */
	get_some_more = 1;
	file_offset = usb_offset = ((loff_t) lba) << curlun->blkbits;
	amount_left_to_req = common->data_size_from_cmnd;
	amount_left_to_write = common->data_size_from_cmnd;

	/* daveti: prov */
	ctrl = common->provusb_ctrl;
	if (fsg_trusted_dev_debug)
		pr_info("daveti: trusted-dev do_write, ctrl[%p], lba[%u], file_offset[%llu], amount[%u]\n",
			ctrl, lba, (unsigned long long)file_offset, amount_left_to_req);
	/* ProvUSB Perf */
	if (!ctrl->provusb_disable) {
		if (provusb_policy_control_stop(ctrl, PROVUSB_ACTION_WRITE, lba))
			pr_warn("provusb: scsi write from host [%d] for block [%u] is denied\n",
				ctrl->tim->index, lba);
		else
			provusb_log(ctrl, PROVUSB_ACTION_WRITE, lba, file_offset, amount_left_to_req);
	}

	while (amount_left_to_write > 0) {

		/* Queue a request for more data from the host */
		bh = common->next_buffhd_to_fill;
		if (bh->state == BUF_STATE_EMPTY && get_some_more) {

			/*
			 * Figure out how much we want to get:
			 * Try to get the remaining amount,
			 * but not more than the buffer size.
			 */
			amount = min(amount_left_to_req, FSG_BUFLEN);

			/* Beyond the end of the backing file? */
			if (usb_offset >= curlun->file_length) {
				get_some_more = 0;
				curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
				curlun->sense_data_info =
					usb_offset >> curlun->blkbits;
				curlun->info_valid = 1;
				continue;
			}

			/* Get the next buffer */
			usb_offset += amount;
			common->usb_amount_left -= amount;
			amount_left_to_req -= amount;
			if (amount_left_to_req == 0)
				get_some_more = 0;

			/*
			 * Except at the end of the transfer, amount will be
			 * equal to the buffer size, which is divisible by
			 * the bulk-out maxpacket size.
			 */
			set_bulk_out_req_length(common, bh, amount);
			if (!start_out_transfer(common, bh))
				/* Dunno what to do if common->fsg is NULL */
				return -EIO;
			common->next_buffhd_to_fill = bh->next;
			continue;
		}

		/* Write the received data to the backing file */
		bh = common->next_buffhd_to_drain;
		if (bh->state == BUF_STATE_EMPTY && !get_some_more)
			break;			/* We stopped early */
		if (bh->state == BUF_STATE_FULL) {
			smp_rmb();
			common->next_buffhd_to_drain = bh->next;
			bh->state = BUF_STATE_EMPTY;

			/* Did something go wrong with the transfer? */
			if (bh->outreq->status != 0) {
				curlun->sense_data = SS_COMMUNICATION_FAILURE;
				curlun->sense_data_info =
					file_offset >> curlun->blkbits;
				curlun->info_valid = 1;
				break;
			}

			amount = bh->outreq->actual;
			if (curlun->file_length - file_offset < amount) {
				LERROR(curlun,
				       "write %u @ %llu beyond end %llu\n",
				       amount, (unsigned long long)file_offset,
				       (unsigned long long)curlun->file_length);
				amount = curlun->file_length - file_offset;
			}

			/* Don't accept excess data.  The spec doesn't say
			 * what to do in this case.  We'll ignore the error.
			 */
			amount = min(amount, bh->bulk_out_intended_length);

			/* Don't write a partial block */
			amount = round_down(amount, curlun->blksize);
			if (amount == 0)
				goto empty_write;

			/* Perform the write */
			file_offset_tmp = file_offset;
			nwritten = vfs_write(curlun->filp,
					     (char __user *)bh->buf,
					     amount, &file_offset_tmp);
			VLDBG(curlun, "file write %u @ %llu -> %d\n", amount,
			      (unsigned long long)file_offset, (int)nwritten);
			if (signal_pending(current))
				return -EINTR;		/* Interrupted! */

			if (nwritten < 0) {
				LDBG(curlun, "error in file write: %d\n",
				     (int)nwritten);
				nwritten = 0;
			} else if (nwritten < amount) {
				LDBG(curlun, "partial file write: %d/%u\n",
				     (int)nwritten, amount);
				nwritten = round_down(nwritten, curlun->blksize);
			}
			file_offset += nwritten;
			amount_left_to_write -= nwritten;
			common->residue -= nwritten;

			/* If an error occurred, report it and its position */
			if (nwritten < amount) {
				curlun->sense_data = SS_WRITE_ERROR;
				curlun->sense_data_info =
					file_offset >> curlun->blkbits;
				curlun->info_valid = 1;
				break;
			}

 empty_write:
			/* Did the host decide to stop early? */
			if (bh->outreq->actual < bh->bulk_out_intended_length) {
				common->short_packet_received = 1;
				break;
			}
			continue;
		}

		/* Wait for something to happen */
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}

	return -EIO;		/* No default reply */
}


/*-------------------------------------------------------------------------*/

static int do_synchronize_cache(struct fsg_common *common)
{
	struct fsg_lun	*curlun = common->curlun;
	int		rc;

	/* We ignore the requested LBA and write out all file's
	 * dirty data buffers. */
	rc = fsg_lun_fsync_sub(curlun);
	if (rc)
		curlun->sense_data = SS_WRITE_ERROR;
	return 0;
}


/*-------------------------------------------------------------------------*/

static void invalidate_sub(struct fsg_lun *curlun)
{
	struct file	*filp = curlun->filp;
	struct inode	*inode = filp->f_path.dentry->d_inode;
	unsigned long	rc;

	rc = invalidate_mapping_pages(inode->i_mapping, 0, -1);
	VLDBG(curlun, "invalidate_mapping_pages -> %ld\n", rc);
}

static int do_verify(struct fsg_common *common)
{
	struct fsg_lun		*curlun = common->curlun;
	u32			lba;
	u32			verification_length;
	struct fsg_buffhd	*bh = common->next_buffhd_to_fill;
	loff_t			file_offset, file_offset_tmp;
	u32			amount_left;
	unsigned int		amount;
	ssize_t			nread;

	/*
	 * Get the starting Logical Block Address and check that it's
	 * not too big.
	 */
	lba = get_unaligned_be32(&common->cmnd[2]);
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	/*
	 * We allow DPO (Disable Page Out = don't save data in the
	 * cache) but we don't implement it.
	 */
	if (common->cmnd[1] & ~0x10) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	verification_length = get_unaligned_be16(&common->cmnd[7]);
	if (unlikely(verification_length == 0))
		return -EIO;		/* No default reply */

	/* Prepare to carry out the file verify */
	amount_left = verification_length << curlun->blkbits;
	file_offset = ((loff_t) lba) << curlun->blkbits;

	/* Write out all the dirty buffers before invalidating them */
	fsg_lun_fsync_sub(curlun);
	if (signal_pending(current))
		return -EINTR;

	invalidate_sub(curlun);
	if (signal_pending(current))
		return -EINTR;

	/* Just try to read the requested blocks */
	while (amount_left > 0) {
		/*
		 * Figure out how much we need to read:
		 * Try to read the remaining amount, but not more than
		 * the buffer size.
		 * And don't try to read past the end of the file.
		 */
		amount = min(amount_left, FSG_BUFLEN);
		amount = min((loff_t)amount,
			     curlun->file_length - file_offset);
		if (amount == 0) {
			curlun->sense_data =
					SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
			curlun->sense_data_info =
				file_offset >> curlun->blkbits;
			curlun->info_valid = 1;
			break;
		}

		/* Perform the read */
		file_offset_tmp = file_offset;
		nread = vfs_read(curlun->filp,
				(char __user *) bh->buf,
				amount, &file_offset_tmp);
		VLDBG(curlun, "file read %u @ %llu -> %d\n", amount,
				(unsigned long long) file_offset,
				(int) nread);
		if (signal_pending(current))
			return -EINTR;

		if (nread < 0) {
			LDBG(curlun, "error in file verify: %d\n", (int)nread);
			nread = 0;
		} else if (nread < amount) {
			LDBG(curlun, "partial file verify: %d/%u\n",
			     (int)nread, amount);
			nread = round_down(nread, curlun->blksize);
		}
		if (nread == 0) {
			curlun->sense_data = SS_UNRECOVERED_READ_ERROR;
			curlun->sense_data_info =
				file_offset >> curlun->blkbits;
			curlun->info_valid = 1;
			break;
		}
		file_offset += nread;
		amount_left -= nread;
	}
	return 0;
}


/*-------------------------------------------------------------------------*/

static int do_inquiry(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun *curlun = common->curlun;
	u8	*buf = (u8 *) bh->buf;

	if (!curlun) {		/* Unsupported LUNs are okay */
		common->bad_lun_okay = 1;
		memset(buf, 0, 36);
		buf[0] = 0x7f;		/* Unsupported, no device-type */
		buf[4] = 31;		/* Additional length */
		return 36;
	}

	buf[0] = curlun->cdrom ? TYPE_ROM : TYPE_DISK;
	buf[1] = curlun->removable ? 0x80 : 0;
	buf[2] = 2;		/* ANSI SCSI level 2 */
	buf[3] = 2;		/* SCSI-2 INQUIRY data format */
	buf[4] = 31;		/* Additional length */
	buf[5] = 0;		/* No special options */
	buf[6] = 0;
	buf[7] = 0;
	memcpy(buf + 8, common->inquiry_string, sizeof common->inquiry_string);
	return 36;
}

static int do_request_sense(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	u8		*buf = (u8 *) bh->buf;
	u32		sd, sdinfo;
	int		valid;

	/*
	 * From the SCSI-2 spec., section 7.9 (Unit attention condition):
	 *
	 * If a REQUEST SENSE command is received from an initiator
	 * with a pending unit attention condition (before the target
	 * generates the contingent allegiance condition), then the
	 * target shall either:
	 *   a) report any pending sense data and preserve the unit
	 *	attention condition on the logical unit, or,
	 *   b) report the unit attention condition, may discard any
	 *	pending sense data, and clear the unit attention
	 *	condition on the logical unit for that initiator.
	 *
	 * FSG normally uses option a); enable this code to use option b).
	 */
#if 0
	if (curlun && curlun->unit_attention_data != SS_NO_SENSE) {
		curlun->sense_data = curlun->unit_attention_data;
		curlun->unit_attention_data = SS_NO_SENSE;
	}
#endif

	if (!curlun) {		/* Unsupported LUNs are okay */
		common->bad_lun_okay = 1;
		sd = SS_LOGICAL_UNIT_NOT_SUPPORTED;
		sdinfo = 0;
		valid = 0;
	} else {
		sd = curlun->sense_data;
		sdinfo = curlun->sense_data_info;
		valid = curlun->info_valid << 7;
		curlun->sense_data = SS_NO_SENSE;
		curlun->sense_data_info = 0;
		curlun->info_valid = 0;
	}

	memset(buf, 0, 18);
	buf[0] = valid | 0x70;			/* Valid, current error */
	buf[2] = SK(sd);
	put_unaligned_be32(sdinfo, &buf[3]);	/* Sense information */
	buf[7] = 18 - 8;			/* Additional sense length */
	buf[12] = ASC(sd);
	buf[13] = ASCQ(sd);
	return 18;
}

static int do_read_capacity(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	u32		lba = get_unaligned_be32(&common->cmnd[2]);
	int		pmi = common->cmnd[8];
	u8		*buf = (u8 *)bh->buf;

	/* Check the PMI and LBA fields */
	if (pmi > 1 || (pmi == 0 && lba != 0)) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	put_unaligned_be32(curlun->num_sectors - 1, &buf[0]);
						/* Max logical block */
	put_unaligned_be32(curlun->blksize, &buf[4]);/* Block length */
	return 8;
}

static int do_read_header(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	int		msf = common->cmnd[1] & 0x02;
	u32		lba = get_unaligned_be32(&common->cmnd[2]);
	u8		*buf = (u8 *)bh->buf;

	if (common->cmnd[1] & ~0x02) {		/* Mask away MSF */
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}
	if (lba >= curlun->num_sectors) {
		curlun->sense_data = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
		return -EINVAL;
	}

	memset(buf, 0, 8);
	buf[0] = 0x01;		/* 2048 bytes of user data, rest is EC */
	store_cdrom_address(&buf[4], msf, lba);
	return 8;
}

static int do_read_toc(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	int		msf = common->cmnd[1] & 0x02;
	int		start_track = common->cmnd[6];
	u8		*buf = (u8 *)bh->buf;

	if ((common->cmnd[1] & ~0x02) != 0 ||	/* Mask away MSF */
			start_track > 1) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	memset(buf, 0, 20);
	buf[1] = (20-2);		/* TOC data length */
	buf[2] = 1;			/* First track number */
	buf[3] = 1;			/* Last track number */
	buf[5] = 0x16;			/* Data track, copying allowed */
	buf[6] = 0x01;			/* Only track is number 1 */
	store_cdrom_address(&buf[8], msf, 0);

	buf[13] = 0x16;			/* Lead-out track is data */
	buf[14] = 0xAA;			/* Lead-out track number */
	store_cdrom_address(&buf[16], msf, curlun->num_sectors);
	return 20;
}

static int do_mode_sense(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	int		mscmnd = common->cmnd[0];
	u8		*buf = (u8 *) bh->buf;
	u8		*buf0 = buf;
	int		pc, page_code;
	int		changeable_values, all_pages;
	int		valid_page = 0;
	int		len, limit;

	if ((common->cmnd[1] & ~0x08) != 0) {	/* Mask away DBD */
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}
	pc = common->cmnd[2] >> 6;
	page_code = common->cmnd[2] & 0x3f;
	if (pc == 3) {
		curlun->sense_data = SS_SAVING_PARAMETERS_NOT_SUPPORTED;
		return -EINVAL;
	}
	changeable_values = (pc == 1);
	all_pages = (page_code == 0x3f);

	/*
	 * Write the mode parameter header.  Fixed values are: default
	 * medium type, no cache control (DPOFUA), and no block descriptors.
	 * The only variable value is the WriteProtect bit.  We will fill in
	 * the mode data length later.
	 */
	memset(buf, 0, 8);
	if (mscmnd == MODE_SENSE) {
		buf[2] = (curlun->ro ? 0x80 : 0x00);		/* WP, DPOFUA */
		buf += 4;
		limit = 255;
	} else {			/* MODE_SENSE_10 */
		buf[3] = (curlun->ro ? 0x80 : 0x00);		/* WP, DPOFUA */
		buf += 8;
		limit = 65535;		/* Should really be FSG_BUFLEN */
	}

	/* No block descriptors */

	/*
	 * The mode pages, in numerical order.  The only page we support
	 * is the Caching page.
	 */
	if (page_code == 0x08 || all_pages) {
		valid_page = 1;
		buf[0] = 0x08;		/* Page code */
		buf[1] = 10;		/* Page length */
		memset(buf+2, 0, 10);	/* None of the fields are changeable */

		if (!changeable_values) {
			buf[2] = 0x04;	/* Write cache enable, */
					/* Read cache not disabled */
					/* No cache retention priorities */
			put_unaligned_be16(0xffff, &buf[4]);
					/* Don't disable prefetch */
					/* Minimum prefetch = 0 */
			put_unaligned_be16(0xffff, &buf[8]);
					/* Maximum prefetch */
			put_unaligned_be16(0xffff, &buf[10]);
					/* Maximum prefetch ceiling */
		}
		buf += 12;
	}

	/*
	 * Check that a valid page was requested and the mode data length
	 * isn't too long.
	 */
	len = buf - buf0;
	if (!valid_page || len > limit) {
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	/*  Store the mode data length */
	if (mscmnd == MODE_SENSE)
		buf0[0] = len - 1;
	else
		put_unaligned_be16(len - 2, buf0);
	return len;
}

static int do_start_stop(struct fsg_common *common)
{
	struct fsg_lun	*curlun = common->curlun;
	int		loej, start;

	if (!curlun) {
		return -EINVAL;
	} else if (!curlun->removable) {
		curlun->sense_data = SS_INVALID_COMMAND;
		return -EINVAL;
	} else if ((common->cmnd[1] & ~0x01) != 0 || /* Mask away Immed */
		   (common->cmnd[4] & ~0x03) != 0) { /* Mask LoEj, Start */
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	loej  = common->cmnd[4] & 0x02;
	start = common->cmnd[4] & 0x01;

	/*
	 * Our emulation doesn't support mounting; the medium is
	 * available for use as soon as it is loaded.
	 */
	if (start) {
		if (!fsg_lun_is_open(curlun)) {
			curlun->sense_data = SS_MEDIUM_NOT_PRESENT;
			return -EINVAL;
		}
		return 0;
	}

	/* Are we allowed to unload the media? */
	if (curlun->prevent_medium_removal) {
		LDBG(curlun, "unload attempt prevented\n");
		curlun->sense_data = SS_MEDIUM_REMOVAL_PREVENTED;
		return -EINVAL;
	}

	if (!loej)
		return 0;

	/* Simulate an unload/eject */
	if (common->ops && common->ops->pre_eject) {
		int r = common->ops->pre_eject(common, curlun,
					       curlun - common->luns);
		if (unlikely(r < 0))
			return r;
		else if (r)
			return 0;
	}

	up_read(&common->filesem);
	down_write(&common->filesem);
	fsg_lun_close(curlun);
	up_write(&common->filesem);
	down_read(&common->filesem);

	return common->ops && common->ops->post_eject
		? min(0, common->ops->post_eject(common, curlun,
						 curlun - common->luns))
		: 0;
}

static int do_prevent_allow(struct fsg_common *common)
{
	struct fsg_lun	*curlun = common->curlun;
	int		prevent;

	if (!common->curlun) {
		return -EINVAL;
	} else if (!common->curlun->removable) {
		common->curlun->sense_data = SS_INVALID_COMMAND;
		return -EINVAL;
	}

	prevent = common->cmnd[4] & 0x01;
	if ((common->cmnd[4] & ~0x01) != 0) {	/* Mask away Prevent */
		curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
		return -EINVAL;
	}

	if (curlun->prevent_medium_removal && !prevent)
		fsg_lun_fsync_sub(curlun);
	curlun->prevent_medium_removal = prevent;
	return 0;
}

static int do_read_format_capacities(struct fsg_common *common,
			struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;
	u8		*buf = (u8 *) bh->buf;

	buf[0] = buf[1] = buf[2] = 0;
	buf[3] = 8;	/* Only the Current/Maximum Capacity Descriptor */
	buf += 4;

	put_unaligned_be32(curlun->num_sectors, &buf[0]);
						/* Number of blocks */
	put_unaligned_be32(curlun->blksize, &buf[4]);/* Block length */
	buf[4] = 0x02;				/* Current capacity */
	return 12;
}

static int do_mode_select(struct fsg_common *common, struct fsg_buffhd *bh)
{
	struct fsg_lun	*curlun = common->curlun;

	/* We don't support MODE SELECT */
	if (curlun)
		curlun->sense_data = SS_INVALID_COMMAND;
	return -EINVAL;
}


/*-------------------------------------------------------------------------*/

static int halt_bulk_in_endpoint(struct fsg_dev *fsg)
{
	int	rc;

	rc = fsg_set_halt(fsg, fsg->bulk_in);
	if (rc == -EAGAIN)
		VDBG(fsg, "delayed bulk-in endpoint halt\n");
	while (rc != 0) {
		if (rc != -EAGAIN) {
			WARNING(fsg, "usb_ep_set_halt -> %d\n", rc);
			rc = 0;
			break;
		}

		/* Wait for a short time and then try again */
		if (msleep_interruptible(100) != 0)
			return -EINTR;
		rc = usb_ep_set_halt(fsg->bulk_in);
	}
	return rc;
}

static int wedge_bulk_in_endpoint(struct fsg_dev *fsg)
{
	int	rc;

	DBG(fsg, "bulk-in set wedge\n");
	rc = usb_ep_set_wedge(fsg->bulk_in);
	if (rc == -EAGAIN)
		VDBG(fsg, "delayed bulk-in endpoint wedge\n");
	while (rc != 0) {
		if (rc != -EAGAIN) {
			WARNING(fsg, "usb_ep_set_wedge -> %d\n", rc);
			rc = 0;
			break;
		}

		/* Wait for a short time and then try again */
		if (msleep_interruptible(100) != 0)
			return -EINTR;
		rc = usb_ep_set_wedge(fsg->bulk_in);
	}
	return rc;
}

static int throw_away_data(struct fsg_common *common)
{
	struct fsg_buffhd	*bh;
	u32			amount;
	int			rc;

	for (bh = common->next_buffhd_to_drain;
	     bh->state != BUF_STATE_EMPTY || common->usb_amount_left > 0;
	     bh = common->next_buffhd_to_drain) {

		/* Throw away the data in a filled buffer */
		if (bh->state == BUF_STATE_FULL) {
			smp_rmb();
			bh->state = BUF_STATE_EMPTY;
			common->next_buffhd_to_drain = bh->next;

			/* A short packet or an error ends everything */
			if (bh->outreq->actual < bh->bulk_out_intended_length ||
			    bh->outreq->status != 0) {
				raise_exception(common,
						FSG_STATE_ABORT_BULK_OUT);
				return -EINTR;
			}
			continue;
		}

		/* Try to submit another request if we need one */
		bh = common->next_buffhd_to_fill;
		if (bh->state == BUF_STATE_EMPTY
		 && common->usb_amount_left > 0) {
			amount = min(common->usb_amount_left, FSG_BUFLEN);

			/*
			 * Except at the end of the transfer, amount will be
			 * equal to the buffer size, which is divisible by
			 * the bulk-out maxpacket size.
			 */
			set_bulk_out_req_length(common, bh, amount);
			if (!start_out_transfer(common, bh))
				/* Dunno what to do if common->fsg is NULL */
				return -EIO;
			common->next_buffhd_to_fill = bh->next;
			common->usb_amount_left -= amount;
			continue;
		}

		/* Otherwise wait for something to happen */
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}
	return 0;
}

static int finish_reply(struct fsg_common *common)
{
	struct fsg_buffhd	*bh = common->next_buffhd_to_fill;
	int			rc = 0;

	switch (common->data_dir) {
	case DATA_DIR_NONE:
		break;			/* Nothing to send */

	/*
	 * If we don't know whether the host wants to read or write,
	 * this must be CB or CBI with an unknown command.  We mustn't
	 * try to send or receive any data.  So stall both bulk pipes
	 * if we can and wait for a reset.
	 */
	case DATA_DIR_UNKNOWN:
		if (!common->can_stall) {
			/* Nothing */
		} else if (fsg_is_set(common)) {
			fsg_set_halt(common->fsg, common->fsg->bulk_out);
			rc = halt_bulk_in_endpoint(common->fsg);
		} else {
			/* Don't know what to do if common->fsg is NULL */
			rc = -EIO;
		}
		break;

	/* All but the last buffer of data must have already been sent */
	case DATA_DIR_TO_HOST:
		if (common->data_size == 0) {
			/* Nothing to send */

		/* Don't know what to do if common->fsg is NULL */
		} else if (!fsg_is_set(common)) {
			rc = -EIO;

		/* If there's no residue, simply send the last buffer */
		} else if (common->residue == 0) {
			bh->inreq->zero = 0;
			if (!start_in_transfer(common, bh))
				return -EIO;
			common->next_buffhd_to_fill = bh->next;

		/*
		 * For Bulk-only, mark the end of the data with a short
		 * packet.  If we are allowed to stall, halt the bulk-in
		 * endpoint.  (Note: This violates the Bulk-Only Transport
		 * specification, which requires us to pad the data if we
		 * don't halt the endpoint.  Presumably nobody will mind.)
		 */
		} else {
			bh->inreq->zero = 1;
			if (!start_in_transfer(common, bh))
				rc = -EIO;
			common->next_buffhd_to_fill = bh->next;
			if (common->can_stall)
				rc = halt_bulk_in_endpoint(common->fsg);
		}
		break;

	/*
	 * We have processed all we want from the data the host has sent.
	 * There may still be outstanding bulk-out requests.
	 */
	case DATA_DIR_FROM_HOST:
		if (common->residue == 0) {
			/* Nothing to receive */

		/* Did the host stop sending unexpectedly early? */
		} else if (common->short_packet_received) {
			raise_exception(common, FSG_STATE_ABORT_BULK_OUT);
			rc = -EINTR;

		/*
		 * We haven't processed all the incoming data.  Even though
		 * we may be allowed to stall, doing so would cause a race.
		 * The controller may already have ACK'ed all the remaining
		 * bulk-out packets, in which case the host wouldn't see a
		 * STALL.  Not realizing the endpoint was halted, it wouldn't
		 * clear the halt -- leading to problems later on.
		 */
#if 0
		} else if (common->can_stall) {
			if (fsg_is_set(common))
				fsg_set_halt(common->fsg,
					     common->fsg->bulk_out);
			raise_exception(common, FSG_STATE_ABORT_BULK_OUT);
			rc = -EINTR;
#endif

		/*
		 * We can't stall.  Read in the excess data and throw it
		 * all away.
		 */
		} else {
			rc = throw_away_data(common);
		}
		break;
	}
	return rc;
}

static int send_status(struct fsg_common *common)
{
	struct fsg_lun		*curlun = common->curlun;
	struct fsg_buffhd	*bh;
	struct bulk_cs_wrap	*csw;
	int			rc;
	u8			status = US_BULK_STAT_OK;
	u32			sd, sdinfo = 0;

	/* Wait for the next buffer to become available */
	bh = common->next_buffhd_to_fill;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}

	if (curlun) {
		sd = curlun->sense_data;
		sdinfo = curlun->sense_data_info;
	} else if (common->bad_lun_okay)
		sd = SS_NO_SENSE;
	else
		sd = SS_LOGICAL_UNIT_NOT_SUPPORTED;

	if (common->phase_error) {
		DBG(common, "sending phase-error status\n");
		status = US_BULK_STAT_PHASE;
		sd = SS_INVALID_COMMAND;
	} else if (sd != SS_NO_SENSE) {
		DBG(common, "sending command-failure status\n");
		status = US_BULK_STAT_FAIL;
		VDBG(common, "  sense data: SK x%02x, ASC x%02x, ASCQ x%02x;"
				"  info x%x\n",
				SK(sd), ASC(sd), ASCQ(sd), sdinfo);
	}

	/* Store and send the Bulk-only CSW */
	csw = (void *)bh->buf;

	csw->Signature = cpu_to_le32(US_BULK_CS_SIGN);
	csw->Tag = common->tag;
	csw->Residue = cpu_to_le32(common->residue);
	csw->Status = status;

	bh->inreq->length = US_BULK_CS_WRAP_LEN;
	bh->inreq->zero = 0;
	if (!start_in_transfer(common, bh))
		/* Don't know what to do if common->fsg is NULL */
		return -EIO;

	common->next_buffhd_to_fill = bh->next;
	return 0;
}


/*-------------------------------------------------------------------------*/

/*
 * Check whether the command is properly formed and whether its data size
 * and direction agree with the values we already have.
 */
static int check_command(struct fsg_common *common, int cmnd_size,
			 enum data_direction data_dir, unsigned int mask,
			 int needs_medium, const char *name)
{
	int			i;
	int			lun = common->cmnd[1] >> 5;
	static const char	dirletter[4] = {'u', 'o', 'i', 'n'};
	char			hdlen[20];
	struct fsg_lun		*curlun;

	hdlen[0] = 0;
	if (common->data_dir != DATA_DIR_UNKNOWN)
		sprintf(hdlen, ", H%c=%u", dirletter[(int) common->data_dir],
			common->data_size);
	VDBG(common, "SCSI command: %s;  Dc=%d, D%c=%u;  Hc=%d%s\n",
	     name, cmnd_size, dirletter[(int) data_dir],
	     common->data_size_from_cmnd, common->cmnd_size, hdlen);

	/*
	 * We can't reply at all until we know the correct data direction
	 * and size.
	 */
	if (common->data_size_from_cmnd == 0)
		data_dir = DATA_DIR_NONE;
	if (common->data_size < common->data_size_from_cmnd) {
		/*
		 * Host data size < Device data size is a phase error.
		 * Carry out the command, but only transfer as much as
		 * we are allowed.
		 */
		common->data_size_from_cmnd = common->data_size;
		common->phase_error = 1;
	}
	common->residue = common->data_size;
	common->usb_amount_left = common->data_size;

	/* Conflicting data directions is a phase error */
	if (common->data_dir != data_dir && common->data_size_from_cmnd > 0) {
		common->phase_error = 1;
		return -EINVAL;
	}

	/* Verify the length of the command itself */
	if (cmnd_size != common->cmnd_size) {

		/*
		 * Special case workaround: There are plenty of buggy SCSI
		 * implementations. Many have issues with cbw->Length
		 * field passing a wrong command size. For those cases we
		 * always try to work around the problem by using the length
		 * sent by the host side provided it is at least as large
		 * as the correct command length.
		 * Examples of such cases would be MS-Windows, which issues
		 * REQUEST SENSE with cbw->Length == 12 where it should
		 * be 6, and xbox360 issuing INQUIRY, TEST UNIT READY and
		 * REQUEST SENSE with cbw->Length == 10 where it should
		 * be 6 as well.
		 */
		if (cmnd_size <= common->cmnd_size) {
			DBG(common, "%s is buggy! Expected length %d "
			    "but we got %d\n", name,
			    cmnd_size, common->cmnd_size);
			cmnd_size = common->cmnd_size;
		} else {
			common->phase_error = 1;
			return -EINVAL;
		}
	}

	/* Check that the LUN values are consistent */
	if (common->lun != lun)
		DBG(common, "using LUN %d from CBW, not LUN %d from CDB\n",
		    common->lun, lun);

	/* Check the LUN */
	curlun = common->curlun;
	if (curlun) {
		if (common->cmnd[0] != REQUEST_SENSE) {
			curlun->sense_data = SS_NO_SENSE;
			curlun->sense_data_info = 0;
			curlun->info_valid = 0;
		}
	} else {
		common->bad_lun_okay = 0;

		/*
		 * INQUIRY and REQUEST SENSE commands are explicitly allowed
		 * to use unsupported LUNs; all others may not.
		 */
		if (common->cmnd[0] != INQUIRY &&
		    common->cmnd[0] != REQUEST_SENSE) {
			DBG(common, "unsupported LUN %d\n", common->lun);
			return -EINVAL;
		}
	}

	/*
	 * If a unit attention condition exists, only INQUIRY and
	 * REQUEST SENSE commands are allowed; anything else must fail.
	 */
	if (curlun && curlun->unit_attention_data != SS_NO_SENSE &&
	    common->cmnd[0] != INQUIRY &&
	    common->cmnd[0] != REQUEST_SENSE) {
		curlun->sense_data = curlun->unit_attention_data;
		curlun->unit_attention_data = SS_NO_SENSE;
		return -EINVAL;
	}

	/* Check that only command bytes listed in the mask are non-zero */
	common->cmnd[1] &= 0x1f;			/* Mask away the LUN */
	for (i = 1; i < cmnd_size; ++i) {
		if (common->cmnd[i] && !(mask & (1 << i))) {
			if (curlun)
				curlun->sense_data = SS_INVALID_FIELD_IN_CDB;
			return -EINVAL;
		}
	}

	/* If the medium isn't mounted and the command needs to access
	 * it, return an error. */
	if (curlun && !fsg_lun_is_open(curlun) && needs_medium) {
		curlun->sense_data = SS_MEDIUM_NOT_PRESENT;
		return -EINVAL;
	}

	return 0;
}

/* wrapper of check_command for data size in blocks handling */
static int check_command_size_in_blocks(struct fsg_common *common,
		int cmnd_size, enum data_direction data_dir,
		unsigned int mask, int needs_medium, const char *name)
{
	if (common->curlun)
		common->data_size_from_cmnd <<= common->curlun->blkbits;
	return check_command(common, cmnd_size, data_dir,
			mask, needs_medium, name);
}

static int do_scsi_command(struct fsg_common *common)
{
	struct fsg_buffhd	*bh;
	int			rc;
	int			reply = -EINVAL;
	int			i;
	static char		unknown[16];

	dump_cdb(common);

//daveti
	/* Check the TPM ATT state of the trusted dev */
	if ((fsg_trusted_dev_ctrl.tpm_att_state != FSG_TRUSTED_DEV_TPM_ATT_SUCCESS) && 
		(fsg_trusted_dev_ctrl.tpm_att_state != FSG_TRUSTED_DEV_TPM_ATT_NONE)) {
		printk(KERN_ERR "daveti: SCSI cmd is stopped because of TPM ATT state [%d]\n",
			fsg_trusted_dev_ctrl.tpm_att_state);
		return -EINVAL;
	}
	/* Debug */
	if (fsg_trusted_dev_debug)
		pr_info("provusb: SCSI cmd [%d]\n", common->cmnd[0]);

	/* Wait for the next buffer to become available for data or status */
	bh = common->next_buffhd_to_fill;
	common->next_buffhd_to_drain = bh;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}
	common->phase_error = 0;
	common->short_packet_received = 0;

	down_read(&common->filesem);	/* We're using the backing file */
	switch (common->cmnd[0]) {

	case INQUIRY:
		common->data_size_from_cmnd = common->cmnd[4];
		reply = check_command(common, 6, DATA_DIR_TO_HOST,
				      (1<<4), 0,
				      "INQUIRY");
		if (reply == 0)
			reply = do_inquiry(common, bh);
		break;

	case MODE_SELECT:
		common->data_size_from_cmnd = common->cmnd[4];
		reply = check_command(common, 6, DATA_DIR_FROM_HOST,
				      (1<<1) | (1<<4), 0,
				      "MODE SELECT(6)");
		if (reply == 0)
			reply = do_mode_select(common, bh);
		break;

	case MODE_SELECT_10:
		common->data_size_from_cmnd =
			get_unaligned_be16(&common->cmnd[7]);
		reply = check_command(common, 10, DATA_DIR_FROM_HOST,
				      (1<<1) | (3<<7), 0,
				      "MODE SELECT(10)");
		if (reply == 0)
			reply = do_mode_select(common, bh);
		break;

	case MODE_SENSE:
		common->data_size_from_cmnd = common->cmnd[4];
		reply = check_command(common, 6, DATA_DIR_TO_HOST,
				      (1<<1) | (1<<2) | (1<<4), 0,
				      "MODE SENSE(6)");
		if (reply == 0)
			reply = do_mode_sense(common, bh);
		break;

	case MODE_SENSE_10:
		common->data_size_from_cmnd =
			get_unaligned_be16(&common->cmnd[7]);
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (1<<1) | (1<<2) | (3<<7), 0,
				      "MODE SENSE(10)");
		if (reply == 0)
			reply = do_mode_sense(common, bh);
		break;

	case ALLOW_MEDIUM_REMOVAL:
		common->data_size_from_cmnd = 0;
		reply = check_command(common, 6, DATA_DIR_NONE,
				      (1<<4), 0,
				      "PREVENT-ALLOW MEDIUM REMOVAL");
		if (reply == 0)
			reply = do_prevent_allow(common);
		break;

	case READ_6:
		i = common->cmnd[4];
		common->data_size_from_cmnd = (i == 0) ? 256 : i;
		reply = check_command_size_in_blocks(common, 6,
				      DATA_DIR_TO_HOST,
				      (7<<1) | (1<<4), 1,
				      "READ(6)");
		if (reply == 0)
			reply = do_read(common);
		break;

	case READ_10:
		common->data_size_from_cmnd =
				get_unaligned_be16(&common->cmnd[7]);
		reply = check_command_size_in_blocks(common, 10,
				      DATA_DIR_TO_HOST,
				      (1<<1) | (0xf<<2) | (3<<7), 1,
				      "READ(10)");
		if (reply == 0)
			reply = do_read(common);
		break;

	case READ_12:
		common->data_size_from_cmnd =
				get_unaligned_be32(&common->cmnd[6]);
		reply = check_command_size_in_blocks(common, 12,
				      DATA_DIR_TO_HOST,
				      (1<<1) | (0xf<<2) | (0xf<<6), 1,
				      "READ(12)");
		if (reply == 0)
			reply = do_read(common);
		break;

	case READ_CAPACITY:
		common->data_size_from_cmnd = 8;
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (0xf<<2) | (1<<8), 1,
				      "READ CAPACITY");
		if (reply == 0)
			reply = do_read_capacity(common, bh);
		break;

	case READ_HEADER:
		if (!common->curlun || !common->curlun->cdrom)
			goto unknown_cmnd;
		common->data_size_from_cmnd =
			get_unaligned_be16(&common->cmnd[7]);
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (3<<7) | (0x1f<<1), 1,
				      "READ HEADER");
		if (reply == 0)
			reply = do_read_header(common, bh);
		break;

	case READ_TOC:
		if (!common->curlun || !common->curlun->cdrom)
			goto unknown_cmnd;
		common->data_size_from_cmnd =
			get_unaligned_be16(&common->cmnd[7]);
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (7<<6) | (1<<1), 1,
				      "READ TOC");
		if (reply == 0)
			reply = do_read_toc(common, bh);
		break;

	case READ_FORMAT_CAPACITIES:
		common->data_size_from_cmnd =
			get_unaligned_be16(&common->cmnd[7]);
		reply = check_command(common, 10, DATA_DIR_TO_HOST,
				      (3<<7), 1,
				      "READ FORMAT CAPACITIES");
		if (reply == 0)
			reply = do_read_format_capacities(common, bh);
		break;

	case REQUEST_SENSE:
		common->data_size_from_cmnd = common->cmnd[4];
		reply = check_command(common, 6, DATA_DIR_TO_HOST,
				      (1<<4), 0,
				      "REQUEST SENSE");
		if (reply == 0)
			reply = do_request_sense(common, bh);
		break;

	case START_STOP:
		common->data_size_from_cmnd = 0;
		reply = check_command(common, 6, DATA_DIR_NONE,
				      (1<<1) | (1<<4), 0,
				      "START-STOP UNIT");
		if (reply == 0)
			reply = do_start_stop(common);
		break;

	case SYNCHRONIZE_CACHE:
		common->data_size_from_cmnd = 0;
		reply = check_command(common, 10, DATA_DIR_NONE,
				      (0xf<<2) | (3<<7), 1,
				      "SYNCHRONIZE CACHE");
		if (reply == 0)
			reply = do_synchronize_cache(common);
		break;

	case TEST_UNIT_READY:
		common->data_size_from_cmnd = 0;
		reply = check_command(common, 6, DATA_DIR_NONE,
				0, 1,
				"TEST UNIT READY");
		break;

	/*
	 * Although optional, this command is used by MS-Windows.  We
	 * support a minimal version: BytChk must be 0.
	 */
	case VERIFY:
		common->data_size_from_cmnd = 0;
		reply = check_command(common, 10, DATA_DIR_NONE,
				      (1<<1) | (0xf<<2) | (3<<7), 1,
				      "VERIFY");
		if (reply == 0)
			reply = do_verify(common);
		break;

	case WRITE_6:
		i = common->cmnd[4];
		common->data_size_from_cmnd = (i == 0) ? 256 : i;
		reply = check_command_size_in_blocks(common, 6,
				      DATA_DIR_FROM_HOST,
				      (7<<1) | (1<<4), 1,
				      "WRITE(6)");
		if (reply == 0)
			reply = do_write(common);
		break;

	case WRITE_10:
		common->data_size_from_cmnd =
				get_unaligned_be16(&common->cmnd[7]);
		reply = check_command_size_in_blocks(common, 10,
				      DATA_DIR_FROM_HOST,
				      (1<<1) | (0xf<<2) | (3<<7), 1,
				      "WRITE(10)");
		if (reply == 0)
			reply = do_write(common);
		break;

	case WRITE_12:
		common->data_size_from_cmnd =
				get_unaligned_be32(&common->cmnd[6]);
		reply = check_command_size_in_blocks(common, 12,
				      DATA_DIR_FROM_HOST,
				      (1<<1) | (0xf<<2) | (0xf<<6), 1,
				      "WRITE(12)");
		if (reply == 0)
			reply = do_write(common);
		break;

	/*
	 * Some mandatory commands that we recognize but don't implement.
	 * They don't mean much in this setting.  It's left as an exercise
	 * for anyone interested to implement RESERVE and RELEASE in terms
	 * of Posix locks.
	 */
	case FORMAT_UNIT:
	case RELEASE:
	case RESERVE:
	case SEND_DIAGNOSTIC:
		/* Fall through */

	default:
unknown_cmnd:
		common->data_size_from_cmnd = 0;
		sprintf(unknown, "Unknown x%02x", common->cmnd[0]);
		reply = check_command(common, common->cmnd_size,
				      DATA_DIR_UNKNOWN, ~0, 0, unknown);
		if (reply == 0) {
			common->curlun->sense_data = SS_INVALID_COMMAND;
			reply = -EINVAL;
		}
		break;
	}
	up_read(&common->filesem);

	if (reply == -EINTR || signal_pending(current))
		return -EINTR;

	/* Set up the single reply buffer for finish_reply() */
	if (reply == -EINVAL)
		reply = 0;		/* Error reply length */
	if (reply >= 0 && common->data_dir == DATA_DIR_TO_HOST) {
		reply = min((u32)reply, common->data_size_from_cmnd);
		bh->inreq->length = reply;
		bh->state = BUF_STATE_FULL;
		common->residue -= reply;
	}				/* Otherwise it's already set */

	return 0;
}


/*-------------------------------------------------------------------------*/

static int received_cbw(struct fsg_dev *fsg, struct fsg_buffhd *bh)
{
	struct usb_request	*req = bh->outreq;
	struct bulk_cb_wrap	*cbw = req->buf;
	struct fsg_common	*common = fsg->common;

	/* Was this a real packet?  Should it be ignored? */
	if (req->status || test_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags))
		return -EINVAL;

	/* Is the CBW valid? */
	if (req->actual != US_BULK_CB_WRAP_LEN ||
			cbw->Signature != cpu_to_le32(
				US_BULK_CB_SIGN)) {
		DBG(fsg, "invalid CBW: len %u sig 0x%x\n",
				req->actual,
				le32_to_cpu(cbw->Signature));

		/*
		 * The Bulk-only spec says we MUST stall the IN endpoint
		 * (6.6.1), so it's unavoidable.  It also says we must
		 * retain this state until the next reset, but there's
		 * no way to tell the controller driver it should ignore
		 * Clear-Feature(HALT) requests.
		 *
		 * We aren't required to halt the OUT endpoint; instead
		 * we can simply accept and discard any data received
		 * until the next reset.
		 */
		wedge_bulk_in_endpoint(fsg);
		set_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags);
		return -EINVAL;
	}

	/* Is the CBW meaningful? */
	if (cbw->Lun >= FSG_MAX_LUNS || cbw->Flags & ~US_BULK_FLAG_IN ||
			cbw->Length <= 0 || cbw->Length > MAX_COMMAND_SIZE) {
		DBG(fsg, "non-meaningful CBW: lun = %u, flags = 0x%x, "
				"cmdlen %u\n",
				cbw->Lun, cbw->Flags, cbw->Length);

		/*
		 * We can do anything we want here, so let's stall the
		 * bulk pipes if we are allowed to.
		 */
		if (common->can_stall) {
			fsg_set_halt(fsg, fsg->bulk_out);
			halt_bulk_in_endpoint(fsg);
		}
		return -EINVAL;
	}

	/* Save the command for later */
	common->cmnd_size = cbw->Length;
	memcpy(common->cmnd, cbw->CDB, common->cmnd_size);
	if (cbw->Flags & US_BULK_FLAG_IN)
		common->data_dir = DATA_DIR_TO_HOST;
	else
		common->data_dir = DATA_DIR_FROM_HOST;
	common->data_size = le32_to_cpu(cbw->DataTransferLength);
	if (common->data_size == 0)
		common->data_dir = DATA_DIR_NONE;
	common->lun = cbw->Lun;
	if (common->lun >= 0 && common->lun < common->nluns)
		common->curlun = &common->luns[common->lun];
	else
		common->curlun = NULL;
	common->tag = cbw->Tag;
	return 0;
}

static int get_next_command(struct fsg_common *common)
{
	struct fsg_buffhd	*bh;
	int			rc = 0;

	/* Wait for the next buffer to become available */
	bh = common->next_buffhd_to_fill;
	while (bh->state != BUF_STATE_EMPTY) {
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}

	/* Queue a request to read a Bulk-only CBW */
	set_bulk_out_req_length(common, bh, US_BULK_CB_WRAP_LEN);
	if (!start_out_transfer(common, bh))
		/* Don't know what to do if common->fsg is NULL */
		return -EIO;

	/*
	 * We will drain the buffer in software, which means we
	 * can reuse it for the next filling.  No need to advance
	 * next_buffhd_to_fill.
	 */

	/* Wait for the CBW to arrive */
	while (bh->state != BUF_STATE_FULL) {
		rc = sleep_thread(common);
		if (rc)
			return rc;
	}
	smp_rmb();
	rc = fsg_is_set(common) ? received_cbw(common->fsg, bh) : -EIO;
	bh->state = BUF_STATE_EMPTY;

	return rc;
}


/*-------------------------------------------------------------------------*/

static int alloc_request(struct fsg_common *common, struct usb_ep *ep,
		struct usb_request **preq)
{
	*preq = usb_ep_alloc_request(ep, GFP_ATOMIC);
	if (*preq)
		return 0;
	ERROR(common, "can't allocate request for %s\n", ep->name);
	return -ENOMEM;
}

/* Reset interface setting and re-init endpoint state (toggle etc). */
static int do_set_interface(struct fsg_common *common, struct fsg_dev *new_fsg)
{
	struct fsg_dev *fsg;
	int i, rc = 0;

	if (common->running)
		DBG(common, "reset interface\n");

reset:
	/* Deallocate the requests */
	if (common->fsg) {
		fsg = common->fsg;

		for (i = 0; i < fsg_num_buffers; ++i) {
			struct fsg_buffhd *bh = &common->buffhds[i];

			if (bh->inreq) {
				usb_ep_free_request(fsg->bulk_in, bh->inreq);
				bh->inreq = NULL;
			}
			if (bh->outreq) {
				usb_ep_free_request(fsg->bulk_out, bh->outreq);
				bh->outreq = NULL;
			}
		}

		/* Disable the endpoints */
		if (fsg->bulk_in_enabled) {
			usb_ep_disable(fsg->bulk_in);
			fsg->bulk_in_enabled = 0;
		}
		if (fsg->bulk_out_enabled) {
			usb_ep_disable(fsg->bulk_out);
			fsg->bulk_out_enabled = 0;
		}

		common->fsg = NULL;
		wake_up(&common->fsg_wait);
	}

	common->running = 0;
	if (!new_fsg || rc)
		return rc;

	common->fsg = new_fsg;
	fsg = common->fsg;

	/* Enable the endpoints */
	rc = config_ep_by_speed(common->gadget, &(fsg->function), fsg->bulk_in);
	if (rc)
		goto reset;
	rc = usb_ep_enable(fsg->bulk_in);
	if (rc)
		goto reset;
	fsg->bulk_in->driver_data = common;
	fsg->bulk_in_enabled = 1;

	rc = config_ep_by_speed(common->gadget, &(fsg->function),
				fsg->bulk_out);
	if (rc)
		goto reset;
	rc = usb_ep_enable(fsg->bulk_out);
	if (rc)
		goto reset;
	fsg->bulk_out->driver_data = common;
	fsg->bulk_out_enabled = 1;
	common->bulk_out_maxpacket = usb_endpoint_maxp(fsg->bulk_out->desc);
	clear_bit(IGNORE_BULK_OUT, &fsg->atomic_bitflags);

	/* Allocate the requests */
	for (i = 0; i < fsg_num_buffers; ++i) {
		struct fsg_buffhd	*bh = &common->buffhds[i];

		rc = alloc_request(common, fsg->bulk_in, &bh->inreq);
		if (rc)
			goto reset;
		rc = alloc_request(common, fsg->bulk_out, &bh->outreq);
		if (rc)
			goto reset;
		bh->inreq->buf = bh->outreq->buf = bh->buf;
		bh->inreq->context = bh->outreq->context = bh;
		bh->inreq->complete = bulk_in_complete;
		bh->outreq->complete = bulk_out_complete;
	}

	common->running = 1;
	for (i = 0; i < common->nluns; ++i)
		common->luns[i].unit_attention_data = SS_RESET_OCCURRED;
	return rc;
}


/****************************** ALT CONFIGS ******************************/

static int fsg_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct fsg_dev *fsg = fsg_from_func(f);
	fsg->common->new_fsg = fsg;
	raise_exception(fsg->common, FSG_STATE_CONFIG_CHANGE);
	return USB_GADGET_DELAYED_STATUS;
}

static void fsg_disable(struct usb_function *f)
{
	struct fsg_dev *fsg = fsg_from_func(f);
	fsg->common->new_fsg = NULL;
	raise_exception(fsg->common, FSG_STATE_CONFIG_CHANGE);
}


/*-------------------------------------------------------------------------*/

static void handle_exception(struct fsg_common *common)
{
	siginfo_t		info;
	int			i;
	struct fsg_buffhd	*bh;
	enum fsg_state		old_state;
	struct fsg_lun		*curlun;
	unsigned int		exception_req_tag;

	/*
	 * Clear the existing signals.  Anything but SIGUSR1 is converted
	 * into a high-priority EXIT exception.
	 */
	for (;;) {
		int sig =
			dequeue_signal_lock(current, &current->blocked, &info);
		if (!sig)
			break;
		if (sig != SIGUSR1) {
			if (common->state < FSG_STATE_EXIT)
				DBG(common, "Main thread exiting on signal\n");
			raise_exception(common, FSG_STATE_EXIT);
		}
	}

	/* Cancel all the pending transfers */
	if (likely(common->fsg)) {
		for (i = 0; i < fsg_num_buffers; ++i) {
			bh = &common->buffhds[i];
			if (bh->inreq_busy)
				usb_ep_dequeue(common->fsg->bulk_in, bh->inreq);
			if (bh->outreq_busy)
				usb_ep_dequeue(common->fsg->bulk_out,
					       bh->outreq);
		}

		/* Wait until everything is idle */
		for (;;) {
			int num_active = 0;
			for (i = 0; i < fsg_num_buffers; ++i) {
				bh = &common->buffhds[i];
				num_active += bh->inreq_busy + bh->outreq_busy;
			}
			if (num_active == 0)
				break;
			if (sleep_thread(common))
				return;
		}

		/* Clear out the controller's fifos */
		if (common->fsg->bulk_in_enabled)
			usb_ep_fifo_flush(common->fsg->bulk_in);
		if (common->fsg->bulk_out_enabled)
			usb_ep_fifo_flush(common->fsg->bulk_out);
	}

	/*
	 * Reset the I/O buffer states and pointers, the SCSI
	 * state, and the exception.  Then invoke the handler.
	 */
	spin_lock_irq(&common->lock);

	for (i = 0; i < fsg_num_buffers; ++i) {
		bh = &common->buffhds[i];
		bh->state = BUF_STATE_EMPTY;
	}
	common->next_buffhd_to_fill = &common->buffhds[0];
	common->next_buffhd_to_drain = &common->buffhds[0];
	exception_req_tag = common->exception_req_tag;
	old_state = common->state;

	if (old_state == FSG_STATE_ABORT_BULK_OUT)
		common->state = FSG_STATE_STATUS_PHASE;
	else {
		for (i = 0; i < common->nluns; ++i) {
			curlun = &common->luns[i];
			curlun->prevent_medium_removal = 0;
			curlun->sense_data = SS_NO_SENSE;
			curlun->unit_attention_data = SS_NO_SENSE;
			curlun->sense_data_info = 0;
			curlun->info_valid = 0;
		}
		common->state = FSG_STATE_IDLE;
	}
	spin_unlock_irq(&common->lock);

	/* Carry out any extra actions required for the exception */
	switch (old_state) {
	case FSG_STATE_ABORT_BULK_OUT:
		send_status(common);
		spin_lock_irq(&common->lock);
		if (common->state == FSG_STATE_STATUS_PHASE)
			common->state = FSG_STATE_IDLE;
		spin_unlock_irq(&common->lock);
		break;

	case FSG_STATE_RESET:
		/*
		 * In case we were forced against our will to halt a
		 * bulk endpoint, clear the halt now.  (The SuperH UDC
		 * requires this.)
		 */
		if (!fsg_is_set(common))
			break;
		if (test_and_clear_bit(IGNORE_BULK_OUT,
				       &common->fsg->atomic_bitflags))
			usb_ep_clear_halt(common->fsg->bulk_in);

		if (common->ep0_req_tag == exception_req_tag)
			ep0_queue(common);	/* Complete the status stage */

		/*
		 * Technically this should go here, but it would only be
		 * a waste of time.  Ditto for the INTERFACE_CHANGE and
		 * CONFIG_CHANGE cases.
		 */
		/* for (i = 0; i < common->nluns; ++i) */
		/*	common->luns[i].unit_attention_data = */
		/*		SS_RESET_OCCURRED;  */
		break;

	case FSG_STATE_CONFIG_CHANGE:
		do_set_interface(common, common->new_fsg);
		if (common->new_fsg)
			usb_composite_setup_continue(common->cdev);
		break;

	case FSG_STATE_EXIT:
	case FSG_STATE_TERMINATED:
		do_set_interface(common, NULL);		/* Free resources */
		spin_lock_irq(&common->lock);
		common->state = FSG_STATE_TERMINATED;	/* Stop the thread */
		spin_unlock_irq(&common->lock);
		break;

	case FSG_STATE_INTERFACE_CHANGE:
	case FSG_STATE_DISCONNECT:
	case FSG_STATE_COMMAND_PHASE:
	case FSG_STATE_DATA_PHASE:
	case FSG_STATE_STATUS_PHASE:
	case FSG_STATE_IDLE:
		break;
	}
}


/*-------------------------------------------------------------------------*/

static int fsg_main_thread(void *common_)
{
	struct fsg_common	*common = common_;

	/*
	 * Allow the thread to be killed by a signal, but set the signal mask
	 * to block everything but INT, TERM, KILL, and USR1.
	 */
	allow_signal(SIGINT);
	allow_signal(SIGTERM);
	allow_signal(SIGKILL);
	allow_signal(SIGUSR1);

	/* Allow the thread to be frozen */
	set_freezable();

	/*
	 * Arrange for userspace references to be interpreted as kernel
	 * pointers.  That way we can pass a kernel pointer to a routine
	 * that expects a __user pointer and it will work okay.
	 */
	set_fs(get_ds());

	/* The main loop */
	while (common->state != FSG_STATE_TERMINATED) {
		if (exception_in_progress(common) || signal_pending(current)) {
			handle_exception(common);
			continue;
		}

		if (!common->running) {
			sleep_thread(common);
			continue;
		}

//daveti: update the fsg_trusted_dev state
//NOTE: move it into the EP0 handler to gaurantee the update
//happens only once in a timely way!
//Jan 23, 2015
//daveti
		//fsg_trusted_dev_update_state(&fsg_trusted_dev_ctrl);

		if (get_next_command(common))
			continue;

		spin_lock_irq(&common->lock);
		if (!exception_in_progress(common))
			common->state = FSG_STATE_DATA_PHASE;
		spin_unlock_irq(&common->lock);

		if (do_scsi_command(common) || finish_reply(common))
			continue;

		spin_lock_irq(&common->lock);
		if (!exception_in_progress(common))
			common->state = FSG_STATE_STATUS_PHASE;
		spin_unlock_irq(&common->lock);

		if (send_status(common))
			continue;

		spin_lock_irq(&common->lock);
		if (!exception_in_progress(common))
			common->state = FSG_STATE_IDLE;
		spin_unlock_irq(&common->lock);
	}

	spin_lock_irq(&common->lock);
	common->thread_task = NULL;
	spin_unlock_irq(&common->lock);

	if (!common->ops || !common->ops->thread_exits
	 || common->ops->thread_exits(common) < 0) {
		struct fsg_lun *curlun = common->luns;
		unsigned i = common->nluns;

		down_write(&common->filesem);
		for (; i--; ++curlun) {
			if (!fsg_lun_is_open(curlun))
				continue;

			fsg_lun_close(curlun);
			curlun->unit_attention_data = SS_MEDIUM_NOT_PRESENT;
		}
		up_write(&common->filesem);
	}

	/* Let fsg_unbind() know the thread has exited */
	complete_and_exit(&common->thread_notifier, 0);
}


/*************************** DEVICE ATTRIBUTES ***************************/

/* Write permission is checked per LUN in store_*() functions. */
static DEVICE_ATTR(ro, 0644, fsg_show_ro, fsg_store_ro);
static DEVICE_ATTR(nofua, 0644, fsg_show_nofua, fsg_store_nofua);
static DEVICE_ATTR(file, 0644, fsg_show_file, fsg_store_file);


/****************************** FSG COMMON ******************************/

static void fsg_common_release(struct kref *ref);

static void fsg_lun_release(struct device *dev)
{
	/* Nothing needs to be done */
}

static inline void fsg_common_get(struct fsg_common *common)
{
	kref_get(&common->ref);
}

static inline void fsg_common_put(struct fsg_common *common)
{
	kref_put(&common->ref, fsg_common_release);
}

static struct fsg_common *fsg_common_init(struct fsg_common *common,
					  struct usb_composite_dev *cdev,
					  struct fsg_config *cfg)
{
	struct usb_gadget *gadget = cdev->gadget;
	struct fsg_buffhd *bh;
	struct fsg_lun *curlun;
	struct fsg_lun_config *lcfg;
	int nluns, i, rc;
	char *pathbuf;

/* daveti: debug */
printk(KERN_INFO "daveti: trusted-dev %s\n", __FUNCTION__);

	rc = fsg_num_buffers_validate();
	if (rc != 0)
		return ERR_PTR(rc);

	/* Find out how many LUNs there should be */
	nluns = cfg->nluns;
	if (nluns < 1 || nluns > FSG_MAX_LUNS) {
		dev_err(&gadget->dev, "invalid number of LUNs: %u\n", nluns);
		return ERR_PTR(-EINVAL);
	}

	/* Allocate? */
	if (!common) {
		common = kzalloc(sizeof *common, GFP_KERNEL);
		if (!common)
			return ERR_PTR(-ENOMEM);
		common->free_storage_on_release = 1;
	} else {
		memset(common, 0, sizeof *common);
		common->free_storage_on_release = 0;
	}

	common->buffhds = kcalloc(fsg_num_buffers,
				  sizeof *(common->buffhds), GFP_KERNEL);
	if (!common->buffhds) {
		if (common->free_storage_on_release)
			kfree(common);
		return ERR_PTR(-ENOMEM);
	}

	common->ops = cfg->ops;
	common->private_data = cfg->private_data;

	common->gadget = gadget;
	common->ep0 = gadget->ep0;
	common->ep0req = cdev->req;
	common->cdev = cdev;

	/* Maybe allocate device-global string IDs, and patch descriptors */
	if (fsg_strings[FSG_STRING_INTERFACE].id == 0) {
		rc = usb_string_id(cdev);
		if (unlikely(rc < 0))
			goto error_release;
		fsg_strings[FSG_STRING_INTERFACE].id = rc;
		fsg_intf_desc.iInterface = rc;
	}

	/*
	 * Create the LUNs, open their backing files, and register the
	 * LUN devices in sysfs.
	 */
	curlun = kcalloc(nluns, sizeof(*curlun), GFP_KERNEL);
	if (unlikely(!curlun)) {
		rc = -ENOMEM;
		goto error_release;
	}
	common->luns = curlun;

	init_rwsem(&common->filesem);

	for (i = 0, lcfg = cfg->luns; i < nluns; ++i, ++curlun, ++lcfg) {
		curlun->cdrom = !!lcfg->cdrom;
		curlun->ro = lcfg->cdrom || lcfg->ro;
		curlun->initially_ro = curlun->ro;
		curlun->removable = lcfg->removable;
		curlun->dev.release = fsg_lun_release;
		curlun->dev.parent = &gadget->dev;
		/* curlun->dev.driver = &fsg_driver.driver; XXX */
		dev_set_drvdata(&curlun->dev, &common->filesem);
		dev_set_name(&curlun->dev,
			     cfg->lun_name_format
			   ? cfg->lun_name_format
			   : "lun%d",
			     i);

		rc = device_register(&curlun->dev);
		if (rc) {
			INFO(common, "failed to register LUN%d: %d\n", i, rc);
			common->nluns = i;
			put_device(&curlun->dev);
			goto error_release;
		}

		rc = device_create_file(&curlun->dev, &dev_attr_ro);
		if (rc)
			goto error_luns;
		rc = device_create_file(&curlun->dev, &dev_attr_file);
		if (rc)
			goto error_luns;
		rc = device_create_file(&curlun->dev, &dev_attr_nofua);
		if (rc)
			goto error_luns;

		if (lcfg->filename) {
			rc = fsg_lun_open(curlun, lcfg->filename);
			if (rc)
				goto error_luns;
		} else if (!curlun->removable) {
			ERROR(common, "no file given for LUN%d\n", i);
			rc = -EINVAL;
			goto error_luns;
		}
	}
	common->nluns = nluns;

	/* Data buffers cyclic list */
	bh = common->buffhds;
	i = fsg_num_buffers;
	goto buffhds_first_it;
	do {
		bh->next = bh + 1;
		++bh;
buffhds_first_it:
		bh->buf = kmalloc(FSG_BUFLEN, GFP_KERNEL);
		if (unlikely(!bh->buf)) {
			rc = -ENOMEM;
			goto error_release;
		}
	} while (--i);
	bh->next = common->buffhds;

	/* Prepare inquiryString */
	if (cfg->release != 0xffff) {
		i = cfg->release;
	} else {
		i = usb_gadget_controller_number(gadget);
		if (i >= 0) {
			i = 0x0300 + i;
		} else {
			WARNING(common, "controller '%s' not recognized\n",
				gadget->name);
			i = 0x0399;
		}
	}
	snprintf(common->inquiry_string, sizeof common->inquiry_string,
		 "%-8s%-16s%04x", cfg->vendor_name ?: "Linux",
		 /* Assume product name dependent on the first LUN */
		 cfg->product_name ?: (common->luns->cdrom
				     ? "File-Stor Gadget"
				     : "File-CD Gadget"),
		 i);

	/*
	 * Some peripheral controllers are known not to be able to
	 * halt bulk endpoints correctly.  If one of them is present,
	 * disable stalls.
	 */
	common->can_stall = cfg->can_stall &&
		!(gadget_is_at91(common->gadget));

	spin_lock_init(&common->lock);
	kref_init(&common->ref);

	/* Tell the thread to start working */
	common->thread_task =
		kthread_create(fsg_main_thread, common,
			       cfg->thread_name ?: "file-storage");
	if (IS_ERR(common->thread_task)) {
		rc = PTR_ERR(common->thread_task);
		goto error_release;
	}
	init_completion(&common->thread_notifier);
	init_waitqueue_head(&common->fsg_wait);

	/* Information */
	INFO(common, FSG_DRIVER_DESC ", version: " FSG_DRIVER_VERSION "\n");
	INFO(common, "Number of LUNs=%d\n", common->nluns);

	pathbuf = kmalloc(PATH_MAX, GFP_KERNEL);
	for (i = 0, nluns = common->nluns, curlun = common->luns;
	     i < nluns;
	     ++curlun, ++i) {
		char *p = "(no medium)";
		if (fsg_lun_is_open(curlun)) {
			p = "(error)";
			if (pathbuf) {
				p = d_path(&curlun->filp->f_path,
					   pathbuf, PATH_MAX);
				if (IS_ERR(p))
					p = "(error)";
			}
		}
		LINFO(curlun, "LUN: %s%s%sfile: %s\n",
		      curlun->removable ? "removable " : "",
		      curlun->ro ? "read only " : "",
		      curlun->cdrom ? "CD-ROM " : "",
		      p);
	}
	kfree(pathbuf);

/* daveti: init the global trusted dev struc */
	fsg_trusted_dev_init(&fsg_trusted_dev_ctrl);
	printk(KERN_INFO "daveti: fsg_trusted_dev_ctrl is initialized\n");
/* daveti: load the config into the trusted dev */
	fsg_trusted_dev_load_config(&fsg_trusted_dev_ctrl, cfg);
	printk(KERN_INFO "daveti: fsg_trusted_dev_ctrl is loaded with config:\n"
		"key_file [%s], force_fail [%u], provusb_disable [%u] - dump the TIM DB:\n",
		fsg_trusted_dev_ctrl.key_file,
		fsg_trusted_dev_ctrl.force_fail,
		fsg_trusted_dev_ctrl.provusb_disable);
	/* dump the key file if available */
	if (fsg_trusted_dev_ctrl.key_file != NULL) {
		/* Dump the AIK pub key */
		print_hex_dump(KERN_INFO, "AIK: ", DUMP_PREFIX_OFFSET, 16, 1,
			(u8 *)fsg_trusted_dev_ctrl.tim_db.tim_aik_pub,
			FSG_TRUSTED_DEV_TPM_ATT_AIK_PUB_LEN, 0);
		/* Dump the validation data */
		print_hex_dump(KERN_INFO, "valid: ", DUMP_PREFIX_OFFSET, 16, 1,
			(u8 *)fsg_trusted_dev_ctrl.tim_db.valid,
			KRSA_TPM_QUOTE_VALID_LEN, 0);
		/* Dump the RSA n part */
		print_hex_dump(KERN_INFO, "RSA-n: ", DUMP_PREFIX_OFFSET, 16, 1,
			(u8 *)fsg_trusted_dev_ctrl.tim_db.rsa_n,
			KRSA_TPM_AIK_PUB_KEY_N_LEN, 0);
		/* Dump the RSA e part */
		print_hex_dump(KERN_INFO, "RSA-e: ", DUMP_PREFIX_OFFSET, 16, 1,
			(u8 *)fsg_trusted_dev_ctrl.tim_db.rsa_e,
			KRSA_TPM_AIK_PUB_KEY_E_LEN, 0);
	}
	/* ProvUSB - save the ctrl block */
	common->provusb_ctrl = &fsg_trusted_dev_ctrl;
	if (fsg_trusted_dev_debug)
		pr_info("provusb: fsg_trusted_dev_ctrl has been added [%p]\n",
			common->provusb_ctrl);

	DBG(common, "I/O thread pid: %d\n", task_pid_nr(common->thread_task));

	wake_up_process(common->thread_task);

	return common;

error_luns:
	common->nluns = i + 1;
error_release:
	common->state = FSG_STATE_TERMINATED;	/* The thread is dead */
	/* Call fsg_common_release() directly, ref might be not initialised. */
	fsg_common_release(&common->ref);
	return ERR_PTR(rc);
}

static void fsg_common_release(struct kref *ref)
{
	struct fsg_common *common = container_of(ref, struct fsg_common, ref);

	/* If the thread isn't already dead, tell it to exit now */
	if (common->state != FSG_STATE_TERMINATED) {
		raise_exception(common, FSG_STATE_EXIT);
		wait_for_completion(&common->thread_notifier);
	}

	if (likely(common->luns)) {
		struct fsg_lun *lun = common->luns;
		unsigned i = common->nluns;

		/* In error recovery common->nluns may be zero. */
		for (; i; --i, ++lun) {
			device_remove_file(&lun->dev, &dev_attr_nofua);
			device_remove_file(&lun->dev, &dev_attr_ro);
			device_remove_file(&lun->dev, &dev_attr_file);
			fsg_lun_close(lun);
			device_unregister(&lun->dev);
		}

		kfree(common->luns);
	}

	{
		struct fsg_buffhd *bh = common->buffhds;
		unsigned i = fsg_num_buffers;
		do {
			kfree(bh->buf);
		} while (++bh, --i);
	}

	kfree(common->buffhds);
	if (common->free_storage_on_release)
		kfree(common);

//daveti
	/* Release the ctrl block for the trusted dev */
	fsg_trusted_dev_free(&fsg_trusted_dev_ctrl);
}


/*-------------------------------------------------------------------------*/

static void fsg_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct fsg_dev		*fsg = fsg_from_func(f);
	struct fsg_common	*common = fsg->common;

	DBG(fsg, "unbind\n");
	if (fsg->common->fsg == fsg) {
		fsg->common->new_fsg = NULL;
		raise_exception(fsg->common, FSG_STATE_CONFIG_CHANGE);
		/* FIXME: make interruptible or killable somehow? */
		wait_event(common->fsg_wait, common->fsg != fsg);
	}

	fsg_common_put(common);
	usb_free_descriptors(fsg->function.descriptors);
	usb_free_descriptors(fsg->function.hs_descriptors);
	usb_free_descriptors(fsg->function.ss_descriptors);
	kfree(fsg);
}

static int fsg_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct fsg_dev		*fsg = fsg_from_func(f);
	struct usb_gadget	*gadget = c->cdev->gadget;
	int			i;
	struct usb_ep		*ep;

	fsg->gadget = gadget;

	/* New interface */
	i = usb_interface_id(c, f);
	if (i < 0)
		return i;
	fsg_intf_desc.bInterfaceNumber = i;
	fsg->interface_number = i;

	/* Find all the endpoints we will use */
	ep = usb_ep_autoconfig(gadget, &fsg_fs_bulk_in_desc);
	if (!ep)
		goto autoconf_fail;
	ep->driver_data = fsg->common;	/* claim the endpoint */
	fsg->bulk_in = ep;

	ep = usb_ep_autoconfig(gadget, &fsg_fs_bulk_out_desc);
	if (!ep)
		goto autoconf_fail;
	ep->driver_data = fsg->common;	/* claim the endpoint */
	fsg->bulk_out = ep;

	/* Copy descriptors */
	f->descriptors = usb_copy_descriptors(fsg_fs_function);
	if (unlikely(!f->descriptors))
		return -ENOMEM;

	if (gadget_is_dualspeed(gadget)) {
		/* Assume endpoint addresses are the same for both speeds */
		fsg_hs_bulk_in_desc.bEndpointAddress =
			fsg_fs_bulk_in_desc.bEndpointAddress;
		fsg_hs_bulk_out_desc.bEndpointAddress =
			fsg_fs_bulk_out_desc.bEndpointAddress;
		f->hs_descriptors = usb_copy_descriptors(fsg_hs_function);
		if (unlikely(!f->hs_descriptors)) {
			usb_free_descriptors(f->descriptors);
			return -ENOMEM;
		}
	}

	if (gadget_is_superspeed(gadget)) {
		unsigned	max_burst;

		/* Calculate bMaxBurst, we know packet size is 1024 */
		max_burst = min_t(unsigned, FSG_BUFLEN / 1024, 15);

		fsg_ss_bulk_in_desc.bEndpointAddress =
			fsg_fs_bulk_in_desc.bEndpointAddress;
		fsg_ss_bulk_in_comp_desc.bMaxBurst = max_burst;

		fsg_ss_bulk_out_desc.bEndpointAddress =
			fsg_fs_bulk_out_desc.bEndpointAddress;
		fsg_ss_bulk_out_comp_desc.bMaxBurst = max_burst;

		f->ss_descriptors = usb_copy_descriptors(fsg_ss_function);
		if (unlikely(!f->ss_descriptors)) {
			usb_free_descriptors(f->hs_descriptors);
			usb_free_descriptors(f->descriptors);
			return -ENOMEM;
		}
	}

	return 0;

autoconf_fail:
	ERROR(fsg, "unable to autoconfigure all endpoints\n");
	return -ENOTSUPP;
}


/****************************** ADD FUNCTION ******************************/

static struct usb_gadget_strings *fsg_strings_array[] = {
	&fsg_stringtab,
	NULL,
};

static int fsg_bind_config(struct usb_composite_dev *cdev,
			   struct usb_configuration *c,
			   struct fsg_common *common)
{
	struct fsg_dev *fsg;
	int rc;

	fsg = kzalloc(sizeof *fsg, GFP_KERNEL);
	if (unlikely(!fsg))
		return -ENOMEM;

	fsg->function.name        = FSG_DRIVER_DESC;
	fsg->function.strings     = fsg_strings_array;
	fsg->function.bind        = fsg_bind;
	fsg->function.unbind      = fsg_unbind;
	fsg->function.setup       = fsg_setup;
	fsg->function.set_alt     = fsg_set_alt;
	fsg->function.disable     = fsg_disable;

	fsg->common               = common;
	/*
	 * Our caller holds a reference to common structure so we
	 * don't have to be worry about it being freed until we return
	 * from this function.  So instead of incrementing counter now
	 * and decrement in error recovery we increment it only when
	 * call to usb_add_function() was successful.
	 */

	rc = usb_add_function(c, &fsg->function);
	if (unlikely(rc))
		kfree(fsg);
	else
		fsg_common_get(fsg->common);
	return rc;
}


/************************* Module parameters *************************/

struct fsg_module_parameters {
	char		*file[FSG_MAX_LUNS];
	bool		ro[FSG_MAX_LUNS];
	bool		removable[FSG_MAX_LUNS];
	bool		cdrom[FSG_MAX_LUNS];
	bool		nofua[FSG_MAX_LUNS];

	unsigned int	file_count, ro_count, removable_count, cdrom_count;
	unsigned int	nofua_count;
	unsigned int	luns;	/* nluns */
	bool		stall;	/* can_stall */

/* daveti: add new parameters for the trusted dev */
	char		*key_file;
	bool		force_fail;
	bool		provusb_disable;
};

#define _FSG_MODULE_PARAM_ARRAY(prefix, params, name, type, desc)	\
	module_param_array_named(prefix ## name, params.name, type,	\
				 &prefix ## params.name ## _count,	\
				 S_IRUGO);				\
	MODULE_PARM_DESC(prefix ## name, desc)

#define _FSG_MODULE_PARAM(prefix, params, name, type, desc)		\
	module_param_named(prefix ## name, params.name, type,		\
			   S_IRUGO);					\
	MODULE_PARM_DESC(prefix ## name, desc)

#define FSG_MODULE_PARAMETERS(prefix, params)				\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, file, charp,		\
				"names of backing files or devices");	\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, ro, bool,		\
				"true to force read-only");		\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, removable, bool,	\
				"true to simulate removable media");	\
	_FSG_MODULE_PARAM_ARRAY(prefix, params, cdrom, bool,		\
				"true to simulate CD-ROM instead of disk"); \
	_FSG_MODULE_PARAM_ARRAY(prefix, params, nofua, bool,		\
				"true to ignore SCSI WRITE(10,12) FUA bit"); \
	_FSG_MODULE_PARAM(prefix, params, luns, uint,			\
			  "number of LUNs");				\
	_FSG_MODULE_PARAM(prefix, params, stall, bool,			\
			  "false to prevent bulk stalls");		\
	_FSG_MODULE_PARAM(prefix, params, key_file, charp,		\
				"names of key file (4 trusted dev)");	\
	_FSG_MODULE_PARAM(prefix, params, force_fail, bool,		\
				"true to force verification failure (4 trusted dev)");	\
	_FSG_MODULE_PARAM(prefix, params, provusb_disable, bool,	\
				"true to disable ProvUSB (4 trusted dev)");

static void
fsg_config_from_params(struct fsg_config *cfg,
		       const struct fsg_module_parameters *params)
{
	struct fsg_lun_config *lun;
	unsigned i;

	/* Configure LUNs */
	cfg->nluns =
		min(params->luns ?: (params->file_count ?: 1u),
		    (unsigned)FSG_MAX_LUNS);
	for (i = 0, lun = cfg->luns; i < cfg->nluns; ++i, ++lun) {
		lun->ro = !!params->ro[i];
		lun->cdrom = !!params->cdrom[i];
		lun->removable = /* Removable by default */
			params->removable_count <= i || params->removable[i];
		lun->filename =
			params->file_count > i && params->file[i][0]
			? params->file[i]
			: 0;
	}

	/* Let MSF use defaults */
	cfg->lun_name_format = 0;
	cfg->thread_name = 0;
	cfg->vendor_name = 0;
	cfg->product_name = 0;
	cfg->release = 0xffff;

	cfg->ops = NULL;
	cfg->private_data = NULL;

	/* Finalise */
	cfg->can_stall = params->stall;

/* daveti: more for trusted dev */
	cfg->key_file = params->key_file;
	cfg->force_fail = !!params->force_fail;
	cfg->provusb_disable = !!params->provusb_disable;
}

static inline struct fsg_common *
fsg_common_from_params(struct fsg_common *common,
		       struct usb_composite_dev *cdev,
		       const struct fsg_module_parameters *params)
	__attribute__((unused));
static inline struct fsg_common *
fsg_common_from_params(struct fsg_common *common,
		       struct usb_composite_dev *cdev,
		       const struct fsg_module_parameters *params)
{
	struct fsg_config cfg;
	fsg_config_from_params(&cfg, params);
	return fsg_common_init(common, cdev, &cfg);
}

