/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Uefi.h>

#include <Library/ArmSmcLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FailSafeLib.h>
#include <Library/HobLib.h>
#include <Library/NVParamLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Platform/Ac01.h>
#include <Protocol/MmCommunication2.h>

#include <NVParamDef.h>

#define EFI_MM_MAX_PAYLOAD_U64_E  10
#define EFI_MM_MAX_PAYLOAD_SIZE   (EFI_MM_MAX_PAYLOAD_U64_E * sizeof (UINT64))

EFI_MM_COMMUNICATION2_PROTOCOL *mFlashLibMmCommProtocol = NULL;

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

#pragma pack(1)
typedef struct {
  UINT8  ImgMajorVer;
  UINT8  ImgMinorVer;
  UINT32 NumRetry1;
  UINT32 NumRetry2;
  UINT32 MaxRetry;
  UINT8  Status;
  /*
   * Byte[3]: Reserved
   * Byte[2]: Slave MCU Failure Mask
   * Byte[1]: Reserved
   * Byte[0]: Master MCU Failure Mask
   */
  UINT32 MCUFailsMask;
  UINT16 CRC16;
  UINT8  Reserved[3];
} FAIL_SAFE_CONTEXT;

#pragma pack()

STATIC
EFI_STATUS
UefiMmCreateSpiNorReq (
  VOID   *Data,
  UINT64 Size
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

STATIC
INTN
CheckCrc16 (
  UINT8 *Pointer,
  INTN  Count
  )
{
  INTN Crc = 0;
  INTN Index;

  while (--Count >= 0) {
    Crc = Crc ^ (INTN)*Pointer++ << 8;
    for (Index = 0; Index < 8; ++Index) {
      if ((Crc & 0x8000) != 0) {
        Crc = Crc << 1 ^ 0x1021;
      } else {
        Crc = Crc << 1;
      }
    }
  }

  return Crc & 0xFFFF;
}

BOOLEAN
FailSafeValidCRC (
  FAIL_SAFE_CONTEXT *FailSafeBuf
  )
{
  UINT8  Valid;
  UINT16 Crc;
  UINT32 Len;

  Len = sizeof (FAIL_SAFE_CONTEXT);
  Crc = FailSafeBuf->CRC16;
  FailSafeBuf->CRC16 = 0;

  Valid = (Crc == CheckCrc16 ((UINT8 *)FailSafeBuf, Len));
  FailSafeBuf->CRC16 = Crc;

  return Valid;
}

BOOLEAN
FailSafeFailureStatus (
  UINT8 Status
  )
{
  if ((Status == FAILSAFE_BOOT_LAST_KNOWN_SETTINGS) ||
      (Status == FAILSAFE_BOOT_DEFAULT_SETTINGS) ||
      (Status == FAILSAFE_BOOT_DDR_DOWNGRADE))
  {
    return TRUE;
  }

  return FALSE;
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
                    &gEfiMmCommunication2ProtocolGuid,
                    NULL,
                    (VOID **)&mFlashLibMmCommProtocol
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Can't locate gEfiMmCommunication2ProtocolGuid\n", __FUNCTION__));
      return Status;
    }
  }

  MmData[0] = MM_SPINOR_FUNC_GET_FAILSAFE_INFO;
  UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof (MmData));

  DataSize = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = mFlashLibMmCommProtocol->Communicate (
                                      mFlashLibMmCommProtocol,
                                      (VOID *)&mEfiMmSpiNorReq,
                                      (VOID *)&mEfiMmSpiNorReq,
                                      &DataSize
                                      );
  ASSERT_EFI_ERROR (Status);

  MmSpiNorFailSafeInfoRes = (EFI_MM_COMMUNICATE_SPINOR_FAILSAFE_INFO_RES *)&mEfiMmSpiNorReq.PayLoad;
  if (MmSpiNorFailSafeInfoRes->Status != MM_SPINOR_RES_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Get flash information failed: 0x%llx\n",
      __FUNCTION__,
      MmSpiNorFailSafeInfoRes->Status
      ));
    return EFI_DEVICE_ERROR;
  }

  *Offset = MmSpiNorFailSafeInfoRes->FailSafeBase;
  *Size   = MmSpiNorFailSafeInfoRes->FailSafeSize;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FailSafeBootSuccessfully (
  VOID
  )
{
  EFI_MM_COMMUNICATE_SPINOR_RES *MmSpiNorRes;
  UINT64                        MmData[5];
  UINTN                         Size;
  EFI_STATUS                    Status;
  UINT64                        FailSafeStartOffset;
  UINT64                        FailSafeSize;
  FAIL_SAFE_CONTEXT             FailSafeBuf;

  Status = FailSafeGetRegionInfo (&FailSafeStartOffset, &FailSafeSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get context region information\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  MmData[0] = MM_SPINOR_FUNC_READ;
  MmData[1] = FailSafeStartOffset;
  MmData[2] = (UINT64)sizeof (FAIL_SAFE_CONTEXT);
  MmData[3] = (UINT64)&FailSafeBuf;
  UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof (MmData));

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = mFlashLibMmCommProtocol->Communicate (
                                      mFlashLibMmCommProtocol,
                                      (VOID *)&mEfiMmSpiNorReq,
                                      (VOID *)&mEfiMmSpiNorReq,
                                      &Size
                                      );
  ASSERT_EFI_ERROR (Status);

  MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mEfiMmSpiNorReq.PayLoad;
  if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Read context failed: 0x%llx\n",
      __FUNCTION__,
      MmSpiNorRes->Status
      ));
    return EFI_DEVICE_ERROR;
  }

  /*
   * If failsafe context is valid, and:
   *    - The status indicate non-failure, then don't clear it
   *    - The status indicate a failure, then go and clear it
   */
  if (FailSafeValidCRC (&FailSafeBuf)
      && !FailSafeFailureStatus (FailSafeBuf.Status)) {
    return EFI_SUCCESS;
  }

  MmData[0] = MM_SPINOR_FUNC_ERASE;
  MmData[1] = FailSafeStartOffset;
  MmData[2] = FailSafeSize;
  UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof (MmData));

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = mFlashLibMmCommProtocol->Communicate (
                                      mFlashLibMmCommProtocol,
                                      (VOID *)&mEfiMmSpiNorReq,
                                      (VOID *)&mEfiMmSpiNorReq,
                                      &Size
                                      );
  ASSERT_EFI_ERROR (Status);

  MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mEfiMmSpiNorReq.PayLoad;
  if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Erase context failed: 0x%llx\n",
      __FUNCTION__,
      MmSpiNorRes->Status
      ));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FailSafeTestBootFailure (
  VOID
  )
{
  EFI_STATUS Status;
  UINT32     Value = 0;

  /*
   * Simulate UEFI boot failure due to config wrong NVPARAM for
   * testing failsafe feature
   */
  Status = NVParamGet (NV_SI_UEFI_FAILURE_FAILSAFE, NV_PERM_ALL, &Value);
  if (!EFI_ERROR (Status) && (Value == 1)) {
    while (1) {
    }
  }

  return EFI_SUCCESS;
}
