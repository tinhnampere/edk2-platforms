/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>
  
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/NVParamLib.h>
#include <Library/SMProLib.h>
#include <Library/SMProInterface.h>
#include <Library/PlatformInfo.h>

#define SMC_SMPMPRO_PROXY_FUNC_ID   0xc300ff03
#define SOC_EFUSE_SHADOWn(n)        (SMPRO_EFUSE_SHADOW0 + (n) * 4)
/* DVFS enable bit is in EFUSE2 register */
#define EFUSE_DVFS_ENABLE(efuse)    (((efuse) & 0x04000000) >> 26)
/* Turbo enable bit is in EFUSE7 register */
#define EFUSE_TURBO_ENABLE(efuse)   (((efuse) & 0x00020000) >> 17)
/* Turbo frequency bit is in EFUSE8 register */
#define EFUSE_TURBO_FREQ(efuse)     (((efuse) & 0x00000FC0) >> 6)
/* Frequency bit is in EFUSE2 register */
#define EFUSE_SPEED_GRADE_FREQ(efuse) (((efuse) & 0x03F00000) >> 20)
/* AVS bit is in EFUSE2 register */
#define EFUSE_AVS(efuse)            (((efuse) & 0x08000000) >> 27)

#define MHZ_SCALE_FACTOR            1000000

/* sub type for SMPMPro Proxy SMC calling */
enum {
  SMPRO_FUNC_GET_FW_VER = 0,
  SMPRO_FUNC_GET_FW_BUILD,
  SMPRO_FUNC_GET_FW_CAP,
  PMPRO_FUNC_GET_FW_VER,
  PMPRO_FUNC_GET_FW_BUILD,
  PMPRO_FUNC_GET_FW_CAP,
  SMPRO_FUNC_SET_CFG,
};

/**
  Return TRUE if this is an efuse chip
**/
STATIC
BOOLEAN
IsEfuseChip (
  VOID
  )
{
  UINT32 Efuse;

  Efuse = MmioRead32 (SOC_EFUSE_SHADOWn(2));

  //
  // The efused chip has an enabled AVS bit (not zero).
  // Otherwise, this chip is not efused.
  //
  if (EFUSE_AVS(Efuse)) {
    return TRUE;
  }

  return FALSE;
}

/**
  Get an ASCII string of SmPro FW version
**/
VOID
GetSmProVersion (
  UINT8 *Buf,
  UINT32 Length
  )
{
  ARM_SMC_ARGS SmcArgs;

  ASSERT (Buf != NULL);

  SetMem ((VOID *) Buf, Length, 0);
  SmcArgs.Arg0 = SMC_SMPMPRO_PROXY_FUNC_ID;
  SmcArgs.Arg1 = SMPRO_FUNC_GET_FW_VER;
  SmcArgs.Arg2 = (UINT64) Buf;
  SmcArgs.Arg3 = Length;

  ArmCallSmc (&SmcArgs);
}

/**
  Get an ASCII string of PmPro FW version
**/
VOID
GetPmProVersion (
  UINT8 *Buf,
  UINT32 Length
  )
{
  ARM_SMC_ARGS SmcArgs;

  ASSERT (Buf != NULL);

  SetMem ((VOID *) Buf, Length, 0);
  SmcArgs.Arg0 = SMC_SMPMPRO_PROXY_FUNC_ID;
  SmcArgs.Arg1 = PMPRO_FUNC_GET_FW_VER;
  SmcArgs.Arg2 = (UINT64) Buf;
  SmcArgs.Arg3 = Length;

  ArmCallSmc (&SmcArgs);
}

/**
  Get an ASCII string of iPP version
**/
VOID
GetIPPVersion (
  UINT8 *Buf,
  UINT32 Length
  )
{
  ARM_SMC_ARGS SmcArgs;

  ASSERT (Buf != NULL);

  SetMem ((VOID *) Buf, Length, 0);
  SmcArgs.Arg0 = SMC_SMPMPRO_PROXY_FUNC_ID;
  SmcArgs.Arg1 = SMPRO_FUNC_GET_FW_BUILD;
  SmcArgs.Arg2 = (UINT64) Buf;
  SmcArgs.Arg3 = Length;

  ArmCallSmc (&SmcArgs);
}

/**
  Set SMpro configuration run-time parameter
**/
UINT64
SMproSetCfg (
  UINT8 CfgType,
  UINT32 Param,
  UINT32 Data
  )
{
  ARM_SMC_ARGS SmcArgs;

  SmcArgs.Arg0 = SMC_SMPMPRO_PROXY_FUNC_ID;
  SmcArgs.Arg1 = SMPRO_FUNC_SET_CFG;
  SmcArgs.Arg2 = CfgType;
  SmcArgs.Arg3 = Param;
  SmcArgs.Arg4 = Data;

  ArmCallSmc (&SmcArgs);
  if (!SmcArgs.Arg0) {
    return SmcArgs.Arg1;
  }

  return 0;
}

/**
  Return Turbo frequency if Turbo is supported
**/
UINT64
GetTurboFreq (
  VOID
  )
{
  UINT32 Efuse = MmioRead32 (SOC_EFUSE_SHADOWn(8));

  if (EFUSE_TURBO_FREQ(Efuse)) {
    return EFUSE_TURBO_FREQ(Efuse) * 100 * MHZ_SCALE_FACTOR;
  }

  return FixedPcdGet64 (PcdTurboDefaultFreq);
}

/**
  Return TRUE if chip supports Turbo mode
**/
BOOLEAN
GetTurboSupport (
  VOID
  )
{
  UINT32 Efuse;

  if (!IsEfuseChip ()) {
    return TRUE;
  }

  Efuse = MmioRead32 (SOC_EFUSE_SHADOWn(7));
  if (EFUSE_TURBO_ENABLE(Efuse)) {
    return TRUE;
  }

  return FALSE;
}

/**
  Return TRUE if DVFS is supported
**/
BOOLEAN
GetDVFSSupport (
  VOID
  )
{
  UINT32 Efuse;

  if (!IsEfuseChip ()) {
    return TRUE;
  }

  Efuse = MmioRead32 (SOC_EFUSE_SHADOWn(2));

  if (EFUSE_DVFS_ENABLE(Efuse)) {
    return TRUE;
  }

  return FALSE;
}

/**
  Return SMPro BMC register value
**/
EFI_STATUS
GetSMProBMCReg (
  IN    UINT8   BmcReg,
  OUT   UINT32  *Value
  )
{
  UINT32        Msg[3];
  EFI_STATUS    Status;

  Msg[0] = SMPRO_I2C_ENCODE_MSG(0, SMPRO_I2C_BMC_BUS_ADDR,
            SMPRO_I2C_RD, SMPRO_I2C_PROTOCOL, 0, 0);
  Msg[1] = BmcReg;
  Msg[2] = 0;

  Status = SMProDBWr (
      SMPRO_NS_MAILBOX_INDEX,
      Msg[0],
      Msg[1],
      Msg[2],
      SMPRO_DB_BASE_REG
      );

  if (Status < 0) {
   return Status;
  }

  Status = SMProDBRd (
      SMPRO_NS_MAILBOX_INDEX,
      &Msg[0],
      &Msg[1],
      &Msg[2],
      SMPRO_DB_BASE_REG
      );

  if (Status < 0) {
   return Status;
  }

  ASSERT (Value != NULL);

  if (Msg[0] == IPP_ENCODE_OK_MSG) {
    *Value = Msg[1];
    return EFI_SUCCESS;
  }

  return EFI_DEVICE_ERROR;
}
