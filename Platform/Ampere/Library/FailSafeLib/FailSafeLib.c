/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiRuntimeLib.h>
#include <PiPei.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/HobLib.h>
#include <Library/FailSafeLib.h>
#include <Library/NVParamLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/MmCommunication.h>
#include <Platform/Ac01.h>

#define EFI_MM_MAX_PAYLOAD_U64_E  10
#define EFI_MM_MAX_PAYLOAD_SIZE   (EFI_MM_MAX_PAYLOAD_U64_E * sizeof (UINT64))

EFI_MM_COMMUNICATION_PROTOCOL *mFlashLibMmCommProtocol = NULL;

typedef struct {
  /* Allows for disambiguation of the message format */
  EFI_GUID HeaderGuid;
  /*
   * Describes the size of Data (in bytes) and does not include the size
   * of the header
   */
  UINTN MsgLength;
} EFI_MM_COMM_HEADER_NOPAYLOAD;

typedef struct {
  UINT64 Data[EFI_MM_MAX_PAYLOAD_U64_E];
} EFI_MM_COMM_SPINOR_PAYLOAD;

typedef struct {
  EFI_MM_COMM_HEADER_NOPAYLOAD EfiMmHdr;
  EFI_MM_COMM_SPINOR_PAYLOAD   PayLoad;
} EFI_MM_COMM_REQUEST;

typedef struct {
  UINT64 Status;
} EFI_MM_COMMUNICATE_SPINOR_RES;

typedef struct {
  UINT64 Status;
  UINT64 FailSafeBase;
  UINT64 FailSafeSize;
} EFI_MM_COMMUNICATE_SPINOR_FAILSAFE_INFO_RES;

EFI_MM_COMM_REQUEST mEfiMmSpiNorReq;

typedef struct  {
  UINT32    NumRetry1;
  UINT32    NumRetry2;
  UINT32    MaxRetry;
  UINT8     Status;
  /*
   * Byte[3]: Reserved
   * Byte[2]: Slave MCU Failure Mask
   * Byte[1]: Reserved
   * Byte[0]: Master MCU Failure Mask
   */
  UINT32    MCUFailsMask;
  UINT16    CRC16;
} __attribute__((packed, aligned(4))) FailsafeCtx_t;

STATIC
EFI_STATUS
UefiMmCreateSpiNorReq (
  VOID    *Data,
  UINT64  Size
  )
{
  CopyGuid (&mEfiMmSpiNorReq.EfiMmHdr.HeaderGuid, &gSpiNorMmGuid);
  mEfiMmSpiNorReq.EfiMmHdr.MsgLength = Size;

  if (Size != 0) {
    ASSERT (Data);
    ASSERT (Size <= EFI_MM_MAX_PAYLOAD_SIZE);

    CopyMem (mEfiMmSpiNorReq.PayLoad.Data, Data, Size);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FailSafeGetRegionInfo (
  UINT64 *Offset,
  UINT64 *Size
  )
{
  EFI_MM_COMMUNICATE_SPINOR_FAILSAFE_INFO_RES *MmSpiNorFailSafeInfoRes;
  UINT64                                      MmData[5];
  UINTN                                       DataSize;
  EFI_STATUS                                  Status;

  if (mFlashLibMmCommProtocol == NULL) {
    Status = gBS->LocateProtocol (
                    &gEfiMmCommunicationProtocolGuid,
                    NULL,
                    (VOID **) &mFlashLibMmCommProtocol
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Can't locate gEfiMmCommunicationProtocolGuid\n", __FUNCTION__));
      return Status;
    }
  }

  MmData[0] = MM_SPINOR_FUNC_GET_FAILSAFE_INFO;
  UefiMmCreateSpiNorReq ((VOID *) &MmData, sizeof (MmData));

  DataSize = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = mFlashLibMmCommProtocol->Communicate (
                                      mFlashLibMmCommProtocol,
                                      (VOID *) &mEfiMmSpiNorReq,
                                      &DataSize
                                      );
  ASSERT_EFI_ERROR (Status);

  MmSpiNorFailSafeInfoRes = (EFI_MM_COMMUNICATE_SPINOR_FAILSAFE_INFO_RES *) &mEfiMmSpiNorReq.PayLoad;
  if (MmSpiNorFailSafeInfoRes->Status != MM_SPINOR_RES_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: Get flash information failed: %d\n",
            __FUNCTION__,
            MmSpiNorFailSafeInfoRes->Status));
    return EFI_DEVICE_ERROR;
  }

  *Offset = MmSpiNorFailSafeInfoRes->FailSafeBase;
  *Size   = MmSpiNorFailSafeInfoRes->FailSafeSize;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FailSafeBootSuccessfully (VOID)
{
  EFI_MM_COMMUNICATE_SPINOR_RES               *MmSpiNorRes;
  UINT64                                      MmData[5];
  UINTN                                       Size;
  EFI_STATUS                                  Status;
  UINT64                                      FailSafeStartOffset;
  UINT64                                      FailSafeSize;

  Status = FailSafeGetRegionInfo (&FailSafeStartOffset, &FailSafeSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get context region information\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  ASSERT (mFlashLibMmCommProtocol != NULL);

  MmData[0] = MM_SPINOR_FUNC_ERASE;
  MmData[1] = FailSafeStartOffset;
  MmData[2] = FailSafeSize;
  UefiMmCreateSpiNorReq ((VOID *) &MmData, sizeof (MmData));

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = mFlashLibMmCommProtocol->Communicate (
                                      mFlashLibMmCommProtocol,
                                      (VOID *) &mEfiMmSpiNorReq,
                                      &Size
                                      );
  ASSERT_EFI_ERROR (Status);

  MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *) &mEfiMmSpiNorReq.PayLoad;
  if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: erase context failed: %d\n",
            __FUNCTION__,
            MmSpiNorRes->Status));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

