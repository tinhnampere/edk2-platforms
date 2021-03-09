/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <PiPei.h>

#include <Library/ArmLib.h>
#include <Library/ArmPlatformLib.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PeiServicesLib.h>

EFI_STATUS
EFIAPI
PlatformPeim (
  VOID
  )
{
  UINT64 FvMainBase;
  UINT32 FvMainSize;

  ASSERT (FixedPcdGet32 (PcdFvMainCoreSize) != 0);

  // Build FV_MAIN Hand-off block (HOB) to let DXE IPL pick up correctly
  FvMainBase = FixedPcdGet64 (PcdFvMainCoreBaseAddress);
  FvMainSize = FixedPcdGet32 (PcdFvMainCoreSize);
  BuildFvHob (FvMainBase, FvMainSize);
  PeiServicesInstallFvInfoPpi (
    &(((EFI_FIRMWARE_VOLUME_HEADER *)ReadUnaligned64 (&FvMainBase)))->FileSystemGuid,
    (VOID *)(UINTN)ReadUnaligned64 (&FvMainBase),
    FvMainSize,
    NULL,
    NULL
    );

  return EFI_SUCCESS;
}
