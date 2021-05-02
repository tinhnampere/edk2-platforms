/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ACPI_PCC_LIB_H_
#define ACPI_PCC_LIB_H_

#include <Library/MailboxInterfaceLib.h>
#include <Library/SystemFirmwareInterfaceLib.h>

// Send a message with dummy payload is to advertise the shared memory address
#define DB_PCC_PAYLOAD_DUMMY         0x0F000000

#define DB_PCC_MSG_PAYLOAD_SIZE      12 // Number of Bytes

//
// ACPI Platform Communication Channel (PCC)
//
#define ACPI_PCC_SUBSPACE_SHARED_MEM_SIGNATURE  0x50434300 // "PCC"
#define ACPI_PCC_SUBSPACE_SHARED_MEM_SIZE       0x4000     // Number of Bytes

//
// Reserved Doorbell Mask
// Bit 0 --> 31 correspond to Doorbell 0 --> 31
//
// List of reserved Doorbells
//  1. Doorbell 4: PCIe Hot-plug
//
#define ACPI_PCC_AVAILABLE_DOORBELL_MASK        0xEFFFEFFF
#define ACPI_PCC_NUMBER_OF_RESERVED_DOORBELLS   1

// Supported doorbells in the platform
#define ACPI_PCC_MAX_DOORBELL              (NUMBER_OF_DOORBELLS_PER_SOCKET * PLATFORM_CPU_MAX_SOCKET)

// Valid doorbells for use
#define ACPI_PCC_MAX_SUBPACE_PER_SOCKET    (NUMBER_OF_DOORBELLS_PER_SOCKET - ACPI_PCC_NUMBER_OF_RESERVED_DOORBELLS)
#define ACPI_PCC_MAX_SUBPACE               (ACPI_PCC_MAX_SUBPACE_PER_SOCKET * PLATFORM_CPU_MAX_SOCKET)

#define ACPI_PCC_NOMINAL_LATENCY_US                1000 // us
#define ACPI_PCC_MAX_PERIODIC_ACCESS_RATE          0    // no limitation
#define ACPI_PCC_MIN_REQ_TURNAROUND_TIME_US        0

// Polling interval for PCC Command Complete
#define ACPI_PCC_COMMAND_POLL_INTERVAL_US          10

#define ACPI_PCC_COMMAND_POLL_COUNT  (ACPI_PCC_NOMINAL_LATENCY_US / ACPI_PCC_COMMAND_POLL_INTERVAL_US)

//
// PCC subspace 2 (PMpro Doorbell Channel 2) is used for ACPI CPPC
//
#define ACPI_PCC_CPPC_DOORBELL_ID                  (PMproDoorbellChannel2)

#define ACPI_PCC_CPPC_NOMINAL_LATENCY_US           100
#define ACPI_PCC_CPPC_MIN_REQ_TURNAROUND_TIME_US   110


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
  OUT EFI_PHYSICAL_ADDRESS *PccSharedMemPointer,
  IN  UINT16               NumberOfSubspaces
  );

/**
  Free the whole shared memory region that is allocated by
  the AcpiPccAllocateSharedMemory() function.

**/
VOID
EFIAPI
AcpiPccFreeSharedMemory (
  VOID
  );

/**
  Send a PCC message to the platform (SMpro/PMpro).

  @param  Socket    The Socket ID.
  @param  Doorbell  The Doorbell index from supported Doorbells per socket.
  @param  Subspace  The Subspace index in the shared memory region.

  @retval EFI_SUCCESS            Send the message successfully.
  @retval EFI_INVALID_PARAMETER  The Socket, Doorbell or Subspace is out of the valid range.
                                 The data buffer is NULL or the size of data buffer is zero.
  @retval EFI_NOT_READY          The shared memory region is NULL.
  @retval EFI_TIMEOUT            Timeout occurred when polling the PCC Command Complete bit.

**/
EFI_STATUS
EFIAPI
AcpiPccSendMessage (
  IN UINT8  Socket,
  IN UINT16 Doorbell,
  IN UINT16 Subspace,
  IN VOID   *DataBuffer,
  IN UINT32 DataSize
  );

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
  );

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
  );

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
  );

#endif /* ACPI_PCC_LIB_H_ */
