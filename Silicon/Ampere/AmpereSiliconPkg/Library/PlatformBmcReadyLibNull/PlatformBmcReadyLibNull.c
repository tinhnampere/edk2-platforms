/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

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
  // Set BMC always ready
  return TRUE;
}
