/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVParamLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <MmLib.h>
#include <Protocol/MmCommunication.h>


STATIC EFI_MM_COMMUNICATION_PROTOCOL *mNVParamMmCommProtocol = NULL;
STATIC EFI_MM_COMM_REQUEST           *mCommBuffer            = NULL;

/**
  This is a notification function registered on EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE
  event. It converts a pointer to a new virtual address.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context

**/
VOID
EFIAPI
RuntimeAddressChangeEvent (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  gRT->ConvertPointer (0, (VOID **)&mNVParamMmCommProtocol);
  gRT->ConvertPointer (0, (VOID **)&mCommBuffer);
}

EFI_STATUS
EFIAPI
NVParamLibConstructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_EVENT  VirtualAddressChangeEvent = NULL;
  EFI_STATUS Status = EFI_SUCCESS;

  mCommBuffer = AllocateRuntimeZeroPool (sizeof (EFI_MM_COMM_REQUEST));
  ASSERT (mCommBuffer != NULL);

  Status = gBS->LocateProtocol (
                  &gEfiMmCommunicationProtocolGuid,
                  NULL,
                  (VOID **)&mNVParamMmCommProtocol
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->CreateEvent (
                  EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
                  TPL_CALLBACK,
                  RuntimeAddressChangeEvent,
                  NULL,
                  &VirtualAddressChangeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
NvParamMmCommunicate (
  IN OUT VOID  *CommBuffer,
  IN OUT UINTN *CommSize
  )
{
  if (mNVParamMmCommProtocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return mNVParamMmCommProtocol->Communicate (
                                   mNVParamMmCommProtocol,
                                   CommBuffer,
                                   CommSize
                                   );
}

STATIC EFI_STATUS
UefiMmCreateNVParamReq (
  IN VOID   *Data,
  IN UINT64 Size
  )
{
  if (mCommBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CopyGuid (&mCommBuffer->EfiMmHdr.HeaderGuid, &gNVParamMmGuid);
  mCommBuffer->EfiMmHdr.MsgLength = Size;

  if (Size != 0) {
    ASSERT (Data);
    ASSERT (Size <= EFI_MM_MAX_PAYLOAD_SIZE);

    CopyMem (mCommBuffer->PayLoad.Data, Data, Size);
  }

  return EFI_SUCCESS;
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

  Status = UefiMmCreateNVParamReq ((VOID *)&MmData, sizeof (MmData));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = NvParamMmCommunicate (
             mCommBuffer,
             &Size
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmNVParamRes = (EFI_MM_COMMUNICATE_NVPARAM_RES *)&mCommBuffer->PayLoad;
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

  Status = UefiMmCreateNVParamReq ((VOID *)&MmData, sizeof (MmData));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = NvParamMmCommunicate (
             mCommBuffer,
             &Size
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmNVParamRes = (EFI_MM_COMMUNICATE_NVPARAM_RES *)&mCommBuffer->PayLoad;
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

  Status = UefiMmCreateNVParamReq ((VOID *)&MmData, sizeof (MmData));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = NvParamMmCommunicate (
             mCommBuffer,
             &Size
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmNVParamRes = (EFI_MM_COMMUNICATE_NVPARAM_RES *)&mCommBuffer->PayLoad;
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

  Status = UefiMmCreateNVParamReq ((VOID *)&MmData, sizeof (MmData));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = NvParamMmCommunicate (
             mCommBuffer,
             &Size
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmNVParamRes = (EFI_MM_COMMUNICATE_NVPARAM_RES *)&mCommBuffer->PayLoad;
  switch (MmNVParamRes->Status) {
  case MM_NVPARAM_RES_SUCCESS:
    return EFI_SUCCESS;

  case MM_NVPARAM_RES_FAIL:
    return EFI_DEVICE_ERROR;

  default:
    return EFI_INVALID_PARAMETER;
  }
}
