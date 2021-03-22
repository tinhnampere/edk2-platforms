/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/AcpiPccLib.h>
#include "AcpiPlatform.h"

EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS PcctSubspaceTemplate = {
  EFI_ACPI_6_3_PCCT_SUBSPACE_TYPE_2_HW_REDUCED_COMMUNICATIONS,
  sizeof (EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS),
  0,                        // PlatformInterrupt
  0,                        // PlatformInterruptFlags
  0,                        // Reserved
  0,                        // BaseAddress
  0x100,                    // AddressLength
  { 0, 0x20, 0, 0x3, 0x0 }, // DoorbellRegister
  0,                        // DoorbellPreserve
  0x53000040,               // DoorbellWrite
  1,                        // NominalLatency
  1,                        // MaximumPeriodicAccessRate
  1,                        // MinimumRequestTurnaroundTime
  { 0, 0x20, 0, 0x3, 0x0 }, // PlatformInterruptAckRegister
  0,                        // PlatformInterruptAckPreserve
  0x10001,                  // PlatformInterruptAckWrite
};

EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER PcctTableHeaderTemplate = {
  __ACPI_HEADER (
    EFI_ACPI_6_3_PLATFORM_COMMUNICATIONS_CHANNEL_TABLE_SIGNATURE,
    EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER,
    EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_REVISION
    ),
  EFI_ACPI_6_3_PCCT_FLAGS_PLATFORM_INTERRUPT,
};

EFI_STATUS
AcpiPcctInit (
  VOID
  )
{
  UINT8  NumberOfSockets;
  UINT8  Socket;
  UINT16 Doorbell;
  UINT16 Subspace;

  NumberOfSockets = GetNumberOfActiveSockets ();
  Subspace = 0;

  for (Socket = 0; Socket < NumberOfSockets; Socket++) {
    for (Doorbell = 0; Doorbell < NUMBER_OF_DOORBELLS_PER_SOCKET; Doorbell++ ) {
      if (AcpiPccIsDoorbellReserved (Doorbell + NUMBER_OF_DOORBELLS_PER_SOCKET * Socket)) {
        continue;
      }
      AcpiPccInitSharedMemory (Socket, Doorbell, Subspace);
      AcpiPccUnmaskDoorbellInterrupt (Socket, Doorbell);

      Subspace++;
    }
  }

  return EFI_SUCCESS;
}

