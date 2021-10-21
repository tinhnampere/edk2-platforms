/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AcpiPlatform.h"

EFI_ACPI_6_3_GIC_ITS_STRUCTURE GicItsTemplate = {
  EFI_ACPI_6_3_GIC_ITS,
  sizeof (EFI_ACPI_6_3_GIC_ITS_STRUCTURE),
  EFI_ACPI_RESERVED_WORD,
  0, /* GicItsId */
  0, /* PhysicalBaseAddress */
  0, /* Reserved2 */
};

EFI_ACPI_6_3_GICR_STRUCTURE GicRTemplate = {
  EFI_ACPI_6_3_GICR,
  sizeof (EFI_ACPI_6_3_GICR_STRUCTURE),
  EFI_ACPI_RESERVED_WORD,
  GICR_MASTER_BASE_REG, /* DiscoveryRangeBaseAddress */
  0x1000000,            /* DiscoveryRangeLength */
};

EFI_ACPI_6_3_GIC_DISTRIBUTOR_STRUCTURE GicDTemplate = {
  EFI_ACPI_6_3_GICD,
  sizeof (EFI_ACPI_6_3_GIC_DISTRIBUTOR_STRUCTURE),
  EFI_ACPI_RESERVED_WORD,
  0,             /* GicDistHwId */
  GICD_BASE_REG, /* GicDistBase */
  0,             /* GicDistVector */
  0x3,           /* GicVersion */
  {EFI_ACPI_RESERVED_BYTE, EFI_ACPI_RESERVED_BYTE, EFI_ACPI_RESERVED_BYTE}
};

EFI_ACPI_6_3_GIC_STRUCTURE GiccTemplate = {
  EFI_ACPI_6_3_GIC,
  sizeof (EFI_ACPI_6_3_GIC_STRUCTURE),
  EFI_ACPI_RESERVED_WORD,
  0, /* GicId */
  0, /* AcpiCpuUid */
  0, /* Flags */
  0,
  23, /* PmuIrq */
  0,
  0,
  0,
  0,
  25, /* GsivId */
  0,  /* GicRBase */
  0,  /* Mpidr */
  0,  /* ProcessorPowerEfficiencyClass */
  0,  /* Reserved2 */
  21, /* SPE irq */
};

EFI_ACPI_6_3_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER MADTTableHeaderTemplate = {
  __ACPI_HEADER (
    EFI_ACPI_6_3_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE,
    0, /* need fill in */
    EFI_ACPI_6_3_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION
    ),
};

UINT32 Ac01CoreOrderMonolithic[PLATFORM_CPU_MAX_CPM * PLATFORM_CPU_NUM_CORES_PER_CPM] = {
  36, 37, 40, 41, 52, 53, 56, 57, 32, 33,
  44, 45, 48, 49, 60, 61, 20, 21, 24, 25,
  68, 69, 72, 73, 16, 17, 28, 29, 64, 65,
  76, 77,  4,  5,  8,  9,  0,  1, 12, 13,
  38, 39, 42, 43, 54, 55, 58, 59, 34, 35,
  46, 47, 50, 51, 62, 63, 22, 23, 26, 27,
  70, 71, 74, 75, 18, 19, 30, 31, 66, 67,
  78, 79,  6,  7, 10, 11,  2,  3, 14, 15,
};

UINT32 Ac01CoreOrderHemisphere[PLATFORM_CPU_MAX_CPM * PLATFORM_CPU_NUM_CORES_PER_CPM] = {
  32, 33, 48, 49, 16, 17, 64, 65, 36, 37,
  52, 53,  0,  1, 20, 21, 68, 69,  4,  5,
  34, 35, 50, 51, 18, 19, 66, 67, 38, 39,
  54, 55,  2,  3, 22, 23, 70, 71,  6,  7,
  44, 45, 60, 61, 28, 29, 76, 77, 40, 41,
  56, 57, 12, 13, 24, 25, 72, 73,  8,  9,
  46, 47, 62, 63, 30, 31, 78, 79, 42, 43,
  58, 59, 14, 15, 26, 27, 74, 75, 10, 11,
};

UINT32 Ac01CoreOrderQuadrant[PLATFORM_CPU_MAX_CPM * PLATFORM_CPU_NUM_CORES_PER_CPM] = {
  16, 17, 32, 33,  0,  1, 20, 21,  4,  5,
  18, 19, 34, 35,  2,  3, 22, 23,  6,  7,
  48, 49, 64, 65, 52, 53, 68, 69, 36, 37,
  50, 51, 66, 67, 54, 55, 70, 71, 38, 39,
  28, 29, 44, 45, 12, 13, 24, 25,  8,  9,
  30, 31, 46, 47, 14, 15, 26, 27, 10, 11,
  60, 61, 76, 77, 56, 57, 72, 73, 40, 41,
  62, 63, 78, 79, 58, 59, 74, 75, 42, 43,
};

