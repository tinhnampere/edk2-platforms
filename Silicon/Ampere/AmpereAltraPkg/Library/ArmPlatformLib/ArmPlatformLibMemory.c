/** @file

  Copyright (c) 2020-2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/PlatformInfoHobGuid.h>
#include <Library/AmpereCpuLib.h>
#include <Library/ArmPlatformLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <PlatformInfoHob.h>

/* Number of Virtual Memory Map Descriptors */
#define MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS          50

/* DDR attributes */
#define DDR_ATTRIBUTES_CACHED           ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK
#define DDR_ATTRIBUTES_UNCACHED         ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED

/**
  Return the Virtual Memory Map of your platform

  This Virtual Memory Map is used by MemoryInitPei Module to initialize the MMU on your platform.

  @param[out]   VirtualMemoryMap    Array of ARM_MEMORY_REGION_DESCRIPTOR describing a Physical-to-
                                    Virtual Memory mapping. This array must be ended by a zero-filled
                                    entry

**/
VOID
ArmPlatformGetVirtualMemoryMap (
  OUT ARM_MEMORY_REGION_DESCRIPTOR **VirtualMemoryMap
  )
{
  UINTN                        Index = 0;
  ARM_MEMORY_REGION_DESCRIPTOR *VirtualMemoryTable;
  UINT32                       NumRegion;
  UINTN                        Count;
  VOID                         *Hob;
  PLATFORM_INFO_HOB            *PlatformHob;

  Hob = GetFirstGuidHob (&gPlatformHobGuid);
  if (Hob == NULL) {
    return;
  }

  PlatformHob = (PLATFORM_INFO_HOB *)GET_GUID_HOB_DATA (Hob);

  ASSERT (VirtualMemoryMap != NULL);

  VirtualMemoryTable = (ARM_MEMORY_REGION_DESCRIPTOR *)AllocatePages (EFI_SIZE_TO_PAGES (sizeof (ARM_MEMORY_REGION_DESCRIPTOR) * MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS));
  if (VirtualMemoryTable == NULL) {
    return;
  }

  /* For Address space 0x1000_0000_0000 to 0x1001_00FF_FFFF
   *  - Device memory
   */
  VirtualMemoryTable[Index].PhysicalBase = 0x100000000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x100000000000ULL;
  VirtualMemoryTable[Index].Length       = 0x102000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /* For Address space 0x5000_0000_0000 to 0x5001_00FF_FFFF
   *  - Device memory
   */
  if (IsSlaveSocketActive ())
  {
    VirtualMemoryTable[++Index].PhysicalBase = 0x500000000000ULL;
    VirtualMemoryTable[Index].VirtualBase  = 0x500000000000ULL;
    VirtualMemoryTable[Index].Length       = 0x101000000ULL;
    VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
  }

  /*
   *  - PCIe RCA0 Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x300000000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x300000000000ULL;
  VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket0 RCA0 32-bit Device memory
   *  - 1P/PCIe consolidated to RCB2 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x20000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x20000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - PCIe RCA1 Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x340000000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x340000000000ULL;
  VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket0 RCA1 32-bit Device memory
   *  - 1P/PCIe consolidated to RCB2 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x28000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x28000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - PCIe RCA2 Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x380000000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x380000000000ULL;
  VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket0 RCA2 32-bit Device memory
   *  - 1P/PCIe consolidated to RCB3 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x30000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x30000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - PCIe RCA3 Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x3C0000000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x3C0000000000ULL;
  VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket0 RCA3 32-bit Device memory
   *  - 1P/PCIe consolidated to RCB3 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x38000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x38000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - PCIe RCB0 Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x200000000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x200000000000ULL;
  VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket0 RCB0 32-bit Device memory
   *  - 1P/PCIe consolidated to RCB0 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x00000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x00000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - PCIe RCB1 Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x240000000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x240000000000ULL;
  VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket0 RCB1 32-bit Device memory
   *  - 1P/PCIe consolidated to RCB0 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x08000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x08000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - PCIe RCB2 Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x280000000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x280000000000ULL;
  VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket0 RCB2 32-bit Device memory
   *  - 1P/PCIe consolidated to RCB1 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x10000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x10000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - PCIe RCB3 Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x2C0000000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x2C0000000000ULL;
  VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket0 RCB3 32-bit Device memory
   *  - 1P/PCIe consolidated to RCB1 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x18000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x18000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  if (IsSlaveSocketActive ()) {
    // Slave socket exist
    /*
     *  - PCIe RCA0 Device memory
     */
    VirtualMemoryTable[++Index].PhysicalBase = 0x700000000000ULL;
    VirtualMemoryTable[Index].VirtualBase  = 0x700000000000ULL;
    VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
    VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

    /*
     *  - PCIe RCA1 Device memory
     */
    VirtualMemoryTable[++Index].PhysicalBase = 0x740000000000ULL;
    VirtualMemoryTable[Index].VirtualBase  = 0x740000000000ULL;
    VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
    VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

    /*
     *  - PCIe RCA2 Device memory
     */
    VirtualMemoryTable[++Index].PhysicalBase = 0x780000000000ULL;
    VirtualMemoryTable[Index].VirtualBase  = 0x780000000000ULL;
    VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
    VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

    /*
     *  - PCIe RCA3 Device memory
     */
    VirtualMemoryTable[++Index].PhysicalBase = 0x7C0000000000ULL;
    VirtualMemoryTable[Index].VirtualBase  = 0x7C0000000000ULL;
    VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
    VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

    /*
     *  - PCIe RCB0 Device memory
     */
    VirtualMemoryTable[++Index].PhysicalBase = 0x600000000000ULL;
    VirtualMemoryTable[Index].VirtualBase  = 0x600000000000ULL;
    VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
    VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

    /*
     *  - PCIe RCB1 Device memory
     */
    VirtualMemoryTable[++Index].PhysicalBase = 0x640000000000ULL;
    VirtualMemoryTable[Index].VirtualBase  = 0x640000000000ULL;
    VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
    VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

    /*
     *  - PCIe RCB2 Device memory
     */
    VirtualMemoryTable[++Index].PhysicalBase = 0x680000000000ULL;
    VirtualMemoryTable[Index].VirtualBase  = 0x680000000000ULL;
    VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
    VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

    /*
     *  - PCIe RCB3 Device memory
     */
    VirtualMemoryTable[++Index].PhysicalBase = 0x6C0000000000ULL;
    VirtualMemoryTable[Index].VirtualBase  = 0x6C0000000000ULL;
    VirtualMemoryTable[Index].Length       = 0x40000000000ULL;
    VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
  }

  /*
   *  - 2P/PCIe Socket1 RCA0 32-bit Device memory
   *  - 1P/PCIe consolidated to RCA2 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x60000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x60000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket1 RCA1 32-bit Device memory
   *  - 1P/PCIe consolidated to RCA2 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x68000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x68000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket1 RCA2 32-bit Device memory
   *  - 1P/PCIe consolidated to RCA3 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x70000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x70000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket1 RCA3 32-bit Device memory
   *  - 1P/PCIe consolidated to RCA3 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x78000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x78000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket1 RCB0 32-bit Device memory
   *  - 1P/PCIe consolidated to RCA0 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x40000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x40000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket1 RCB1 32-bit Device memory
   *  - 1P/PCIe consolidated to RCA0 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x48000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x48000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket1 RCB2 32-bit Device memory
   *  - 1P/PCIe consolidated to RCA1 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x50000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x50000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - 2P/PCIe Socket1 RCB3 32-bit Device memory
   *  - 1P/PCIe consolidated to RCA1 32-bit Device memory
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x58000000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x58000000ULL;
  VirtualMemoryTable[Index].Length       = 0x8000000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   *  - BERT memory region
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x88230000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x88230000ULL;
  VirtualMemoryTable[Index].Length       = 0x50000ULL;
  VirtualMemoryTable[Index].Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  /*
   * TPM CRB address - Attribute has to be Uncached
   */
  VirtualMemoryTable[++Index].PhysicalBase = 0x88500000ULL;
  VirtualMemoryTable[Index].VirtualBase  = 0x88500000ULL;
  VirtualMemoryTable[Index].Length       = 0x100000ULL;
  VirtualMemoryTable[Index].Attributes   = DDR_ATTRIBUTES_UNCACHED;

  /*
   *  - DDR memory region
   */
  NumRegion = PlatformHob->DramInfo.NumRegion;
  Count = 0;
  while (NumRegion-- > 0) {
    if (PlatformHob->DramInfo.NvdRegion[Count]) { /* Skip NVDIMM Region */
      Count++;
      continue;
    }

    VirtualMemoryTable[++Index].PhysicalBase = PlatformHob->DramInfo.Base[Count];
    VirtualMemoryTable[Index].VirtualBase  = PlatformHob->DramInfo.Base[Count];
    VirtualMemoryTable[Index].Length       = PlatformHob->DramInfo.Size[Count];
    VirtualMemoryTable[Index].Attributes   = DDR_ATTRIBUTES_CACHED;
    Count++;
  }

  /* SPM MM NS Buffer for MmCommunicateDxe */
  VirtualMemoryTable[++Index].PhysicalBase = PcdGet64 (PcdMmBufferBase);
  VirtualMemoryTable[Index].VirtualBase  = PcdGet64 (PcdMmBufferBase);
  VirtualMemoryTable[Index].Length       = PcdGet64 (PcdMmBufferSize);
  VirtualMemoryTable[Index].Attributes   = DDR_ATTRIBUTES_CACHED;

  /* End of Table */
  VirtualMemoryTable[++Index].PhysicalBase = 0;
  VirtualMemoryTable[Index].VirtualBase  = 0;
  VirtualMemoryTable[Index].Length       = 0;
  VirtualMemoryTable[Index].Attributes   = (ARM_MEMORY_REGION_ATTRIBUTES)0;

  ASSERT ((Index + 1) <= MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS);

  *VirtualMemoryMap = VirtualMemoryTable;
}
