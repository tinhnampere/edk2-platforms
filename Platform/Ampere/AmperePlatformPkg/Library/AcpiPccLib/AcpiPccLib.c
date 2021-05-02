/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <IndustryStandard/Acpi.h>
#include <Library/AcpiPccLib.h>
#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Platform/Ac01.h>

STATIC EFI_PHYSICAL_ADDRESS mPccSharedMemoryAddress;
STATIC UINTN                mPccSharedMemorySize;

EFI_STATUS
AcpiPccGetSharedMemoryAddress (
  IN  UINT8  Socket,
  IN  UINT16 Subspace,
  OUT VOID   **SharedMemoryAddress
  )
{
  if (Socket >= PLATFORM_CPU_MAX_SOCKET
      || Subspace >= ACPI_PCC_MAX_SUBPACE)
  {
    return EFI_INVALID_PARAMETER;
  }

  if (mPccSharedMemoryAddress == 0) {
    return EFI_NOT_READY;
  }

  *SharedMemoryAddress = (VOID *)(mPccSharedMemoryAddress + ACPI_PCC_SUBSPACE_SHARED_MEM_SIZE * Subspace);

  return EFI_SUCCESS;
}

/**
  Allocate memory pages for the PCC shared memory region.

  @param  PccSharedMemoryPtr     Pointer to the shared memory address.
  @param  NumberOfSubspaces      Number of subspaces slot in the shared memory region.

  @retval EFI_SUCCESS            Send the message successfully.
  @retval EFI_INVALID_PARAMETER  TheNumberOfSubspaces is out of the valid range.
  @retval Otherwise              Return errors from call to gBS->AllocatePages().

**/
EFI_STATUS
EFIAPI
AcpiPccAllocateSharedMemory (
  OUT EFI_PHYSICAL_ADDRESS *PccSharedMemoryPtr,
  IN  UINT16               NumberOfSubspaces
  )
{
  EFI_STATUS Status;

  if (NumberOfSubspaces > ACPI_PCC_MAX_SUBPACE) {
    return EFI_INVALID_PARAMETER;
  }

  mPccSharedMemorySize = ACPI_PCC_SUBSPACE_SHARED_MEM_SIZE * NumberOfSubspaces;

  Status = gBS->AllocatePages (
                  AllocateAnyPages,
                  EfiRuntimeServicesData,
                  EFI_SIZE_TO_PAGES (mPccSharedMemorySize),
                  &mPccSharedMemoryAddress
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate PCC shared memory\n"));
    mPccSharedMemorySize = 0;
    return Status;
  }

  *PccSharedMemoryPtr = mPccSharedMemoryAddress;

  return EFI_SUCCESS;
}

/**
  Free the whole shared memory region that is allocated by
  the AcpiPccAllocateSharedMemory() function.

**/
VOID
EFIAPI
AcpiPccFreeSharedMemory (
  VOID
  )
{
  if (mPccSharedMemoryAddress != 0 && mPccSharedMemorySize != 0)
  {
    gBS->FreePages (
           mPccSharedMemoryAddress,
           EFI_SIZE_TO_PAGES (mPccSharedMemorySize)
           );

    mPccSharedMemoryAddress = 0;
  }
}

/**
  Initialize the shared memory in the SMpro/PMpro Doorbell handler.
  This function is to advertise the shared memory region address to the platform (SMpro/PMpro).

  @param  Socket    The Socket ID.
  @param  Doorbell  The Doorbell index from supported Doorbells per socket.
  @param  Subspace  The Subspace index in the shared memory region.

  @retval EFI_SUCCESS            Initialize successfully.
  @retval EFI_INVALID_PARAMETER  The Socket, Doorbell or Subspace is out of the valid range.

**/
EFI_STATUS
EFIAPI
AcpiPccInitSharedMemory (
  IN UINT8  Socket,
  IN UINT16 Doorbell,
  IN UINT16 Subspace
  )
{
  EFI_STATUS                                            Status;
  EFI_ACPI_6_3_PCCT_GENERIC_SHARED_MEMORY_REGION_HEADER *PcctSharedMemoryRegion;
  UINT32                                                CommunicationData;
  UINTN                                                 Timeout;

  if (Socket >= PLATFORM_CPU_MAX_SOCKET
      || Doorbell >= NUMBER_OF_DOORBELLS_PER_SOCKET
      || Subspace >= ACPI_PCC_MAX_SUBPACE)
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = AcpiPccGetSharedMemoryAddress (Socket, Subspace, (VOID **)&PcctSharedMemoryRegion);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Zero shared memory region for each PCC subspace
  //
  SetMem (
    (VOID *)PcctSharedMemoryRegion,
    sizeof (EFI_ACPI_6_3_PCCT_GENERIC_SHARED_MEMORY_REGION_HEADER) + DB_PCC_MSG_PAYLOAD_SIZE,
    0
    );

  // Advertise shared memory address to Platform (SMpro/PMpro)
  // by ringing the doorbell with dummy PCC message
  //
  CommunicationData = DB_PCC_PAYLOAD_DUMMY;

  //
  // Write Data into Communication Space Region
  //
  CopyMem ((VOID *)(PcctSharedMemoryRegion + 1), &CommunicationData, sizeof (CommunicationData));

  PcctSharedMemoryRegion->Status.CommandComplete = 0;
  PcctSharedMemoryRegion->Signature = ACPI_PCC_SUBSPACE_SHARED_MEM_SIGNATURE | Subspace;

  Status = MailboxMsgSetPccSharedMem (Socket, Doorbell, TRUE, (UINT64)PcctSharedMemoryRegion);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send mailbox message!\n", __FUNCTION__));
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  //
  // Polling CMD_COMPLETE bit
  //
  Timeout = ACPI_PCC_COMMAND_POLL_COUNT;
  while (PcctSharedMemoryRegion->Status.CommandComplete != 1) {
    if (--Timeout <= 0) {
      DEBUG ((DEBUG_ERROR, "%a - Timeout occurred when polling the PCC Status Complete\n", __FUNCTION__));
      return EFI_TIMEOUT;
    }
    MicroSecondDelay (ACPI_PCC_COMMAND_POLL_INTERVAL_US);
  }

  return EFI_SUCCESS;
}

/**
  Unmask the Doorbell interrupt.

  @param  Socket    The Socket ID.
  @param  Doorbell  The Doorbell index from supported Doorbells per socket.

  @retval EFI_SUCCESS            Unmask the Doorbell interrupt successfully.
  @retval EFI_INVALID_PARAMETER  The Socket or Doorbell is out of the valid range.

**/
EFI_STATUS
EFIAPI
AcpiPccUnmaskDoorbellInterrupt (
  IN UINT8  Socket,
  IN UINT16 Doorbell
  )
{
  return MailboxUnmaskInterrupt (Socket, Doorbell);
}

/**
  Check whether the Doorbell is reserved or not.

  @param  Doorbell   The Doorbell index from supported Doorbells.

  @retval TRUE         The Doorbell is reserved for private use or invalid.
  @retval FALSE        The Doorbell is available.

**/
BOOLEAN
EFIAPI
AcpiPccIsDoorbellReserved (
  IN UINT16 Doorbell
  )
{
  if (Doorbell >= ACPI_PCC_MAX_DOORBELL) {
    ASSERT (FALSE);
    return TRUE;
  }

  if (((1 << Doorbell) & ACPI_PCC_AVAILABLE_DOORBELL_MASK) == 0) {
    //
    // The doorbell is reserved for private use.
    //
    return TRUE;
  }

  return FALSE;
}
