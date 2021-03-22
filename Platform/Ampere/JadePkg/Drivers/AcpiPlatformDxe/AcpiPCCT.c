/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/AcpiPccLib.h>
#include "AcpiPlatform.h"

EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS PCCTSubspaceTemplate = {
  2,    /* HW-Reduced Communication Subspace Type 2 */
  sizeof (EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS),
  0,    /* DoorbellInterrupt: need fill in */
  0,
  0,
  0,    /* BaseAddress: need fill in */
  0x100,
  {0, 0x20, 0, 0x3, 0x0},    /* DoorbellRegister: need fill in */
  0,
  0x53000040,
  1,
  1,
  1,
  {0, 0x20, 0, 0x3, 0x0},    /* DoorbellAckRegister: need fill in */
  0,
  0x10001,
};

EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER PCCTTableHeaderTemplate = {
  {
    EFI_ACPI_6_3_PLATFORM_COMMUNICATIONS_CHANNEL_TABLE_SIGNATURE,
    0,                                                          /* need fill in */
    EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_REVISION, // Revision
    0x00,                                                       // Checksum will be updated at runtime
    //
    // It is expected that these values will be updated at EntryPoint.
    //
    {0},     // OEM ID is a 6 bytes long field
    0,       // OEM Table ID(8 bytes long)
    0,       // OEM Revision
    0,       // Creator ID
    0,       // Creator Revision
  },
  EFI_ACPI_6_3_PCCT_FLAGS_PLATFORM_INTERRUPT,
};

STATIC BOOLEAN
AcpiPcctIsV2 (
  VOID
  )
{
  VOID *Hob;

  /* Get the Platform HOB */
  Hob = GetFirstGuidHob (&gPlatformHobGuid);
  if (Hob == NULL) {
    return FALSE;
  }

  return TRUE;
}

STATIC UINT32
AcpiPcctGetNumOfSocket (
  VOID
  )
{
  UINTN              NumberSockets, NumberActiveSockets, Count, Index, Index1;
  PlatformInfoHob    *PlatformHob;
  PlatformClusterEn  *Socket;
  VOID               *Hob;

  /* Get the Platform HOB */
  Hob = GetFirstGuidHob (&gPlatformHobGuid);
  if (Hob == NULL) {
    return 1;
  }
  PlatformHob = (PlatformInfoHob *)GET_GUID_HOB_DATA (Hob);

  NumberSockets = sizeof (PlatformHob->ClusterEn) / sizeof (PlatformClusterEn);
  NumberActiveSockets = 0;

  for (Index = 0; Index < NumberSockets; Index++) {
    Socket = &PlatformHob->ClusterEn[Index];
    Count = ARRAY_SIZE (Socket->EnableMask);
    for (Index1 = 0; Index1 < Count; Index1++) {
      if (Socket->EnableMask[Index1] != 0) {
        NumberActiveSockets++;
        break;
      }
    }
  }

  return NumberActiveSockets;
}

EFI_STATUS
AcpiPcctInit (
  VOID
  )
{
  INTN NumOfSocket = AcpiPcctGetNumOfSocket ();
  INTN Subspace;
  INTN Socket;
  INTN Idx;

  for (Socket = 0; Socket < NumOfSocket; Socket++) {
    for (Subspace = 0; Subspace < PCC_MAX_SUBSPACES_PER_SOCKET; Subspace++ ) {
      Idx = Subspace + PCC_MAX_SUBSPACES_PER_SOCKET * Socket;
      if (((1 << Idx) & PCC_SUBSPACE_MASK) == 0) {
        continue;
      }
      if (AcpiPcctIsV2 ()) {
        AcpiPccSharedMemInitV2 (Socket, Subspace);
      } else {
        AcpiPccSharedMemInit (Socket, Subspace);
      }
      AcpiPccSyncSharedMemAddr (Socket, Subspace);
      AcpiPccUnmaskInt (Socket, Subspace);
    }
  }

  return EFI_SUCCESS;
}

/*
 *  Install PCCT table.
 *
 *  Each socket has 16 PCC subspaces
 *    0 - 7  : PMPro subspaces
 *    8 - 15 : SMPro subspaces
 */