/**
  Install PCCT table.

  Each socket has 16 PCC subspaces corresponding to 16 Mailbox/Doorbell channels
    0 - 7  : PMpro subspaces
    8 - 15 : SMpro subspaces

  Please note that some SMpro/PMpro Doorbell are reserved for private use.
  The reserved Doorbells are filtered by using the ACPI_PCC_AVAILABLE_DOORBELL_MASK
  and ACPI_PCC_NUMBER_OF_RESERVED_DOORBELLS macro.

**/
EFI_STATUS
AcpiInstallPcctTable (
  VOID
  )
{
  EFI_STATUS                                               Status;
  EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER *PcctTablePointer;
  EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS   *PcctEntryPointer;
  EFI_PHYSICAL_ADDRESS                                     PccSharedMemPointer;
  EFI_ACPI_TABLE_PROTOCOL                                  *AcpiTableProtocol;
  UINTN                                                    PcctTableKey;
  UINTN                                                    ConfigTableIndex;
  UINT8                                                    NumberOfSockets;
  UINT8                                                    Socket;
  UINT16                                                   Doorbell;
  UINT16                                                   Subspace;
  UINT16                                                   NumberOfSubspaces;
  UINTN                                                    Size;
  UINTN                                                    DoorbellAddress;

  Subspace = 0;
  NumberOfSockets = GetNumberOfActiveSockets ();

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (ConfigTableIndex = 0; ConfigTableIndex < gST->NumberOfTableEntries; ConfigTableIndex++) {
    if (CompareGuid (&gArmMpCoreInfoGuid, &(gST->ConfigurationTable[ConfigTableIndex].VendorGuid))) {
      NumberOfSubspaces = ACPI_PCC_MAX_SUBPACE_PER_SOCKET * NumberOfSockets;

      AcpiPccAllocateSharedMemory (&PccSharedMemPointer, NumberOfSubspaces);
      if (PccSharedMemPointer == 0) {
        return EFI_OUT_OF_RESOURCES;
      }

      Size = sizeof (EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER) +
             NumberOfSubspaces * sizeof (EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS);

      PcctTablePointer = (EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER *)AllocateZeroPool (Size);
      if (PcctTablePointer == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      PcctEntryPointer = (EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS *)
                          ((UINT64)PcctTablePointer + sizeof (EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER));

      for (Socket = 0; Socket < NumberOfSockets; Socket++) {
        for (Doorbell = 0; Doorbell < NUMBER_OF_DOORBELLS_PER_SOCKET; Doorbell++ ) {
          if (AcpiPccIsDoorbellReserved (Doorbell + NUMBER_OF_DOORBELLS_PER_SOCKET * Socket)) {
            continue;
          }

          CopyMem (
            &PcctEntryPointer[Subspace],
            &PcctSubspaceTemplate,
            sizeof (EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS)
            );

          PcctEntryPointer[Subspace].BaseAddress = (UINT64)PccSharedMemPointer + ACPI_PCC_SUBSPACE_SHARED_MEM_SIZE * Subspace;
          PcctEntryPointer[Subspace].AddressLength = ACPI_PCC_SUBSPACE_SHARED_MEM_SIZE;

          DoorbellAddress = MailboxGetDoorbellAddress (Socket, Doorbell);

          PcctEntryPointer[Subspace].DoorbellRegister.Address = DoorbellAddress + DB_OUT_REG_OFST;
          PcctEntryPointer[Subspace].PlatformInterrupt = MailboxGetDoorbellInterruptNumber (Socket, Doorbell);
          PcctEntryPointer[Subspace].PlatformInterruptAckRegister.Address = DoorbellAddress + DB_STATUS_REG_OFST;

          if (Doorbell == ACPI_PCC_CPPC_DOORBELL_ID) {
            PcctEntryPointer[Subspace].DoorbellWrite = MAILBOX_URGENT_CPPC_MESSAGE;
            PcctEntryPointer[Subspace].NominalLatency = ACPI_PCC_CPPC_NOMINAL_LATENCY_US;
            PcctEntryPointer[Subspace].MinimumRequestTurnaroundTime = ACPI_PCC_CPPC_MIN_REQ_TURNAROUND_TIME_US;
          } else {
            PcctEntryPointer[Subspace].DoorbellWrite = MAILBOX_TYPICAL_PCC_MESSAGE;
            PcctEntryPointer[Subspace].NominalLatency = ACPI_PCC_NOMINAL_LATENCY_US;
            PcctEntryPointer[Subspace].MinimumRequestTurnaroundTime = ACPI_PCC_MIN_REQ_TURNAROUND_TIME_US;
          }
          PcctEntryPointer[Subspace].MaximumPeriodicAccessRate = ACPI_PCC_MAX_PERIODIC_ACCESS_RATE;

          Subspace++;
        }
      }

      CopyMem (
        PcctTablePointer,
        &PcctTableHeaderTemplate,
        sizeof (EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER)
        );

      //
      // Recalculate the size
      //
      Size = sizeof (EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER) +
             Subspace * sizeof (EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS);

      PcctTablePointer->Header.Length = Size;
      AcpiTableChecksum (
        (UINT8 *)PcctTablePointer,
        PcctTablePointer->Header.Length
        );

      Status = AcpiTableProtocol->InstallAcpiTable (
                                    AcpiTableProtocol,
                                    (VOID *)PcctTablePointer,
                                    PcctTablePointer->Header.Length,
                                    &PcctTableKey
                                    );
      if (EFI_ERROR (Status)) {
        AcpiPccFreeSharedMemory ();
        FreePool ((VOID *)PcctTablePointer);
        return Status;
      }
      break;
    }
  }

  if (ConfigTableIndex == gST->NumberOfTableEntries) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}
