/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FlashLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <MmLib.h>
#include <Protocol/MmCommunication.h>

STATIC EFI_MM_COMMUNICATION_PROTOCOL *mMmCommunicationProtocol = NULL;
STATIC EFI_MM_COMM_REQUEST           *mCommBuffer              = NULL;

BOOLEAN mIsEfiRuntime;
UINT8   *mTmpBufVirt;
UINT8   *mTmpBufPhy;

/**
  This is a notification function registered on EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE
  event. It converts a pointer to a new virtual address.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context

**/
VOID
EFIAPI
FlashLibAddressChangeEvent (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  gRT->ConvertPointer (0x0, (VOID **)&mTmpBufVirt);
  gRT->ConvertPointer (0x0, (VOID **)&mCommBuffer);
  gRT->ConvertPointer (0x0, (VOID **)&mMmCommunicationProtocol);

  mIsEfiRuntime = TRUE;
}

EFI_STATUS
EFIAPI
FlashLibConstructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_EVENT  VirtualAddressChangeEvent = NULL;
  EFI_STATUS Status = EFI_SUCCESS;

  mCommBuffer = AllocateRuntimeZeroPool (sizeof (EFI_MM_COMM_REQUEST));
  ASSERT (mCommBuffer != NULL);

  mTmpBufPhy = AllocateRuntimeZeroPool (EFI_MM_MAX_TMP_BUF_SIZE);
  mTmpBufVirt = mTmpBufPhy;
  ASSERT (mTmpBufPhy != NULL);

  Status = gBS->LocateProtocol (
                  &gEfiMmCommunicationProtocolGuid,
                  NULL,
                  (VOID **)&mMmCommunicationProtocol
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->CreateEvent (
                  EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
                  TPL_CALLBACK,
                  FlashLibAddressChangeEvent,
                  NULL,
                  &VirtualAddressChangeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
FlashMmCommunicate (
  IN OUT VOID  *CommBuffer,
  IN OUT UINTN *CommSize
  )
{
  if (mMmCommunicationProtocol == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return mMmCommunicationProtocol->Communicate (
                                     mMmCommunicationProtocol,
                                     CommBuffer,
                                     CommSize
                                     );
}

STATIC
EFI_STATUS
UefiMmCreateSpiNorReq (
  IN VOID   *Data,
  IN UINT64 Size
  )
{
  if (mCommBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CopyGuid (&mCommBuffer->EfiMmHdr.HeaderGuid, &gSpiNorMmGuid);
  mCommBuffer->EfiMmHdr.MsgLength = Size;

  if (Size != 0) {
    ASSERT (Data);
    ASSERT (Size <= EFI_MM_MAX_PAYLOAD_SIZE);

    CopyMem (mCommBuffer->PayLoad.Data, Data, Size);
  }

  return EFI_SUCCESS;
}

/**
  Convert Virtual Address to Physical Address at Runtime Services

  @param  VirtualPtr          Virtual Address Pointer
  @param  Size                Total bytes of the buffer

  @retval Ptr                 Return the pointer of the converted address

**/
STATIC
UINT8 *
ConvertVirtualToPhysical (
  IN UINT8 *VirtualPtr,
  IN UINTN Size
  )
{
  if (mIsEfiRuntime) {
    ASSERT (VirtualPtr != NULL);
    CopyMem ((VOID *)mTmpBufVirt, (VOID *)VirtualPtr, Size);
    return (UINT8 *)mTmpBufPhy;
  }

  return (UINT8 *)VirtualPtr;
}

/**
  Convert Physical Address to Virtual Address at Runtime Services

  @param  VirtualPtr          Physical Address Pointer
  @param  Size                Total bytes of the buffer

**/
STATIC
VOID
ConvertPhysicaltoVirtual (
  IN UINT8 *PhysicalPtr,
  IN UINTN Size
  )
{
  if (mIsEfiRuntime) {
    ASSERT (PhysicalPtr != NULL);
    CopyMem ((VOID *)PhysicalPtr, (VOID *)mTmpBufVirt, Size);
  }
}

EFI_STATUS
EFIAPI
FlashGetNvRamInfo (
  OUT UINT64 *NvRamBase,
  OUT UINT32 *NvRamSize
  )
{
  EFI_MM_COMMUNICATE_SPINOR_NVINFO_RES *MmSpiNorNVInfoRes;
  EFI_STATUS                           Status;
  UINT64                               MmData[5];
  UINTN                                Size;

  MmData[0] = MM_SPINOR_FUNC_GET_NVRAM_INFO;

  Status = UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof (MmData));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = FlashMmCommunicate (
             mCommBuffer,
             &Size
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmSpiNorNVInfoRes = (EFI_MM_COMMUNICATE_SPINOR_NVINFO_RES *)&mCommBuffer->PayLoad;
  if (MmSpiNorNVInfoRes->Status == MM_SPINOR_RES_SUCCESS) {
    *NvRamBase = MmSpiNorNVInfoRes->NVBase;
    *NvRamSize = MmSpiNorNVInfoRes->NVSize;
    DEBUG ((DEBUG_INFO, "NVInfo Base 0x%llx, Size 0x%lx\n", *NvRamBase, *NvRamSize));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FlashEraseCommand (
  IN UINT8  *pBlockAddress,
  IN UINT32 Length
  )
{
  EFI_MM_COMMUNICATE_SPINOR_RES *MmSpiNorRes;
  EFI_STATUS                    Status;
  UINT64                        MmData[5];
  UINTN                         Size;

  ASSERT (pBlockAddress != NULL);

  MmData[0] = MM_SPINOR_FUNC_ERASE;
  MmData[1] = (UINT64)pBlockAddress;
  MmData[2] = Length;

  Status = UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof (MmData));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = FlashMmCommunicate (
             mCommBuffer,
             &Size
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mCommBuffer->PayLoad;
  if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Flash Erase: Device error %llx\n", MmSpiNorRes->Status));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FlashProgramCommand (
  IN     UINT8 *pByteAddress,
  IN     UINT8 *Byte,
  IN OUT UINTN *Length
  )
{
  EFI_MM_COMMUNICATE_SPINOR_RES *MmSpiNorRes;
  EFI_STATUS                    Status;
  UINT64                        MmData[5];
  UINTN                         Remain, Size, NumWrite;
  UINTN                         Count = 0;

  ASSERT (pByteAddress != NULL);
  ASSERT (Byte != NULL);
  ASSERT (Length != NULL);

  Remain = *Length;
  while (Remain > 0) {
    NumWrite = (Remain > EFI_MM_MAX_TMP_BUF_SIZE) ? EFI_MM_MAX_TMP_BUF_SIZE : Remain;

    MmData[0] = MM_SPINOR_FUNC_WRITE;
    MmData[1] = (UINT64)pByteAddress;
    MmData[2] = NumWrite;
    MmData[3] = (UINT64)ConvertVirtualToPhysical (Byte + Count, NumWrite);

    Status = UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof (MmData));
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
    Status = FlashMmCommunicate (
               mCommBuffer,
               &Size
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mCommBuffer->PayLoad;
    if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
      DEBUG ((DEBUG_ERROR, "Flash program: Device error 0x%llx\n", MmSpiNorRes->Status));
      return EFI_DEVICE_ERROR;
    }

    Remain -= NumWrite;
    Count += NumWrite;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FlashReadCommand (
  IN     UINT8 *pByteAddress,
  OUT    UINT8 *Byte,
  IN OUT UINTN *Length
  )
{
  EFI_MM_COMMUNICATE_SPINOR_RES *MmSpiNorRes;
  EFI_STATUS                    Status;
  UINT64                        MmData[5];
  UINTN                         Remain, Size, NumRead;
  UINTN                         Count = 0;

  ASSERT (pByteAddress != NULL);
  ASSERT (Byte != NULL);
  ASSERT (Length != NULL);

  Remain = *Length;
  while (Remain > 0) {
    NumRead = (Remain > EFI_MM_MAX_TMP_BUF_SIZE) ? EFI_MM_MAX_TMP_BUF_SIZE : Remain;

    MmData[0] = MM_SPINOR_FUNC_READ;
    MmData[1] = (UINT64)pByteAddress;
    MmData[2] = NumRead;
    MmData[3] = (UINT64)ConvertVirtualToPhysical (Byte + Count, NumRead);

    Status = UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof (MmData));
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
    Status = FlashMmCommunicate (
               mCommBuffer,
               &Size
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mCommBuffer->PayLoad;
    if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
      DEBUG ((DEBUG_ERROR, "Flash Read: Device error %llx\n", MmSpiNorRes->Status));
      return EFI_DEVICE_ERROR;
    }

    ConvertPhysicaltoVirtual (Byte + Count, NumRead);
    Remain -= NumRead;
    Count += NumRead;
  }

  return EFI_SUCCESS;
}
