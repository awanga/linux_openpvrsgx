/*************************************************************************/ /*!
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if !defined(__SOCCONFIG_H__)
#define __SOCCONFIG_H__
#define VS_PRODUCT_NAME	"vita"

/*
 * The GPU's clock under the console's OS is 111 MHz (0xE3103010 reads
 * 0x00030003); it is not changed from here.
 */
#define SYS_SGX_CLOCK_SPEED		 111000000

#define SYS_SGX_HWRECOVERY_TIMEOUT_FREQ		(100)	// 10ms (100hz)
#define SYS_SGX_PDS_TIMER_FREQ				(1000)	// 1ms (1000hz)

/* Allow the AP latency to be overridden in the build config */
#if !defined(SYS_SGX_ACTIVE_POWER_LATENCY_MS)
#define SYS_SGX_ACTIVE_POWER_LATENCY_MS		(2)
#endif


/*
 * Register window and interrupt come from the device tree (platform
 * resources); these are the values the console's physical map gives.  The
 * window holds the master bank at +0x4000 and one 16 KB bank per core
 * from +0x8000, so four cores end at 0x18000.
 */
#define SYS_VITA_SGX_REGS_SYS_PHYS_BASE  0xE8400000

/* The GPU addresses memory by the CPU's physical addresses (the page directory
 * base the console's OS programs is a RAM address, 0x41a3a000), so the device
 * address is the CPU address. */
#define SYS_VITA_DEV_PHYS_OFFSET 0x0
#define SYS_VITA_SGX_REGS_SIZE           0x20000
#define SYS_VITA_SGX_IRQ                 0	/* not used: platform resource */

/*
 * Secure monitor call that switches the GPU's power: function 0x110 in r12,
 * domain 0 in r0, state in r1 (1 on, 0 off).  Measured on the console, not
 * a documented interface.
 */
#define SYS_VITA_SMC_POWER		0x110
#define SYS_VITA_SMC_DOMAIN_GPU		0

/* Interrupt bits */
#define DEVICE_SGX_INTERRUPT		(1<<0)
#define DEVICE_MSVDX_INTERRUPT		(1<<1)
#define DEVICE_DISP_INTERRUPT		(1<<2)

#if defined(__linux__)

#if defined(PVR_LDM_PLATFORM_PRE_REGISTERED_DEV)
#define	SYS_SGX_DEV_NAME	PVR_LDM_PLATFORM_PRE_REGISTERED_DEV
#else
#define	SYS_SGX_DEV_NAME	"vita_gpu"
#endif	/* defined(PVR_LDM_PLATFORM_PRE_REGISTERED_DEV) */
#endif	/* defined(__linux__) */

/*****************************************************************************
 * system specific data structures
 *****************************************************************************/
 
#endif	/* __SYSCONFIG_H__ */
