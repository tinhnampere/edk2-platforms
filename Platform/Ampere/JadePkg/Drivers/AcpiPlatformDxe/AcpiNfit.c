/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AcpiPlatform.h"

EFI_ACPI_6_3_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE NfitSPATemplate = {
  EFI_ACPI_6_3_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE_TYPE,
  sizeof (EFI_ACPI_6_3_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE),
  0, // The uniue index - need to be filled.
  0, // The flags - need to be filled.
  0, // Reserved.
  0, // Proximity domain - need to be filled.
  EFI_ACPI_6_3_NFIT_GUID_BYTE_ADDRESSABLE_PERSISTENT_MEMORY_REGION, // PM range type.
  0, // Start address - need to be filled.
  0, // Size - need to be filled.
  EFI_MEMORY_UC | EFI_MEMORY_WC | EFI_MEMORY_WT | EFI_MEMORY_WB | EFI_MEMORY_WP | EFI_MEMORY_UCE, // attribute - need to be filled.
};

EFI_ACPI_6_3_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE NvdimmControlRegionTemplate = {
  EFI_ACPI_6_3_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE_TYPE,
  sizeof (EFI_ACPI_6_3_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE),
  0, // The unique index - need to be filled.
  0x1122, // The vendor id - dummy value.
  0x3344, // The device id - dummy value.
  0, // The revision - dummy value.
  0x5566, // The subsystem nvdimm id - dummy value.
  0x7788, // The subsystem nvdimm device id - dummy value.
  0x0, // The subsystem revision - dummy value.
  0, // The valid field.
  0, // The manufacturing location - not valid.
  0, // The manufacturing date - not valid.
  {0}, // Reserved.
  0xaabbccdd, // The serial number - dummy value.
  0, // The region format interface code - dummy value.
  0, // The number of block control windows.
  0, // The size of block control windows.
  0, // The Command Register Offset in Block Control Window.
  0, // The Size of Command Register in Block Control Windows.
  0, // The Status Register Offset in Block Control Window.
  0, // Size of Status Register in Block Control Windows.
  0, // The NVDIMM Control Region Flag.
  {0}, // Reserved.
};

EFI_ACPI_6_3_NFIT_NVDIMM_REGION_MAPPING_STRUCTURE NvdimmRegionMappingTemplate = {
  EFI_ACPI_6_3_NFIT_NVDIMM_REGION_MAPPING_STRUCTURE_TYPE,
  sizeof (EFI_ACPI_6_3_NFIT_NVDIMM_REGION_MAPPING_STRUCTURE),
  {0}, // _ADR of the NVDIMM device - need to be filled.
  0, // Dimm smbios handle index - need to be filled.
  0, // The unique region index - need to be filled.
  0, // The SPA range index - need to be filled.
  0, // The control region index - need to be filled.
  0, // The region size - need to be filled.
  0, // The region offset - need to be filled.
  0, // The region base - need to be filled.
  0, // The interleave structure index - need to be filled.
  0, // The interleave ways - need to be filled.
  0, // NVDIMM flags - need to be filled.
  0, // Reserved.
};

EFI_ACPI_6_3_NVDIMM_FIRMWARE_INTERFACE_TABLE NFITTableHeaderTemplate = {
  __ACPI_HEADER (
    EFI_ACPI_6_3_NVDIMM_FIRMWARE_INTERFACE_TABLE_STRUCTURE_SIGNATURE,
    0, /* need fill in */
    EFI_ACPI_6_3_NVDIMM_FIRMWARE_INTERFACE_TABLE_REVISION
  ),
  0x00000000, // Reserved
};

/*
 * Count NVDIMM Region
 */
