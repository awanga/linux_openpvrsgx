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

#include <linux/version.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/hardirq.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "sgxdefs.h"
#include "services_headers.h"
#include "sysinfo.h"
#include "sgxapi_km.h"
#include "sysconfig.h"
#include "sgxinfokm.h"
#include "syslocal.h"

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/types.h>

/*
 * The GPU is switched through a secure monitor call (see sysconfig.h): the
 * console has no clock or regulator the kernel can control for it.  r2 and r3
 * come back overwritten by the monitor, which is why they are outputs.
 */
static int VitaSmcGpuPower(unsigned int state)
{
	register unsigned int r0 asm("r0") = SYS_VITA_SMC_DOMAIN_GPU;
	register unsigned int r1 asm("r1") = state;
	register unsigned int r2 asm("r2") = 0;
	register unsigned int r3 asm("r3") = 0;
	register unsigned int r12 asm("r12") = SYS_VITA_SMC_POWER;

	asm volatile(".arch_extension sec\n\tsmc #0"
		     : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
		     : "r"(r12)
		     : "memory");
	return (int)r0;
}

extern struct platform_device *gpsPVRLDMDev;

static PVRSRV_ERROR PowerLockWrap(SYS_SPECIFIC_DATA *psSysSpecData, IMG_BOOL bTryLock)
{
	if (!in_interrupt())
	{
		if (bTryLock)
		{
			int locked = mutex_trylock(&psSysSpecData->sPowerLock);
			if (locked == 0)
			{
				return PVRSRV_ERROR_RETRY;
			}
		}
		else
		{
			mutex_lock(&psSysSpecData->sPowerLock);
		}
	}

	return PVRSRV_OK;
}

static IMG_VOID PowerLockUnwrap(SYS_SPECIFIC_DATA *psSysSpecData)
{
	if (!in_interrupt())
	{
		mutex_unlock(&psSysSpecData->sPowerLock);
	}
}

PVRSRV_ERROR SysPowerLockWrap(IMG_BOOL bTryLock)
{
	SYS_DATA	*psSysData;

	SysAcquireData(&psSysData);

	return PowerLockWrap(psSysData->pvSysSpecificData, bTryLock);
}

IMG_VOID SysPowerLockUnwrap(IMG_VOID)
{
	SYS_DATA	*psSysData;

	SysAcquireData(&psSysData);

	PowerLockUnwrap(psSysData->pvSysSpecificData);
}

/*
 * This function should be called to unwrap the Services power lock, prior
 * to calling any function that might sleep.
 * This function shouldn't be called prior to calling EnableSystemClocks
 * or DisableSystemClocks, as those functions perform their own power lock
 * unwrapping.
 * If the function returns IMG_TRUE, UnwrapSystemPowerChange must be
 * called to rewrap the power lock, prior to returning to Services.
 */
IMG_BOOL WrapSystemPowerChange(SYS_SPECIFIC_DATA *psSysSpecData)
{
	
	return IMG_TRUE;
}

IMG_VOID UnwrapSystemPowerChange(SYS_SPECIFIC_DATA *psSysSpecData)
{
	
}

/*
 * Return SGX timining information to caller.
 */
IMG_VOID SysGetSGXTimingInformation(SGX_TIMING_INFORMATION *psTimingInfo)
{
#if !defined(NO_HARDWARE)
	PVR_ASSERT(atomic_read(&gpsSysSpecificData->sSGXClocksEnabled) != 0);
#endif
	psTimingInfo->ui32CoreClockSpeed = SYS_SGX_CLOCK_SPEED;
	psTimingInfo->ui32HWRecoveryFreq = SYS_SGX_HWRECOVERY_TIMEOUT_FREQ;
	psTimingInfo->ui32uKernelFreq = SYS_SGX_PDS_TIMER_FREQ;
#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	psTimingInfo->bEnableActivePM = IMG_TRUE;
#else
	psTimingInfo->bEnableActivePM = IMG_FALSE;
#endif /* SUPPORT_ACTIVE_POWER_MANAGEMENT */
	psTimingInfo->ui32ActivePowManLatencyms = SYS_SGX_ACTIVE_POWER_LATENCY_MS;
}

/*!
******************************************************************************

 @Function  EnableSGXClocks

 @Description Power the GPU on.  The call also releases the GPU's reset and
              opens its clock gate; the register window answers afterwards
              (master bank only: see docs/vita/linux-boot.md).

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR EnableSGXClocks(SYS_DATA *psSysData, IMG_BOOL bNoDev)
{
#if !defined(NO_HARDWARE)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	PVR_UNREFERENCED_PARAMETER(bNoDev);

	if (atomic_read(&psSysSpecData->sSGXClocksEnabled) != 0)
	{
		return PVRSRV_OK;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "EnableSGXClocks: powering the GPU on"));
	if (VitaSmcGpuPower(1) != 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "EnableSGXClocks: the power call failed"));
		return PVRSRV_ERROR_UNABLE_TO_ENABLE_CLOCK;
	}

	SysEnableSGXInterrupts(psSysData);
	atomic_set(&psSysSpecData->sSGXClocksEnabled, 1);
#else
	PVR_UNREFERENCED_PARAMETER(psSysData);
	PVR_UNREFERENCED_PARAMETER(bNoDev);
#endif
	return PVRSRV_OK;
}

/*!
******************************************************************************

 @Function  DisableSGXClocks

 @Description Power the GPU off.

 @Return   none

******************************************************************************/
IMG_VOID DisableSGXClocks(SYS_DATA *psSysData)
{
#if !defined(NO_HARDWARE)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	if (atomic_read(&psSysSpecData->sSGXClocksEnabled) == 0)
	{
		return;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "DisableSGXClocks: powering the GPU off"));
	SysDisableSGXInterrupts(psSysData);
	VitaSmcGpuPower(0);
	atomic_set(&psSysSpecData->sSGXClocksEnabled, 0);
#else
	PVR_UNREFERENCED_PARAMETER(psSysData);
#endif
}

/* There are no separate system clocks to set up: the power call above is the
 * whole of it. */
PVRSRV_ERROR EnableSystemClocks(SYS_DATA *psSysData)
{
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	psSysSpecData->bSysClocksOneTimeInit = IMG_TRUE;
	return PVRSRV_OK;
}

IMG_VOID DisableSystemClocks(SYS_DATA *psSysData)
{
	PVR_UNREFERENCED_PARAMETER(psSysData);
}

PVRSRV_ERROR SysPMRuntimeRegister(void)
{
	pm_runtime_enable(&gpsPVRLDMDev->dev);

	return PVRSRV_OK;
}

PVRSRV_ERROR SysPMRuntimeUnregister(void)
{
	pm_runtime_disable(&gpsPVRLDMDev->dev);

	return PVRSRV_OK;
}

PVRSRV_ERROR SysDvfsInitialize(SYS_SPECIFIC_DATA *psSysSpecificData)
{
	PVR_UNREFERENCED_PARAMETER(psSysSpecificData);

	return PVRSRV_OK;
}

PVRSRV_ERROR SysDvfsDeinitialize(SYS_SPECIFIC_DATA *psSysSpecificData)
{
	PVR_UNREFERENCED_PARAMETER(psSysSpecificData);

	return PVRSRV_OK;
}

