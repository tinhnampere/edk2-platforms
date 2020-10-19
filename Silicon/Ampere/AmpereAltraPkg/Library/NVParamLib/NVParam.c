/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/ArmSmcLib.h>
#include <Library/DebugLib.h>
#include <Library/NVParamLib.h>
#include <NVParamDef.h>

#define SMC_NV_PARAM           0xC300FF04 /* Non-volatile function */
#define NV_PARAM_SMC_GET       0x00000001
#define NV_PARAM_SMC_SET       0x00000002
#define NV_PARAM_SMC_CLR       0x00000003
#define NV_PARAM_SMC_CLR_ALL   0x00000004

#define SMC_NOT_SET            -1000
#define SMC_ACCESS_DENIED      -1

INTN
NVParamGet (
  NVPARAM Param,
  UINT16 ACLRd,
  UINT32 *Val
  )
{
  ARM_SMC_ARGS SMCArgs = {
    .Arg0 = SMC_NV_PARAM,
    .Arg1 = NV_PARAM_SMC_GET,
    .Arg2 = Param,
    .Arg3 = (((UINT32) ACLRd) << 16),
  };

  ASSERT (Val != NULL);

  ArmCallSmc (&SMCArgs);
  if (SMCArgs.Arg0) {
    return EFI_UNSUPPORTED;
  }
  if (SMCArgs.Arg1 == 0) {
    *Val = SMCArgs.Arg2;
    return EFI_SUCCESS;
  }
  if (SMCArgs.Arg1 == SMC_NOT_SET) {
    return EFI_NOT_FOUND;
  }
  if (SMCArgs.Arg1 == SMC_ACCESS_DENIED) {
    return EFI_ACCESS_DENIED;
  }
  return EFI_INVALID_PARAMETER;
}

INTN
NVParamSet (
  NVPARAM Param,
  UINT16 ACLRd,
  UINT16 ACLWr,
  UINT32 Val
  )
{
  ARM_SMC_ARGS SMCArgs = {
    .Arg0 = SMC_NV_PARAM,
    .Arg1 = NV_PARAM_SMC_SET,
    .Arg2 = Param,
    .Arg3 = (((UINT32) ACLRd) << 16) | ACLWr,
    .Arg4 = Val,
  };

  ArmCallSmc (&SMCArgs);
  if (SMCArgs.Arg0) {
    return EFI_UNSUPPORTED;
  }
  if (SMCArgs.Arg1 == 0) {
    return EFI_SUCCESS;
  }
  if (SMCArgs.Arg1 == SMC_ACCESS_DENIED) {
    return EFI_ACCESS_DENIED;
  }
  return EFI_INVALID_PARAMETER;
}

INTN
NVParamClr (
  NVPARAM Param,
  UINT16 ACLWr
  )
{
  ARM_SMC_ARGS SMCArgs = {
    .Arg0 = SMC_NV_PARAM,
    .Arg1 = NV_PARAM_SMC_CLR,
    .Arg2 = Param,
    .Arg3 = ACLWr,
  };

  ArmCallSmc (&SMCArgs);
  if (SMCArgs.Arg0) {
    return EFI_UNSUPPORTED;
  }
  if (SMCArgs.Arg1 == 0) {
    return EFI_SUCCESS;
  }
  if (SMCArgs.Arg1 == SMC_ACCESS_DENIED) {
    return EFI_ACCESS_DENIED;
  }
  return EFI_INVALID_PARAMETER;
}

INTN
NVParamClrAll (
  VOID
  )
{
  ARM_SMC_ARGS SMCArgs = {
    .Arg0 = SMC_NV_PARAM,
    .Arg1 = NV_PARAM_SMC_CLR_ALL,
  };

  ArmCallSmc (&SMCArgs);
  if (SMCArgs.Arg0) {
    return EFI_UNSUPPORTED;
  }
  if (SMCArgs.Arg1 == 0) {
    return EFI_SUCCESS;
  }
  return EFI_INVALID_PARAMETER;
}
