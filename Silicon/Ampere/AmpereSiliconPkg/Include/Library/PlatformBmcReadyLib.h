/** @file

  Copyright (c) 2020 - 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_BMC_READY_LIB_H_
#define PLATFORM_BMC_READY_LIB_H_

/**
  This function check BMC ready via GPIO

  @return The status of BMC

**/
BOOLEAN
EFIAPI
PlatformBmcReady (
  VOID
  );

#endif /* PLATFORM_BMC_READY_LIB_H_ */
