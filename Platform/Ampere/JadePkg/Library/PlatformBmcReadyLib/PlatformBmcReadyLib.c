/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/GpioLib.h>
#include <Library/PcdLib.h>

//
// Platform Specific
//
#define BMC_READY_GPIO  (FixedPcdGet8 (PcdBmcReadyGpio))

/**
  This function check BMC ready via GPIO

  @return The status of BMC

**/
BOOLEAN
EFIAPI
PlatformBmcReady (
  VOID
  )
{
  return GpioReadBit (BMC_READY_GPIO) == 0x1;
}
