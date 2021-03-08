/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/AcpiPccLib.h>
#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Platform/Ac01.h>

#define PCC_NULL_MSG             0x0F000000

STATIC UINT64 PccSharedMemAddr = 0;

STATIC EFI_STATUS
AcpiPccGetSharedMemAddr (
  IN UINT32                      Socket,
  IN UINT32                      Subspace,
  OUT VOID                       **AcpiPcct
  )
{
  if ((Subspace >= PCC_MAX_SUBSPACES_PER_SOCKET)
    || (Socket >= PLATFORM_CPU_MAX_SOCKET)) {
    return EFI_INVALID_PARAMETER;
  }

  if (PccSharedMemAddr == 0) {
    return EFI_NOT_READY;
  }

  *AcpiPcct = (VOID *) (PccSharedMemAddr + PCC_SUBSPACE_SHARED_MEM_SIZE *
                       (Subspace + PCC_MAX_SUBSPACES_PER_SOCKET * Socket));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AcpiPccSendMsg (
  IN UINT32                      Socket,
  IN UINT32                      Subspace,
  IN VOID                        *MsgBuf,
  IN UINT32                      Length
  )
{
  INTN TimeoutCnt                = PCC_NOMINAL_LATENCY / PCC_CMD_POLL_UDELAY;
  VOID                           *CommunicationSpacePtr;
  struct ACPI_PCCT_SHARED_MEMORY *AcpiPcct;
  EFI_STATUS                     Status;
  UINT32                         PccMsg;

  if ((Subspace >= PCC_MAX_SUBSPACES_PER_SOCKET)
    || (Socket >= PLATFORM_CPU_MAX_SOCKET)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AcpiPccGetSharedMemAddr (Socket, Subspace, (VOID **) &AcpiPcct);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CommunicationSpacePtr = AcpiPcct + 1;

  /* Write Data into Communication Space Region */
  CopyMem (CommunicationSpacePtr, MsgBuf, Length);
  /* Flip CMD_COMPLETE bit */
  AcpiPcct->StatusData.StatusT.CommandComplete = 0;
  /* PCC signature */
  AcpiPcct->Signature = PCC_SIGNATURE_MASK | Subspace;
  /* Ring the Doorbell */
  PccMsg = PCC_MSG;
  /* Store the upper address (Bit 40-43) of PCC shared memory */
  PccMsg |= ((UINT64) AcpiPcct >> 40) & PCP_MSG_UPPER_ADDR_MASK;
  if (Subspace < PMPRO_MAX_DB) {
    MmioWrite32 (PMPRO_DBx_REG (Socket, Subspace, DB_OUT), PccMsg);
  } else {
    MmioWrite32 (SMPRO_DBx_REG (Socket, Subspace - PMPRO_MAX_DB, DB_OUT),
                PccMsg);
  }

  /* Polling CMD_COMPLETE bit */
  while (AcpiPcct->StatusData.StatusT.CommandComplete != 1) {
    if (--TimeoutCnt <= 0) {
      return EFI_TIMEOUT;
    }
    MicroSecondDelay (PCC_CMD_POLL_UDELAY);
  };

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AcpiPccUnmaskInt (
  IN UINT32                      Socket,
  IN UINT32                      Subspace
  )
{
  if ((Subspace >= PCC_MAX_SUBSPACES_PER_SOCKET)
    || (Socket >= PLATFORM_CPU_MAX_SOCKET)) {
    return EFI_INVALID_PARAMETER;
  }

  /* Unmask Interrupt */
  if (Subspace < PMPRO_MAX_DB) {
    MmioWrite32 (
      PMPRO_DBx_REG (Socket, Subspace, DB_STATUSMASK),
      ~DB_AVAIL_MASK
      );
  } else {
    MmioWrite32 (
      SMPRO_DBx_REG (Socket, Subspace - PMPRO_MAX_DB, DB_STATUSMASK),
      ~DB_AVAIL_MASK
      );
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AcpiPccSyncSharedMemAddr (
  IN UINT32                      Socket,
  IN UINT32                      Subspace
  )
{
  UINT32 PccData;

  if ((Subspace >= PCC_MAX_SUBSPACES_PER_SOCKET)
    || (Socket >= PLATFORM_CPU_MAX_SOCKET)) {
    return EFI_INVALID_PARAMETER;
  }

  /*
   * Advertise shared memory address to Platform (SMPro/PMPro)
   * by ring the doorbell with dummy PCC message
   */
  PccData = PCC_NULL_MSG;

  return AcpiPccSendMsg (Socket, Subspace, &PccData, 4);
}

EFI_STATUS
EFIAPI
AcpiPccSharedMemInit (
  IN UINT32                      Socket,
  IN UINT32                      Subspace
  )
{
  struct ACPI_PCCT_SHARED_MEMORY *AcpiPcct;
  EFI_STATUS                     Status;

  if ((Subspace >= PCC_MAX_SUBSPACES_PER_SOCKET)
    || (Socket >= PLATFORM_CPU_MAX_SOCKET)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AcpiPccGetSharedMemAddr (Socket, Subspace, (VOID **) &AcpiPcct);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Set Shared Memory address into DB OUT register */
  if (Subspace < PMPRO_MAX_DB) {
    MmioWrite32 (
      PMPRO_DBx_REG (Socket, Subspace, DB_OUT0),
      (UINT32) ((UINT64) AcpiPcct >> 8)
      );
  } else {
    MmioWrite32 (
      SMPRO_DBx_REG(Socket, Subspace - PMPRO_MAX_DB, DB_OUT0),
      (UINT32) ((UINT64) AcpiPcct >> 8)
      );
  }

  /* Init shared memory for each PCC subspaces */
  SetMem (
    (VOID *) AcpiPcct,
    sizeof (struct ACPI_PCCT_SHARED_MEMORY) + PCC_MSG_SIZE,
    0x0
    );
  AcpiPcct->StatusData.StatusT.CommandComplete = 1;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AcpiPccSharedMemInitV2 (
  IN UINT32                      Socket,
  IN UINT32                      Subspace
  )
{
  struct ACPI_PCCT_SHARED_MEMORY *AcpiPcct;
  EFI_STATUS                     Status;
  UINT32                         AlignBit;

  if ((Subspace >= PCC_MAX_SUBSPACES_PER_SOCKET)
    || (Socket >= PLATFORM_CPU_MAX_SOCKET)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AcpiPccGetSharedMemAddr (Socket, Subspace, (VOID **) &AcpiPcct);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((PCC_MSG & PCC_256_ALIGN_ADDR) != 0) {
    AlignBit = 8;
  }

  /* Set Shared Memory address into DB OUT register */
  if (Subspace < PMPRO_MAX_DB) {
    MmioWrite32 (
      PMPRO_DBx_REG (Socket, Subspace, DB_OUT0),
      (UINT32) ((UINT64) AcpiPcct >> AlignBit)
      );

    MmioWrite32 (
      (PMPRO_DBx_REG (Socket, Subspace, DB_OUT1)),
      (UINT32) ((UINT64) AcpiPcct >> (32 + AlignBit))
      );
  } else {
    MmioWrite32 (
      SMPRO_DBx_REG (Socket, Subspace - PMPRO_MAX_DB, DB_OUT0),
      (UINT32) ((UINT64) AcpiPcct >> AlignBit)
      );

    MmioWrite32 (
      SMPRO_DBx_REG (Socket, Subspace - PMPRO_MAX_DB, DB_OUT1),
      (UINT32) ((UINT64) AcpiPcct >> (32 + AlignBit))
      );
  }

  /* Init shared memory for each PCC subspaces */
  SetMem (
    (VOID *) AcpiPcct,
    sizeof (struct ACPI_PCCT_SHARED_MEMORY) + PCC_MSG_SIZE,
    0x0
    );
  AcpiPcct->StatusData.StatusT.CommandComplete = 1;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AcpiIppPccIsSupported (
  VOID
  )
{
  EFI_STATUS                     Status;

  /* Send a PCC NULL command to check if IPP supports PCC request */
  AcpiPccSharedMemInit (0, 0);

  Status = AcpiPccSyncSharedMemAddr (0, 0);
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AcpiPccAllocSharedMemory (
  OUT UINT64                     *PccSharedMemPointer,
  IN UINT32                      SubspaceNum
  )
{
  EFI_STATUS                     Status;

  if (SubspaceNum > PCC_MAX_SUBSPACES) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->AllocatePages (
                  AllocateAnyPages,
                  EfiRuntimeServicesData,
                  EFI_SIZE_TO_PAGES (PCC_SUBSPACE_SHARED_MEM_SIZE * SubspaceNum),
                  &PccSharedMemAddr
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate PCC shared memory\n"));
    return Status;
  }

  *PccSharedMemPointer = PccSharedMemAddr;

  return EFI_SUCCESS;
}

VOID
EFIAPI
AcpiPccFreeSharedMemory (
  OUT UINT64                     *PccSharedMemPointer,
  IN UINT32                      SubspaceNum
  )
{
  if (SubspaceNum > PCC_MAX_SUBSPACES) {
    return;
  }

  gBS->FreePages (
         *PccSharedMemPointer,
         EFI_SIZE_TO_PAGES (PCC_SUBSPACE_SHARED_MEM_SIZE * SubspaceNum)
         );

  PccSharedMemAddr = 0;
}
