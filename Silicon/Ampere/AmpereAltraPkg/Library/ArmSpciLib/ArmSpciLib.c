/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/ArmSmcLib.h>
#include <Library/ArmSpciLib.h>
#include <Library/DebugLib.h>

#include "ArmSpciSvcLib.h"

STATIC
EFI_STATUS
SpciStatusMap (
  UINTN SpciStatus
  )
{
  switch (SpciStatus) {
  case SPCI_SUCCESS:
    return EFI_SUCCESS;

  case SPCI_NOT_SUPPORTED:
    return EFI_UNSUPPORTED;

  case SPCI_INVALID_PARAMETER:
    return EFI_INVALID_PARAMETER;

  case SPCI_NO_MEMORY:
    return EFI_OUT_OF_RESOURCES;

  case SPCI_BUSY:
  case SPCI_QUEUED:
    return EFI_NOT_READY;

  case SPCI_DENIED:
    return EFI_ACCESS_DENIED;

  case SPCI_NOT_PRESENT:
    return EFI_NOT_FOUND;

  default:
    return EFI_DEVICE_ERROR;
  }
}

EFI_STATUS
EFIAPI
SpciServiceHandleOpen (
  UINT16   ClientId,
  UINT32   *HandleId,
  EFI_GUID Guid
  )
{
  ARM_SMC_ARGS SmcArgs;
  EFI_STATUS   Status;
  UINT32       X1;
  UINT64       Uuid1, Uuid2, Uuid3, Uuid4;

  if (HandleId == NULL) {
    DEBUG ((DEBUG_ERROR, "%a HandleId is NULL \n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Uuid1 = Guid.Data1;
  Uuid2 = Guid.Data3 << 16 | Guid.Data2;
  Uuid3 = Guid.Data4[3] << 24 | Guid.Data4[2] << 16 | Guid.Data4[1] << 8 \
          | Guid.Data4[0];
  Uuid4 = Guid.Data4[7] << 24 | Guid.Data4[6] << 16 | Guid.Data4[5] << 8 \
          | Guid.Data4[4];

  SmcArgs.Arg0 = SPCI_SERVICE_HANDLE_OPEN;
  SmcArgs.Arg1 = Uuid1;
  SmcArgs.Arg2 = Uuid2;
  SmcArgs.Arg3 = Uuid3;
  SmcArgs.Arg4 = Uuid4;
  SmcArgs.Arg5 = 0;
  SmcArgs.Arg6 = 0;
  SmcArgs.Arg7 = ClientId;
  ArmCallSmc (&SmcArgs);

  Status = SpciStatusMap (SmcArgs.Arg0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  X1 = SmcArgs.Arg1;

  if ((X1 & 0x0000FFFF) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: SpciServiceHandleOpen returned X1 = 0x%08x\n",
      __FUNCTION__,
      X1
      ));
    return EFI_DEVICE_ERROR;
  }

  /* Combine of returned handle and clientid */
  *HandleId = (UINT32)X1 | ClientId;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpciServiceHandleClose (
  UINT32 HandleId
  )
{
  ARM_SMC_ARGS SmcArgs;
  EFI_STATUS   Status;

  SmcArgs.Arg0 = SPCI_SERVICE_HANDLE_CLOSE;
  SmcArgs.Arg1 = HandleId;
  ArmCallSmc (&SmcArgs);

  Status = SpciStatusMap (SmcArgs.Arg0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpciServiceRequestStart (
  ARM_SPCI_ARGS *Args
  )
{
  ARM_SMC_ARGS SmcArgs;
  EFI_STATUS   Status;

  if (Args == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  SmcArgs.Arg0 = SPCI_SERVICE_REQUEST_START_AARCH64;
  SmcArgs.Arg1 = Args->X1;
  SmcArgs.Arg2 = Args->X2;
  SmcArgs.Arg3 = Args->X3;
  SmcArgs.Arg4 = Args->X4;
  SmcArgs.Arg5 = Args->X5;
  SmcArgs.Arg6 = Args->X6;
  SmcArgs.Arg7 = (UINT64)Args->HandleId;
  ArmCallSmc (&SmcArgs);

  Status = SpciStatusMap (SmcArgs.Arg0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Return Token
  Args->Token = SmcArgs.Arg1;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpciServiceRequestResume (
  ARM_SPCI_ARGS *Args
  )
{
  ARM_SMC_ARGS SmcArgs;
  EFI_STATUS   Status;

  if (Args == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  SmcArgs.Arg0 = SPCI_SERVICE_REQUEST_RESUME_AARCH64;
  SmcArgs.Arg1 = Args->Token;
  SmcArgs.Arg2 = 0;
  SmcArgs.Arg3 = 0;
  SmcArgs.Arg4 = 0;
  SmcArgs.Arg5 = 0;
  SmcArgs.Arg6 = 0;
  SmcArgs.Arg7 = (UINT64)Args->HandleId;
  ArmCallSmc (&SmcArgs);

  Status = SpciStatusMap (SmcArgs.Arg0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Args->X1 = SmcArgs.Arg1;
  Args->X2 = SmcArgs.Arg2;
  Args->X3 = SmcArgs.Arg3;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpciServiceGetResponse (
  ARM_SPCI_ARGS *Args
  )
{
  ARM_SMC_ARGS SmcArgs;
  EFI_STATUS   Status;

  if (Args == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  SmcArgs.Arg0 = SPCI_SERVICE_GET_RESPONSE_AARCH64;
  SmcArgs.Arg1 = Args->Token;
  SmcArgs.Arg2 = 0;
  SmcArgs.Arg3 = 0;
  SmcArgs.Arg4 = 0;
  SmcArgs.Arg5 = 0;
  SmcArgs.Arg6 = 0;
  SmcArgs.Arg7 = (UINT64)Args->HandleId;
  ArmCallSmc (&SmcArgs);

  Status = SpciStatusMap (SmcArgs.Arg0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Args->X1 = SmcArgs.Arg1;
  Args->X2 = SmcArgs.Arg2;
  Args->X3 = SmcArgs.Arg3;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SpciServiceRequestBlocking (
  ARM_SPCI_ARGS *Args
  )
{
  ARM_SMC_ARGS SmcArgs;
  EFI_STATUS   Status;

  if (Args == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  SmcArgs.Arg0 = SPCI_SERVICE_REQUEST_BLOCKING_AARCH64;
  SmcArgs.Arg1 = Args->X1;
  SmcArgs.Arg2 = Args->X2;
  SmcArgs.Arg3 = Args->X3;
  SmcArgs.Arg4 = Args->X4;
  SmcArgs.Arg5 = Args->X5;
  SmcArgs.Arg6 = Args->X6;
  SmcArgs.Arg7 = (UINT64)Args->HandleId;
  ArmCallSmc (&SmcArgs);

  Status = SpciStatusMap (SmcArgs.Arg0);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Args->X1 = SmcArgs.Arg1;
  Args->X2 = SmcArgs.Arg2;
  Args->X3 = SmcArgs.Arg3;

  return EFI_SUCCESS;
}