UINT8
FailSafeValidStatus (
  IN UINT8 Status
  )
{
  if ((Status == FAILSAFE_BOOT_NORMAL) ||
      (Status == FAILSAFE_BOOT_LAST_KNOWN_SETTINGS) ||
      (Status == FAILSAFE_BOOT_DEFAULT_SETTINGS) ||
      (Status == FAILSAFE_BOOT_SUCCESSFUL)) {
    return 1;
  }

  return 0;
}

UINT64
EFIAPI
FailSafeGetStatus (VOID)
{
  EFI_MM_COMMUNICATE_SPINOR_RES       *MmSpiNorRes;
  UINT64                              MmData[5];
  UINTN                               Size;
  EFI_STATUS                          Status;
  UINT64                              FailSafeStartOffset;
  UINT64                              FailSafeSize;
  FailsafeCtx_t                       FailsafeBuf;

  Status = FailSafeGetRegionInfo (&FailSafeStartOffset, &FailSafeSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get region information\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  ASSERT (mFlashLibMmCommProtocol != NULL);

  MmData[0] = MM_SPINOR_FUNC_READ;
  MmData[1] = FailSafeStartOffset;
  MmData[2] = (UINT64) sizeof (FailsafeCtx_t);
  MmData[3] = (UINT64) &FailsafeBuf;
  UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof(MmData));

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = mFlashLibMmCommProtocol->Communicate (
                                      mFlashLibMmCommProtocol,
                                      (VOID *) &mEfiMmSpiNorReq,
                                      &Size
                                      );
  ASSERT_EFI_ERROR (Status);

  MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *) &mEfiMmSpiNorReq.PayLoad;
  if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "%a: read context failed: %d\n",
            __FUNCTION__,
            MmSpiNorRes->Status));
    return EFI_DEVICE_ERROR;
  }

  if (FailSafeValidStatus (FailsafeBuf.Status) == 0) {
    FailsafeBuf.Status = FAILSAFE_BOOT_NORMAL;
  }

  return FailsafeBuf.Status;
}

EFI_STATUS
EFIAPI
FailSafeTestBootFailure (VOID)
{
  EFI_STATUS    Status;
  UINT32        Value = 0;

  /*
   * Simulate UEFI boot failure due to config wrong NVPARAM for
   * testing failsafe feature
   */
  Status = NVParamGet (NV_UEFI_FAILURE_FAILSAFE_OFFSET, NV_PERM_ALL, &Value);
  if (!EFI_ERROR (Status) && (Value == 1)) {
     while (1);
  }

  return EFI_SUCCESS;
}
