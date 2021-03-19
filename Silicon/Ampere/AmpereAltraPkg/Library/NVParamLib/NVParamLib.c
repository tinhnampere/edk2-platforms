/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MmCommunicationLib.h>
#include <Library/NVParamLib.h>
#include <MmLib.h>

EFI_MM_COMM_REQUEST mCommBuffer;

STATIC VOID
UefiMmCreateNVParamReq (
  IN VOID   *Data,
  IN UINT64 Size
  )
{
  CopyGuid (&mCommBuffer.EfiMmHdr.HeaderGuid, &gNVParamMmGuid);
  mCommBuffer.EfiMmHdr.MsgLength = Size;

  if (Size != 0) {
    ASSERT (Data);
    ASSERT (Size <= EFI_MM_MAX_PAYLOAD_SIZE);

    CopyMem (mCommBuffer.PayLoad.Data, Data, Size);
  }
}

EFI_STATUS
NVParamGet (
  IN  UINT32 Param,
  IN  UINT16 ACLRd,
  OUT UINT32 *Val
  )
{
  EFI_STATUS                     Status;
  EFI_MM_COMMUNICATE_NVPARAM_RES *MmNVParamRes;
  UINT64                         MmData[5];
  UINTN                          Size;

  MmData[0] = MM_NVPARAM_FUNC_READ;
  MmData[1] = Param;
  MmData[2] = (UINT64)ACLRd;

  UefiMmCreateNVParamReq ((VOID *)&MmData, sizeof (MmData));

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = MmCommunicationCommunicate (
             (VOID *)&mCommBuffer,
             &Size
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmNVParamRes = (EFI_MM_COMMUNICATE_NVPARAM_RES *)&mCommBuffer.PayLoad;
  switch (MmNVParamRes->Status) {
  case MM_NVPARAM_RES_SUCCESS:
    *Val = (UINT32)MmNVParamRes->Value;
    return EFI_SUCCESS;

  case MM_NVPARAM_RES_NOT_SET:
    return EFI_NOT_FOUND;

  case MM_NVPARAM_RES_NO_PERM:
    return EFI_ACCESS_DENIED;

  case MM_NVPARAM_RES_FAIL:
    return EFI_DEVICE_ERROR;

  default:
    return EFI_INVALID_PARAMETER;
  }
}

EFI_STATUS
NVParamSet (
  IN UINT32 Param,
  IN UINT16 ACLRd,
  IN UINT16 ACLWr,
  IN UINT32 Val
  )
{
  EFI_STATUS                     Status;
  EFI_MM_COMMUNICATE_NVPARAM_RES *MmNVParamRes;
  UINT64                         MmData[5];
  UINTN                          Size;

  MmData[0] = MM_NVPARAM_FUNC_WRITE;
  MmData[1] = Param;
  MmData[2] = (UINT64)ACLRd;
  MmData[3] = (UINT64)ACLWr;
  MmData[4] = (UINT64)Val;

  UefiMmCreateNVParamReq ((VOID *)&MmData, sizeof (MmData));
  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = MmCommunicationCommunicate (
             (VOID *)&mCommBuffer,
             &Size
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmNVParamRes = (EFI_MM_COMMUNICATE_NVPARAM_RES *)&mCommBuffer.PayLoad;
  switch (MmNVParamRes->Status) {
  case MM_NVPARAM_RES_SUCCESS:
    return EFI_SUCCESS;

  case MM_NVPARAM_RES_NO_PERM:
    return EFI_ACCESS_DENIED;

  case MM_NVPARAM_RES_FAIL:
    return EFI_DEVICE_ERROR;

  default:
    return EFI_INVALID_PARAMETER;
  }
}

EFI_STATUS
NVParamClr (
  IN UINT32 Param,
  IN UINT16 ACLWr
  )
{
  EFI_STATUS                     Status;
  EFI_MM_COMMUNICATE_NVPARAM_RES *MmNVParamRes;
  UINT64                         MmData[5];
  UINTN                          Size;

  MmData[0] = MM_NVPARAM_FUNC_CLEAR;
  MmData[1] = Param;
  MmData[2] = 0;
  MmData[3] = (UINT64)ACLWr;

  UefiMmCreateNVParamReq ((VOID *)&MmData, sizeof (MmData));
  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = MmCommunicationCommunicate (
             (VOID *)&mCommBuffer,
             &Size
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmNVParamRes = (EFI_MM_COMMUNICATE_NVPARAM_RES *)&mCommBuffer.PayLoad;
  switch (MmNVParamRes->Status) {
  case MM_NVPARAM_RES_SUCCESS:
    return EFI_SUCCESS;

  case MM_NVPARAM_RES_NO_PERM:
    return EFI_ACCESS_DENIED;

  case MM_NVPARAM_RES_FAIL:
    return EFI_DEVICE_ERROR;

  default:
    return EFI_INVALID_PARAMETER;
  }
}

EFI_STATUS
NVParamClrAll (
  VOID
  )
{
  EFI_STATUS                     Status;
  EFI_MM_COMMUNICATE_NVPARAM_RES *MmNVParamRes;
  UINT64                         MmData[5];
  UINTN                          Size;

  MmData[0] = MM_NVPARAM_FUNC_CLEAR_ALL;

  UefiMmCreateNVParamReq ((VOID *)&MmData, sizeof (MmData));
  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = MmCommunicationCommunicate (
             (VOID *)&mCommBuffer,
             &Size
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmNVParamRes = (EFI_MM_COMMUNICATE_NVPARAM_RES *)&mCommBuffer.PayLoad;
  switch (MmNVParamRes->Status) {
  case MM_NVPARAM_RES_SUCCESS:
    return EFI_SUCCESS;

  case MM_NVPARAM_RES_FAIL:
    return EFI_DEVICE_ERROR;

  default:
    return EFI_INVALID_PARAMETER;
  }
}