EFI_STATUS
AcpiInstallPcctTable (
  VOID
  )
{
  EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER *PcctTablePointer = NULL;
  EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS   *PcctEntryPointer = NULL;
  INTN                                                     NumOfSocket = AcpiPcctGetNumOfSocket ();
  UINT64                                                   PccSharedMemPointer = 0;
  EFI_ACPI_TABLE_PROTOCOL                                  *AcpiTableProtocol;
  UINTN                                                    PcctTableKey  = 0;
  INTN                                                     Count, Subspace;
  INTN                                                     SubspaceIdx;
  INTN                                                     NumOfSubspace;
  INTN                                                     IntNum = 0;
  EFI_STATUS                                               Status;
  UINTN                                                    Size;
  INTN                                                     Socket;
  INTN                                                     Idx;

  SubspaceIdx = 0;

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Count = 0; Count < gST->NumberOfTableEntries; Count++) {
    if (CompareGuid (&gArmMpCoreInfoGuid, &(gST->ConfigurationTable[Count].VendorGuid))) {
      NumOfSubspace = PCC_MAX_SUBSPACES_PER_SOCKET * NumOfSocket;

      Size = sizeof (EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER) +
             NumOfSubspace * sizeof (EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS);

      PcctTablePointer = (EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER *)AllocateZeroPool (Size);
      if (PcctTablePointer == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      /* Allocation PCC shared memory */
      AcpiPccAllocSharedMemory (&PccSharedMemPointer, NumOfSubspace);
      if (PccSharedMemPointer == 0) {
        FreePool ((VOID *)PcctTablePointer);
        return EFI_OUT_OF_RESOURCES;
      }

      PcctEntryPointer = (EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS *)((UINT64)PcctTablePointer +
                                                                                    sizeof (EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER));

      for (Socket = 0; Socket < NumOfSocket; Socket++) {
        for (Subspace = 0; Subspace < PCC_MAX_SUBSPACES_PER_SOCKET; Subspace++ ) {
          Idx = Subspace + PCC_MAX_SUBSPACES_PER_SOCKET * Socket;
          if (((1 << Idx) & PCC_SUBSPACE_MASK) == 0) {
            continue;
          }

          CopyMem (
            &PcctEntryPointer[SubspaceIdx],
            &PCCTSubspaceTemplate,
            sizeof (EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS)
            );

          PcctEntryPointer[SubspaceIdx].BaseAddress = PccSharedMemPointer +
                                                      PCC_SUBSPACE_SHARED_MEM_SIZE * Idx;
          PcctEntryPointer[SubspaceIdx].AddressLength = PCC_SUBSPACE_SHARED_MEM_SIZE;

          IntNum = 0;
          if (Socket > 0) {
            IntNum = Socket * PLATFORM_SLAVE_SOCKET_SPI_INTERRUPT_START - 32;
          }
          if (Subspace < PMPRO_MAX_DB) {
            IntNum += PMPRO_DB0_IRQ_NUM + Subspace;
            /* PCC subspaces for 8 PMpro Doorbells */
            PcctEntryPointer[SubspaceIdx].DoorbellRegister.Address = PMPRO_DBx_REG (Socket, Subspace, DB_OUT);
            PcctEntryPointer[SubspaceIdx].PlatformInterrupt = IntNum;
            PcctEntryPointer[SubspaceIdx].PlatformInterruptAckRegister.Address = PMPRO_DBx_REG (Socket, Subspace, DB_STATUS);
          } else {
            IntNum += SMPRO_DB0_IRQ_NUM + Subspace - PMPRO_MAX_DB;
            /* PCC subspaces for 8 SMpro Doorbells */
            PcctEntryPointer[SubspaceIdx].DoorbellRegister.Address = SMPRO_DBx_REG (Socket,Subspace - PMPRO_MAX_DB, DB_OUT);
            PcctEntryPointer[SubspaceIdx].PlatformInterrupt = IntNum;
            PcctEntryPointer[SubspaceIdx].PlatformInterruptAckRegister.Address = SMPRO_DBx_REG (Socket, Subspace - PMPRO_MAX_DB, DB_STATUS);
          }

          /* Timing */
          PcctEntryPointer[SubspaceIdx].NominalLatency = PCC_NOMINAL_LATENCY;
          PcctEntryPointer[SubspaceIdx].MaximumPeriodicAccessRate = PCC_MAX_PERIOD_ACCESS;
          PcctEntryPointer[SubspaceIdx].MinimumRequestTurnaroundTime = PCC_MIN_REQ_TURNAROUND_TIME;

          /* Initialize subspace used for CPPC queries */
          if (Subspace == PCC_CPPC_SUBSPACE) {
            /* Configure CPPC control byte for CPPC : 0x100 */
            PcctEntryPointer[SubspaceIdx].DoorbellWrite = PCC_MSG | PCC_CPPC_URG_MSG |  PCC_CPPC_MSG;
            PcctEntryPointer[SubspaceIdx].NominalLatency = PCC_CPPC_NOMINAL_LATENCY;
            PcctEntryPointer[SubspaceIdx].MinimumRequestTurnaroundTime = PCC_CPPC_MIN_REQ_TURNAROUND_TIME;
          } else {
            PcctEntryPointer[SubspaceIdx].DoorbellWrite = PCC_MSG;
          }
          if (!AcpiPcctIsV2 ()) {
            /* Store the upper address (Bit 40-43) of PCC shared memory */
            PcctEntryPointer[SubspaceIdx].DoorbellWrite |=
              (PcctEntryPointer[SubspaceIdx].BaseAddress >> 40) & PCP_MSG_UPPER_ADDR_MASK;
          }

          SubspaceIdx++;
        }
      }

      CopyMem (
        PcctTablePointer,
        &PCCTTableHeaderTemplate,
        sizeof (EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER)
        );

      /* Recalculate the size */
      Size = sizeof (EFI_ACPI_6_3_PLATFORM_COMMUNICATION_CHANNEL_TABLE_HEADER) +
             SubspaceIdx * sizeof (EFI_ACPI_6_3_PCCT_SUBSPACE_2_HW_REDUCED_COMMUNICATIONS);

      PcctTablePointer->Header.Length = Size;
      CopyMem (
        PcctTablePointer->Header.OemId,
        PcdGetPtr (PcdAcpiDefaultOemId),
        sizeof (PcctTablePointer->Header.OemId)
        );
      PcctTablePointer->Header.OemTableId = EFI_ACPI_OEM_TABLE_ID;
      PcctTablePointer->Header.OemRevision = 3;
      PcctTablePointer->Header.CreatorId = EFI_ACPI_CREATOR_ID;
      PcctTablePointer->Header.CreatorRevision = EFI_ACPI_CREATOR_REVISION;

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
        AcpiPccFreeSharedMemory (&PccSharedMemPointer, NumOfSubspace);
        FreePool ((VOID *)PcctTablePointer);
        return Status;
      }
      break;
    }
  }

  if (Count == gST->NumberOfTableEntries) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}
