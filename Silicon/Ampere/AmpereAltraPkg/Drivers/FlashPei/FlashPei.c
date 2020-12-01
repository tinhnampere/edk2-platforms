/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Uefi/UefiSpec.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/MmCommunicationLib.h>
#include <MmLib.h>

// Convert to string
#define _STR(x)          #x

// Make sure the argument is expanded before converting to string
#define STR(x)          _STR(x)

// Use dynamic UEFI_UUID of each build time
#define UEFI_UUID_BUILD      STR(UEFI_UUID)

EFI_MM_COMM_REQUEST mEfiMmSpiNorReq;

STATIC CHAR8 mBuildUuid[sizeof (UEFI_UUID_BUILD)];
STATIC CHAR8 mStoredUuid[sizeof (UEFI_UUID_BUILD)];

STATIC
EFI_STATUS
UefiMmCreateSpiNorReq (
  VOID *Data,
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

/**
  Entry point function for the PEIM

  @param FileHandle      Handle of the file being invoked.
  @param PeiServices     Describes the list of possible PEI Services.

  @return EFI_SUCCESS    If we installed our PPI

**/
EFI_STATUS
EFIAPI
FlashPeiEntryPoint (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  UINT64                               FWNvRamStartOffset;
  EFI_STATUS                           Status;
  EFI_MM_COMMUNICATE_SPINOR_RES        *MmSpiNorRes;
  EFI_MM_COMMUNICATE_SPINOR_NVINFO_RES *MmSpiNorNVInfoRes;
  UINT64                               MmData[5];
  UINTN                                Size;
  VOID                                 *NvRamAddress;
  UINTN                                NvRamSize;

#if defined(RAM_BLOCKIO_START_ADDRESS) && defined(RAM_BLOCKIO_SIZE)
  EFI_MM_COMMUNICATE_SPINOR_NVINFO_RES *MmSpiNorNV2InfoRes;
  UINT64                               NV2Base, NV2Size;
#endif

  NvRamAddress = (VOID *) PcdGet64 (PcdFlashNvStorageVariableBase64);
  NvRamSize = FixedPcdGet32 (PcdFlashNvStorageVariableSize) +
              FixedPcdGet32 (PcdFlashNvStorageFtwWorkingSize) +
              FixedPcdGet32 (PcdFlashNvStorageFtwSpareSize);

  /* Find out about the start offset of NVRAM to be passed to SMC */
  ZeroMem ((VOID *)MmData, sizeof (MmData));
  MmData[0] = MM_SPINOR_FUNC_GET_NVRAM_INFO;
  UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof(MmData));

  Size = sizeof(EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof(MmData);
  Status = MmCommunicationCommunicate (
             (VOID *) &mEfiMmSpiNorReq,
             &Size
             );
  ASSERT_EFI_ERROR(Status);

  MmSpiNorNVInfoRes = (EFI_MM_COMMUNICATE_SPINOR_NVINFO_RES *)&mEfiMmSpiNorReq.PayLoad;
  if (MmSpiNorNVInfoRes->Status != MM_SPINOR_RES_SUCCESS) {
    /* Old FW so just exit */
    return EFI_SUCCESS;
  }
  FWNvRamStartOffset = MmSpiNorNVInfoRes->NVBase;

  CopyMem ((VOID *) mBuildUuid, (VOID *) UEFI_UUID_BUILD, sizeof (UEFI_UUID_BUILD));
  if (MmSpiNorNVInfoRes->NVSize < (NvRamSize * 2 + sizeof (mBuildUuid))) {
    /* NVRAM size provided by FW is not enough */
    return EFI_INVALID_PARAMETER;
  }

  /* We stored BIOS UUID build at the offset NVRAM_SIZE * 2 */
  ZeroMem ((VOID *)MmData, sizeof (MmData));
  MmData[0] = MM_SPINOR_FUNC_READ;
  MmData[1] = (UINT64) (FWNvRamStartOffset + NvRamSize * 2);
  MmData[2] = (UINT64) sizeof (mStoredUuid);
  MmData[3] = (UINT64) mStoredUuid;
  UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof(MmData));

  Size = sizeof(EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof(MmData);
  Status = MmCommunicationCommunicate (
             (VOID *) &mEfiMmSpiNorReq,
             &Size
             );
  ASSERT_EFI_ERROR(Status);

  MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mEfiMmSpiNorReq.PayLoad;
  if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
    return Status;
  }

  if (CompareMem ((VOID *) mStoredUuid, (VOID *) mBuildUuid, sizeof (mBuildUuid))) {
    ZeroMem ((VOID *)MmData, sizeof (MmData));
    MmData[0] = MM_SPINOR_FUNC_ERASE;
    MmData[1] = (UINT64) FWNvRamStartOffset;
    MmData[2] = (UINT64) (NvRamSize * 2);
    UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof(MmData));

    Size = sizeof(EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof(MmData);
    Status = MmCommunicationCommunicate (
               (VOID *) &mEfiMmSpiNorReq,
               &Size
               );
    ASSERT_EFI_ERROR (Status);

    MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mEfiMmSpiNorReq.PayLoad;
    if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
      return Status;
    }

    ZeroMem ((VOID *)MmData, sizeof (MmData));
    MmData[0] = MM_SPINOR_FUNC_WRITE;
    MmData[1] = (UINT64) FWNvRamStartOffset;
    MmData[2] = (UINT64) (NvRamSize * 2);
    MmData[3] = (UINT64) NvRamAddress;
    UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof(MmData));

    Size = sizeof(EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof(MmData);
    Status = MmCommunicationCommunicate (
               (VOID *) &mEfiMmSpiNorReq,
               &Size
               );
    ASSERT_EFI_ERROR (Status);

    MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mEfiMmSpiNorReq.PayLoad;
    if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
      return Status;
    }

    /* Update UUID */
    ZeroMem ((VOID *)MmData, sizeof (MmData));
    MmData[0] = MM_SPINOR_FUNC_ERASE;
    MmData[1] = (UINT64) (FWNvRamStartOffset + NvRamSize * 2);
    MmData[2] = (UINT64) sizeof (mBuildUuid);
    UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof(MmData));

    Size = sizeof(EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof(MmData);
    Status = MmCommunicationCommunicate(
               (VOID *) &mEfiMmSpiNorReq,
               &Size
               );
    ASSERT_EFI_ERROR(Status);

    MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mEfiMmSpiNorReq.PayLoad;
    if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
      return Status;
    }

    ZeroMem ((VOID *)MmData, sizeof (MmData));
    MmData[0] = MM_SPINOR_FUNC_WRITE;
    MmData[1] = (UINT64) (FWNvRamStartOffset + NvRamSize * 2);
    MmData[2] = (UINT64) sizeof (mBuildUuid);
    MmData[3] = (UINT64) mBuildUuid;
    UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof(MmData));

    Size = sizeof(EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof(MmData);
    Status = MmCommunicationCommunicate (
               (VOID *) &mEfiMmSpiNorReq,
               &Size
               );
    ASSERT_EFI_ERROR (Status);

    MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mEfiMmSpiNorReq.PayLoad;
    if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
      return Status;
    }
    DEBUG((DEBUG_INFO, "UUID Changed, Update Storage with FV NVRAM\n"));

    /* Indicate that NVRAM was cleared */
    PcdSetBoolS (PcdNvramErased, TRUE);
  } else {
    /* Copy the stored NVRAM to RAM */
    ZeroMem ((VOID *)MmData, sizeof (MmData));
    MmData[0] = MM_SPINOR_FUNC_READ;
    MmData[1] = (UINT64) FWNvRamStartOffset;
    MmData[2] = (UINT64) (NvRamSize * 2);
    MmData[3] = (UINT64) NvRamAddress;
    UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof(MmData));

    Size = sizeof(EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof(MmData);
    Status = MmCommunicationCommunicate (
               (VOID *) &mEfiMmSpiNorReq,
               &Size
               );
    ASSERT_EFI_ERROR (Status);

    MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mEfiMmSpiNorReq.PayLoad;
    if (MmSpiNorRes->Status != MM_SPINOR_RES_SUCCESS) {
      return Status;
    }
    DEBUG ((DEBUG_INFO, "Identical UUID, copy stored NVRAM to RAM\n"));
  }

