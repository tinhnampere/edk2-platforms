/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>

/*
 * The protocols, PPI and GUID defintions for this module
 */
#include <Guid/MemoryTypeInformation.h>
#include <Guid/PlatformInfoHobGuid.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/PeimEntryPoint.h>
#include <PlatformInfoHob.h>
#include <Ppi/BootInRecoveryMode.h>
#include <Ppi/MasterBootMode.h>

EFI_STATUS
EFIAPI
MemoryPeim (
  IN EFI_PHYSICAL_ADDRESS               UefiMemoryBase,
  IN UINT64                             UefiMemorySize
  );

VOID
BuildMemoryTypeInformationHob (
  VOID
  )
{
  EFI_MEMORY_TYPE_INFORMATION   Info[10];

  Info[0].Type          = EfiACPIReclaimMemory;
  Info[0].NumberOfPages = PcdGet32 (PcdMemoryTypeEfiACPIReclaimMemory);
  Info[1].Type          = EfiACPIMemoryNVS;
  Info[1].NumberOfPages = PcdGet32 (PcdMemoryTypeEfiACPIMemoryNVS);
  Info[2].Type          = EfiReservedMemoryType;
  Info[2].NumberOfPages = PcdGet32 (PcdMemoryTypeEfiReservedMemoryType);
  Info[3].Type          = EfiRuntimeServicesData;
  Info[3].NumberOfPages = PcdGet32 (PcdMemoryTypeEfiRuntimeServicesData);
  Info[4].Type          = EfiRuntimeServicesCode;
  Info[4].NumberOfPages = PcdGet32 (PcdMemoryTypeEfiRuntimeServicesCode);
  Info[5].Type          = EfiBootServicesCode;
  Info[5].NumberOfPages = PcdGet32 (PcdMemoryTypeEfiBootServicesCode);
  Info[6].Type          = EfiBootServicesData;
  Info[6].NumberOfPages = PcdGet32 (PcdMemoryTypeEfiBootServicesData);
  Info[7].Type          = EfiLoaderCode;
  Info[7].NumberOfPages = PcdGet32 (PcdMemoryTypeEfiLoaderCode);
  Info[8].Type          = EfiLoaderData;
  Info[8].NumberOfPages = PcdGet32 (PcdMemoryTypeEfiLoaderData);

  /* Terminator for the list */
  Info[9].Type          = EfiMaxMemoryType;
  Info[9].NumberOfPages = 0;

  BuildGuidDataHob (&gEfiMemoryTypeInformationGuid, &Info, sizeof (Info));
}

EFI_STATUS
EFIAPI
InitializeMemory (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                            Status;
  UINTN                                 SystemMemoryBase;
  UINTN                                 SystemMemoryTop;
  UINTN                                 FdBase;
  UINTN                                 FdTop;
  UINTN                                 UefiMemoryBase;
  UINTN                                 Index;
  VOID                                  *Hob;
  PlatformInfoHob_V2                    *PlatformHob;

  DEBUG ((DEBUG_INFO, "Memory Init PEIM Loaded\n"));

  Hob = GetFirstGuidHob (&gPlatformHobV2Guid);
  if (Hob == NULL) {
    return EFI_DEVICE_ERROR;
  }

  PlatformHob = (PlatformInfoHob_V2 *) GET_GUID_HOB_DATA (Hob);

  /* Find system memory top of the first node */
  SystemMemoryTop = 0;
  for (Index = 0; Index < PlatformHob->DramInfo.NumRegion; Index++) {
    if (SystemMemoryTop <= PlatformHob->DramInfo.Base[Index] &&
            PlatformHob->DramInfo.Node[Index] == 0 &&
            (PlatformHob->DramInfo.Base[Index] + PlatformHob->DramInfo.Size[Index] - 1) <= 0xFFFFFFFF) {
      SystemMemoryTop = PlatformHob->DramInfo.Base[Index] + PlatformHob->DramInfo.Size[Index];
    }
  }

  DEBUG ((DEBUG_INFO, "PEIM memory configuration.\n"));

  SystemMemoryBase = (UINTN) FixedPcdGet64 (PcdSystemMemoryBase);
  FdBase = (UINTN) PcdGet64 (PcdFdBaseAddress);
  FdTop = FdBase + (UINTN)PcdGet32 (PcdFdSize);

  // In case the firmware has been shadowed in the System Memory
  if ((FdBase >= SystemMemoryBase) && (FdTop <= SystemMemoryTop)) {
    //
    //Check if there is enough space between the top of the system memory and the top of the
    // firmware to place the UEFI memory (for PEI & DXE phases)
    //
    if (SystemMemoryTop - FdTop >= FixedPcdGet32 (PcdSystemMemoryUefiRegionSize)) {
      UefiMemoryBase = SystemMemoryTop - FixedPcdGet32 (PcdSystemMemoryUefiRegionSize);
    } else {
      // Check there is enough space for the UEFI memory
      ASSERT (SystemMemoryBase + FixedPcdGet32 (PcdSystemMemoryUefiRegionSize) <= FdBase);

      UefiMemoryBase = FdBase - FixedPcdGet32 (PcdSystemMemoryUefiRegionSize);
    }
  } else {
    // Check the Firmware does not overlapped with the system memory
    ASSERT ((FdBase < SystemMemoryBase) || (FdBase >= SystemMemoryTop));
    ASSERT ((FdTop <= SystemMemoryBase) || (FdTop > SystemMemoryTop));

    UefiMemoryBase = SystemMemoryTop - FixedPcdGet32 (PcdSystemMemoryUefiRegionSize);
  }

  Status = PeiServicesInstallPeiMemory (UefiMemoryBase, FixedPcdGet32 (PcdSystemMemoryUefiRegionSize));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Error: Failed to install Pei Memory\n"));
  } else {
    DEBUG ((DEBUG_INFO, "Info: Installed Pei Memory\n"));
  }
  ASSERT_EFI_ERROR (Status);

  // Initialize MMU and Memory HOBs (Resource Descriptor HOBs)
  Status = MemoryPeim (UefiMemoryBase, FixedPcdGet32 (PcdSystemMemoryUefiRegionSize));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Error: Failed to initialize MMU and Memory HOBS\n"));
  }
  ASSERT_EFI_ERROR (Status);

  return Status;
}
