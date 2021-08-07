/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Uefi.h>

#include <Guid/PlatformInfoHobGuid.h>
#include <Library/AmpereCpuLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <PlatformInfoHob.h>

VOID *mPlatformInfoHob = NULL;

/**
  Get the platform HOB data.

  @return   PLATFORM_INFO_HOB   The pointer to the platform HOB data.

**/
PLATFORM_INFO_HOB *
GetPlatformHob (
  VOID
  )
{
  if (mPlatformInfoHob == NULL) {
    mPlatformInfoHob = GetNextGuidHob (
                         &gPlatformHobGuid,
                         (CONST VOID *)FixedPcdGet64 (PcdSystemMemoryBase)
                         );
    if (mPlatformInfoHob == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get gPlatformHobGuid!\n", __FUNCTION__));
      return NULL;
    }
 }

  return ((PLATFORM_INFO_HOB *)GET_GUID_HOB_DATA (mPlatformInfoHob));
}