#if defined(RAM_BLOCKIO_START_ADDRESS) && defined(RAM_BLOCKIO_SIZE)
  /* Find out about the start offset of NVRAM2 to be passed to SMC */
  ZeroMem ((VOID *)MmData, sizeof (MmData));
  MmData[0] = MM_SPINOR_FUNC_GET_NVRAM2_INFO;
  UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof(MmData));

  Size = sizeof(EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof(MmData);
  Status = MmCommunicationCommunicate (
             (VOID *) &mEfiMmSpiNorReq,
             &Size
             );
  ASSERT_EFI_ERROR (Status);

  MmSpiNorNV2InfoRes = (EFI_MM_COMMUNICATE_SPINOR_NVINFO_RES *)&mEfiMmSpiNorReq.PayLoad;
  if (MmSpiNorNV2InfoRes->Status == MM_SPINOR_RES_SUCCESS)
  {
    NV2Base = MmSpiNorNV2InfoRes->NVBase;
    NV2Size = MmSpiNorNV2InfoRes->NVSize;
    /* Make sure the requested size is smaller than allocated */
    if (RAM_BLOCKIO_SIZE <= NV2Size) {
      /* Copy the ramdisk image to RAM */
      ZeroMem ((VOID *)MmData, sizeof (MmData));
      MmData[0] = MM_SPINOR_FUNC_READ;
      MmData[1] = (UINT64) NV2Base; /* Start virtual address */
      MmData[2] = (UINT64) RAM_BLOCKIO_SIZE;
      MmData[3] = (UINT64) RAM_BLOCKIO_START_ADDRESS;
      UefiMmCreateSpiNorReq ((VOID *)&MmData, sizeof(MmData));

      Size = sizeof(EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof(MmData);
      Status = MmCommunicationCommunicate (
                 (VOID *)&mEfiMmSpiNorReq,
                 &Size
                 );
      ASSERT_EFI_ERROR(Status);

      MmSpiNorRes = (EFI_MM_COMMUNICATE_SPINOR_RES *)&mEfiMmSpiNorReq.PayLoad;
      ASSERT (MmSpiNorRes->Status == MM_SPINOR_RES_SUCCESS);
    }

    BuildMemoryAllocationHob ((EFI_PHYSICAL_ADDRESS) RAM_BLOCKIO_START_ADDRESS,
      EFI_SIZE_TO_PAGES (RAM_BLOCKIO_SIZE) * EFI_PAGE_SIZE, EfiLoaderData);
  }
#endif

  return EFI_SUCCESS;
}