EFI_STATUS
AcpiGetNvdRegionNumber (
  IN OUT UINTN *NvdRegionNum
  )
{
  PlatformInfoHob_V2  *PlatformHob;
  UINTN               Count;
  VOID                *Hob;

  /* Get the Platform HOB */
  Hob = GetFirstGuidHob (&gPlatformHobV2Guid);
  if (Hob == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  PlatformHob = (PlatformInfoHob_V2 *) GET_GUID_HOB_DATA (Hob);

  *NvdRegionNum = 0;
  for (Count = 0; Count < PlatformHob->DramInfo.NumRegion; Count++) {
    if (PlatformHob->DramInfo.NvdRegion[Count]) {
      *NvdRegionNum += 1;
    }
  }

  if (*NvdRegionNum == 0) {
    DEBUG ((DEBUG_INFO, "No NVDIMM Region\n"));
    return EFI_INVALID_PARAMETER; /* No NVDIMM Region */
  }

  return EFI_SUCCESS;
}

/*
 * Fill in SPA strucutre
 */
VOID
AcpiNfitFillSPA (
  IN OUT EFI_ACPI_6_3_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE *NfitSpaPointer,
  IN     UINTN  NvdRegionIndex,
  IN     UINT64 NvdRegionBase,
  IN     UINT64 NvdRegionSize
  )
{
  NfitSpaPointer->Flags                            = 0;
  NfitSpaPointer->SPARangeStructureIndex           = NvdRegionIndex;
  NfitSpaPointer->SystemPhysicalAddressRangeBase   = NvdRegionBase;
  NfitSpaPointer->SystemPhysicalAddressRangeLength = NvdRegionSize;
}

VOID
NfitFillControlRegion (
  IN OUT EFI_ACPI_6_3_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE *NfitControlRegionPointer,
  IN UINTN NvdRegionIndex
  )
{
  NfitControlRegionPointer->NVDIMMControlRegionStructureIndex = NvdRegionIndex;
}

VOID
NfitFillRegionMapping (
  IN OUT EFI_ACPI_6_3_NFIT_NVDIMM_REGION_MAPPING_STRUCTURE *NfitRegionMappingPointer,
  IN EFI_ACPI_6_3_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE *NfitControlRegionPointer,
  IN EFI_ACPI_6_3_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE *NfitSpaPointer
  )
{
  NfitRegionMappingPointer->NVDIMMControlRegionStructureIndex    = NfitControlRegionPointer->NVDIMMControlRegionStructureIndex;
  NfitRegionMappingPointer->SPARangeStructureIndex               = NfitSpaPointer->SPARangeStructureIndex;
  NfitRegionMappingPointer->NVDIMMPhysicalAddressRegionBase      = NfitSpaPointer->SystemPhysicalAddressRangeBase;
  NfitRegionMappingPointer->NVDIMMRegionSize                     = NfitSpaPointer->SystemPhysicalAddressRangeLength;
  NfitRegionMappingPointer->NFITDeviceHandle.DIMMNumber          = 1; // Hardcoded for now.
  NfitRegionMappingPointer->NFITDeviceHandle.MemoryChannelNumber = 0; // Hardcoded for now.
  NfitRegionMappingPointer->NFITDeviceHandle.MemoryControllerID  = 0; // Hardcoded for now.
  NfitRegionMappingPointer->NFITDeviceHandle.NodeControllerID    = 0; // Hardcoded for now.
  NfitRegionMappingPointer->NFITDeviceHandle.SocketID            = 0; // Hardcoded for now.
  NfitRegionMappingPointer->RegionOffset                         = 0; // Hardcoded for now.
}

EFI_STATUS
AcpiNfitFillTable (
  IN EFI_ACPI_6_3_NVDIMM_FIRMWARE_INTERFACE_TABLE *NfitTablePointer
  )
{
  EFI_ACPI_6_3_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE   *NfitSpaPointer;
  EFI_ACPI_6_3_NFIT_NVDIMM_REGION_MAPPING_STRUCTURE           *NfitRegionMappingPointer;
  EFI_ACPI_6_3_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE           *NfitControlRegionPointer;
  PlatformInfoHob_V2  *PlatformHob;
  UINTN               Count;
  VOID                *Hob;
  UINTN               NvdRegionIndex;
  UINT64              NvdRegionBase;
  UINT64              NvdRegionSize;

  /* Get the Platform HOB */
  Hob = GetFirstGuidHob (&gPlatformHobV2Guid);
  if (Hob == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  PlatformHob = (PlatformInfoHob_V2 *) GET_GUID_HOB_DATA (Hob);

  NfitSpaPointer = (EFI_ACPI_6_3_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE *)
                   (NfitTablePointer + 1);
  NvdRegionIndex = 0;
  for (Count = 0; Count < PlatformHob->DramInfo.NumRegion; Count++) {
    if (PlatformHob->DramInfo.NvdRegion[Count] != 0) {
      NvdRegionIndex++;
      NvdRegionBase = PlatformHob->DramInfo.Base[Count];
      NvdRegionSize = PlatformHob->DramInfo.Size[Count];

      CopyMem ((VOID *) NfitSpaPointer, (VOID *) &NfitSPATemplate, sizeof (NfitSPATemplate));
      AcpiNfitFillSPA (NfitSpaPointer, NvdRegionIndex, NvdRegionBase, NvdRegionSize);

      NfitControlRegionPointer = (EFI_ACPI_6_3_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE *)
                                 (NfitSpaPointer + 1);
      CopyMem ((VOID *) NfitControlRegionPointer,
               (VOID *) &NvdimmControlRegionTemplate,
               sizeof (NvdimmControlRegionTemplate));
      NfitFillControlRegion (NfitControlRegionPointer, NvdRegionIndex);

      NfitRegionMappingPointer = (EFI_ACPI_6_3_NFIT_NVDIMM_REGION_MAPPING_STRUCTURE *)
                                 (NfitControlRegionPointer + 1);
      CopyMem ((VOID *) NfitRegionMappingPointer,
               (VOID *) &NvdimmRegionMappingTemplate,
               sizeof (NvdimmRegionMappingTemplate));
      NfitFillRegionMapping (NfitRegionMappingPointer,
                             NfitControlRegionPointer,
                             NfitSpaPointer);
      /* Next Region */
      NfitSpaPointer = (EFI_ACPI_6_3_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE *)
                       (NfitRegionMappingPointer + 1);
    }
  }

  return EFI_SUCCESS;
}

/*
 * Install NFIT table.
 */
EFI_STATUS
AcpiInstallNfitTable (VOID)
{
  EFI_ACPI_6_3_NVDIMM_FIRMWARE_INTERFACE_TABLE                *NfitTablePointer;
  EFI_ACPI_TABLE_PROTOCOL                                     *AcpiTableProtocol;
  UINTN                                                       NfitTableKey  = 0;
  EFI_STATUS                                                  Status;
  UINTN                                                       Size;
  UINTN                                                       NvdRegionNum;

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid,
                  NULL, (VOID **) &AcpiTableProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AcpiGetNvdRegionNumber (&NvdRegionNum);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size = sizeof (EFI_ACPI_6_3_NVDIMM_FIRMWARE_INTERFACE_TABLE) +
         ((sizeof (EFI_ACPI_6_3_NFIT_SYSTEM_PHYSICAL_ADDRESS_RANGE_STRUCTURE) +
           sizeof (EFI_ACPI_6_3_NFIT_NVDIMM_REGION_MAPPING_STRUCTURE) +
           sizeof (EFI_ACPI_6_3_NFIT_NVDIMM_CONTROL_REGION_STRUCTURE)) * NvdRegionNum);
  NfitTablePointer = (EFI_ACPI_6_3_NVDIMM_FIRMWARE_INTERFACE_TABLE *) AllocateZeroPool (Size);
  if (NfitTablePointer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  CopyMem ((VOID *) NfitTablePointer,
           (VOID *) &NFITTableHeaderTemplate,
           sizeof (NFITTableHeaderTemplate));
  NfitTablePointer->Header.Length = Size;

  Status = AcpiNfitFillTable (NfitTablePointer);
  if (EFI_ERROR (Status)) {
    FreePool ((VOID *) NfitTablePointer);
    return Status;
  }

  AcpiTableChecksum ((UINT8 *)NfitTablePointer, NfitTablePointer->Header.Length);
  Status = AcpiTableProtocol->InstallAcpiTable (AcpiTableProtocol,
                                                (VOID *) NfitTablePointer,
                                                NfitTablePointer->Header.Length,
                                                &NfitTableKey);
  if (EFI_ERROR (Status)) {
    FreePool ((VOID *) NfitTablePointer);
  }

  return Status;
}
