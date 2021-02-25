/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef GENERIC_WATCHDOG_H_
#define GENERIC_WATCHDOG_H_

#include <Protocol/WatchdogTimer.h>

/* The number of 100ns periods (the unit of time passed to these functions)
   in a second */
#define TIME_UNITS_PER_SECOND           10000000

/**
  The function to install Watchdog timer protocol to the system

  @retval  Return         EFI_SUCCESS if install Watchdog timer protocol successfully.
 **/
EFI_STATUS
EFIAPI
WatchdogTimerInstallProtocol (
  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL **WatchdogTimerProtocol
  );

#endif /* GENERIC_WATCHDOG_H_ */