EFI_ACPI_6_3_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *MadtTablePointer;

UINT32 *
CpuGetCoreOrder (
  VOID
  )
{
  UINT8              SubNumaMode;

  SubNumaMode = CpuGetSubNumaMode ();
  switch (SubNumaMode) {
  case SUBNUMA_MODE_MONOLITHIC:
    return (UINT32 *)&Ac01CoreOrderMonolithic;

  case SUBNUMA_MODE_HEMISPHERE:
    return (UINT32 *)&Ac01CoreOrderHemisphere;

  case SUBNUMA_MODE_QUADRANT:
    return (UINT32 *)&Ac01CoreOrderQuadrant;

  default:
    // Should never reach here
    ASSERT (FALSE);
    return NULL;
  }

  return NULL;
}

UINT32
AcpiInstallMadtProcessorNode (
  VOID   *EntryPointer,
  UINT32 CpuId
  )
{
  EFI_ACPI_6_3_GIC_STRUCTURE *MadtProcessorEntryPointer = EntryPointer;
  UINT32                     SocketId;
  UINT32                     ClusterId;
  UINTN                      Size;

  Size = sizeof (GiccTemplate);
  CopyMem (MadtProcessorEntryPointer, &GiccTemplate, Size);

  SocketId = SOCKET_ID (CpuId);
  ClusterId = CLUSTER_ID (CpuId);

  //
  // GICv2 compatibility mode is not supported.
  // Hence, set GIC's CPU Interface Number to 0.
  //
  MadtProcessorEntryPointer->CPUInterfaceNumber = 0;
  MadtProcessorEntryPointer->AcpiProcessorUid =
    (SocketId << PLATFORM_SOCKET_UID_BIT_OFFSET) +
    (ClusterId << 8) + (CpuId  % PLATFORM_CPU_NUM_CORES_PER_CPM);
  MadtProcessorEntryPointer->Flags = 1;
  MadtProcessorEntryPointer->MPIDR =
    (((ClusterId << 8) + (CpuId  % PLATFORM_CPU_NUM_CORES_PER_CPM)) << 8);
  MadtProcessorEntryPointer->MPIDR += (((UINT64)SocketId) << 32);

  return Size;
}

UINT32
AcpiInstallMadtGicD (
  VOID *EntryPointer
  )
{
  EFI_ACPI_6_3_GIC_DISTRIBUTOR_STRUCTURE *GicDEntryPointer = EntryPointer;
  UINTN                                  Size;

  Size = sizeof (GicDTemplate);
  CopyMem (GicDEntryPointer, &GicDTemplate, Size);

  return Size;
}

UINT32
AcpiInstallMadtGicR (
  VOID   *EntryPointer,
  UINT32 SocketId
  )
{
  EFI_ACPI_6_3_GICR_STRUCTURE *GicREntryPointer = EntryPointer;
  UINTN                       Size;

  /*
   * If the Slave socket is not present, discard the Slave socket
   * GIC redistributor region
   */
  if (SocketId == 1 && !IsSlaveSocketActive ()) {
    return 0;
  }

  Size = sizeof (GicRTemplate);
  CopyMem (GicREntryPointer, &GicRTemplate, Size);

  if (SocketId == 1) {
    GicREntryPointer->DiscoveryRangeBaseAddress = GICR_SLAVE_BASE_REG;
  }

  return Size;
}

UINT32
AcpiInstallMadtGicIts (
  VOID   *EntryPointer,
  UINT32 Index
  )
{
  EFI_ACPI_6_3_GIC_ITS_STRUCTURE *GicItsEntryPointer = EntryPointer;
  UINTN                          Size, Offset;
  UINT64                         GicBase = GICD_BASE_REG;
  UINT32                         ItsId = Index;

  if (Index > SOCKET0_LAST_RC) { /* Socket 1, Index: 8-15 */
    GicBase = GICD_SLAVE_BASE_REG;
    Index -= (SOCKET0_LAST_RC + 1); /* Socket 1, Index:8 -> RCA0 */
  }
  Size = sizeof (GicItsTemplate);
  CopyMem (GicItsEntryPointer, &GicItsTemplate, Size);
  Offset = 0x40000 + Index * 0x20000;
  GicItsEntryPointer->GicItsId = ItsId;
  GicItsEntryPointer->PhysicalBaseAddress = Offset + GicBase;

  return Size;
}

