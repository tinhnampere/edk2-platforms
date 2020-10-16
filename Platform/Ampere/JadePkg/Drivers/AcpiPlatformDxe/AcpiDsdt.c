/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AcpiPlatform.h"

STATIC VOID
AcpiPatchCmn600 (VOID)
{
  CHAR8     NodePath[MAX_ACPI_NODE_PATH];
  UINTN     Index;

  for (Index = 0; Index < GetNumberSupportedSockets (); Index++) {
    AsciiSPrint (NodePath, sizeof(NodePath), "\\_SB.CMN%1X._STA", Index);
    if (GetNumberActiveCPMsPerSocket (Index)) {
      AcpiDSDTSetNodeStatusValue (NodePath, 0xf);
    } else {
      AcpiDSDTSetNodeStatusValue (NodePath, 0x0);
    }
  }
}

STATIC VOID
AcpiPatchDmc620 (VOID)
{
  CHAR8                 NodePath[MAX_ACPI_NODE_PATH];
  UINTN                 Index, Index1;
  EFI_GUID              PlatformHobGuid = PLATFORM_INFO_HOB_GUID_V2;
  PlatformInfoHob_V2    *PlatformHob;
  UINT32                McuMask;
  VOID                  *Hob;

  Hob = GetFirstGuidHob (&PlatformHobGuid);
  if (!Hob) {
    return;
  }
  PlatformHob = (PlatformInfoHob_V2 *) GET_GUID_HOB_DATA (Hob);

  for (Index = 0; Index < GetNumberSupportedSockets (); Index++) {
    McuMask = PlatformHob->DramInfo.McuMask[Index];
    for (Index1 = 0; Index1 < sizeof (McuMask) * 8; Index1++) {
      AsciiSPrint (NodePath, sizeof(NodePath), "\\_SB.MC%1X%1X._STA", Index, Index1);
      if (McuMask & (0x1 << Index1)) {
        AcpiDSDTSetNodeStatusValue (NodePath, 0xf);
      } else {
        AcpiDSDTSetNodeStatusValue (NodePath, 0x0);
      }
    }
  }
}

STATIC VOID
AcpiPatchNvdimm (VOID)
{
  CHAR8                 NodePath[MAX_ACPI_NODE_PATH];
  UINTN                 NvdRegionNum, Count;
  EFI_GUID              PlatformHobGuid = PLATFORM_INFO_HOB_GUID_V2;
  PlatformInfoHob_V2    *PlatformHob;
  VOID                  *Hob;

  Hob = GetFirstGuidHob (&PlatformHobGuid);
  if (!Hob) {
    return;
  }
  PlatformHob = (PlatformInfoHob_V2 *) GET_GUID_HOB_DATA (Hob);

  NvdRegionNum = 0;
  for (Count = 0; Count < PlatformHob->DramInfo.NumRegion; Count++) {
    if (PlatformHob->DramInfo.NvdRegion[Count]) {
      NvdRegionNum += 1;
    }
  }

  if (NvdRegionNum == PLATFORM_MAX_NUM_NVDIMM_DEVICE) {
    return;
  }

  /* Disable NVDIMM Root Device */
  if (NvdRegionNum == 0) {
    AsciiSPrint (NodePath, sizeof(NodePath), "\\_SB.NVDR._STA");
    AcpiDSDTSetNodeStatusValue (NodePath, 0x0);
  }

  /* Disable NVDIMM Device which is not available */
  Count = NvdRegionNum + 1;
  while (Count <= PLATFORM_MAX_NUM_NVDIMM_DEVICE) {
    AsciiSPrint (NodePath, sizeof(NodePath), "\\_SB.NVDR.NVD%1X._STA", Count);
    AcpiDSDTSetNodeStatusValue (NodePath, 0x0);
    Count++;
  }
}

STATIC VOID
AcpiPatchHwmon (VOID)
{
  CHAR8     NodePath[MAX_ACPI_NODE_PATH];
  UINTN     Index;

  // PCC Hardware Monitor Devices
  for (Index = 0; Index < GetNumberSupportedSockets (); Index++) {
    AsciiSPrint (NodePath, sizeof(NodePath), "\\_SB.HM0%1X._STA", Index);
    if (GetNumberActiveCPMsPerSocket (Index)) {
      AcpiDSDTSetNodeStatusValue (NodePath, 0xf);
    } else {
      AcpiDSDTSetNodeStatusValue (NodePath, 0x0);
    }
  }

  // Ampere Altra SoC Hardware Monitor Devices
  for (Index = 0; Index < GetNumberSupportedSockets (); Index++) {
    AsciiSPrint (NodePath, sizeof(NodePath), "\\_SB.HM0%1X._STA", Index + 2);
    if (GetNumberActiveCPMsPerSocket (Index)) {
      AcpiDSDTSetNodeStatusValue (NodePath, 0xf);
    } else {
      AcpiDSDTSetNodeStatusValue (NodePath, 0x0);
    }
  }
}

STATIC VOID
AcpiPatchDsu (VOID)
{
  CHAR8                 NodePath[MAX_ACPI_NODE_PATH];
  UINTN                 Index;

  /*
   * The following code may take very long time in emulator. Just leave all Dsu enabled in
   * emulation mode as they only associate with enabled core in OS
   */
#if Emag2Emulator_SUPPORT
  return;
#endif

  for (Index = 0; Index < PLATFORM_CPU_MAX_NUM_CORES; Index += PLATFORM_CPU_NUM_CORES_PER_CPM) {
    AsciiSPrint (NodePath, sizeof(NodePath), "\\_SB.DU%2X._STA", Index / PLATFORM_CPU_NUM_CORES_PER_CPM);
    if (IsCpuEnabled (Index)) {
      AcpiDSDTSetNodeStatusValue (NodePath, 0xf);
    } else {
      AcpiDSDTSetNodeStatusValue (NodePath, 0x0);
    }
  }
}

EFI_STATUS
AcpiPatchDsdtTable (VOID)
{
  AcpiPatchCmn600 ();
  AcpiPatchDmc620 ();
  AcpiPatchDsu ();
  AcpiPatchHwmon ();
  AcpiPatchNvdimm ();

  return EFI_SUCCESS;
}