/*
 *  Install MADT table.
 */
EFI_STATUS
AcpiInstallMadtTable (
  VOID
  )
{
  EFI_ACPI_6_3_GIC_STRUCTURE *GiccEntryPointer = NULL;
  EFI_ACPI_TABLE_PROTOCOL    *AcpiTableProtocol;
  UINTN                      MadtTableKey  = 0;
  INTN                       Index;
  EFI_STATUS                 Status;
  UINTN                      Size;
  UINT32                     *CoreOrder;
  UINT32                     SktMaxCoreNum;

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = sizeof (MADTTableHeaderTemplate) +
          (PLATFORM_CPU_MAX_NUM_CORES * sizeof (GiccTemplate)) +
          sizeof (GicDTemplate) +
          (PLATFORM_CPU_MAX_SOCKET * sizeof (GicRTemplate)) +
          ((SOCKET0_LAST_RC - SOCKET0_FIRST_RC +  1) * sizeof (GicItsTemplate));
  if (IsSlaveSocketActive ()) {
    Size += ((SOCKET1_LAST_RC - SOCKET1_FIRST_RC +  1) * sizeof (GicItsTemplate));
  } else if (!IsSlaveSocketPresent ()) {
    Size += 2 * sizeof (GicItsTemplate); /* RCA0/1 */
  }

  MadtTablePointer =
    (EFI_ACPI_6_3_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER *)AllocateZeroPool (Size);
  if (MadtTablePointer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  GiccEntryPointer =
    (EFI_ACPI_6_3_GIC_STRUCTURE *)((UINT64)MadtTablePointer +
                                    sizeof (MADTTableHeaderTemplate));

  /* Install Gic interface for each processor */
  Size = 0;
  CoreOrder = CpuGetCoreOrder ();
  ASSERT (CoreOrder != NULL);
  SktMaxCoreNum = PLATFORM_CPU_MAX_CPM * PLATFORM_CPU_NUM_CORES_PER_CPM;
  for (Index = 0; Index < SktMaxCoreNum; Index++) {
    if (IsCpuEnabled (CoreOrder[Index])) {
      Size += AcpiInstallMadtProcessorNode ((VOID *)((UINT64)GiccEntryPointer + Size), CoreOrder[Index]);
    }
  }

  for (Index = 0; Index < SktMaxCoreNum; Index++) {
    if (IsCpuEnabled (CoreOrder[Index] + SktMaxCoreNum)) {
      Size += AcpiInstallMadtProcessorNode ((VOID *)((UINT64)GiccEntryPointer + Size), CoreOrder[Index] + SktMaxCoreNum);
    }
  }

  /* Install Gic Distributor */
  Size += AcpiInstallMadtGicD ((VOID *)((UINT64)GiccEntryPointer + Size));

  /* Install Gic Redistributor */
  for (Index = 0; Index < PLATFORM_CPU_MAX_SOCKET; Index++) {
    Size += AcpiInstallMadtGicR ((VOID *)((UINT64)GiccEntryPointer + Size), Index);
  }

  /* Install Gic ITS */
  if (!IsSlaveSocketPresent ()) {
    for (Index = 0; Index <= 1; Index++) { /* RCA0/1 */
      Size += AcpiInstallMadtGicIts ((VOID *)((UINT64)GiccEntryPointer + Size), Index);
    }
  }
  for (Index = SOCKET0_FIRST_RC; Index <= SOCKET0_LAST_RC; Index++) {
    Size += AcpiInstallMadtGicIts ((VOID *)((UINT64)GiccEntryPointer + Size), Index);
  }
  if (IsSlaveSocketActive ()) {
    for (Index = SOCKET1_FIRST_RC; Index <= SOCKET1_LAST_RC; Index++) {
      Size += AcpiInstallMadtGicIts ((VOID *)((UINT64)GiccEntryPointer + Size), Index);
    }
  }
  CopyMem (
    MadtTablePointer,
    &MADTTableHeaderTemplate,
    sizeof (MADTTableHeaderTemplate)
    );

  Size += sizeof (MADTTableHeaderTemplate);
  MadtTablePointer->Header.Length = Size;
  CopyMem (
    MadtTablePointer->Header.OemId,
    PcdGetPtr (PcdAcpiDefaultOemId),
    sizeof (MadtTablePointer->Header.OemId)
    );

  AcpiTableChecksum (
    (UINT8 *)MadtTablePointer,
    MadtTablePointer->Header.Length
    );

  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                (VOID *)MadtTablePointer,
                                MadtTablePointer->Header.Length,
                                &MadtTableKey
                                );
  FreePool ((VOID *)MadtTablePointer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}
