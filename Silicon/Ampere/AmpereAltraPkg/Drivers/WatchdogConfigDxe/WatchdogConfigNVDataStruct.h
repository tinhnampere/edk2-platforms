/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef WATCHDOG_CONFIG_NV_DATA_STRUCT_H_
#define WATCHDOG_CONFIG_NV_DATA_STRUCT_H_

#include <Guid/WatchdogConfigHii.h>

#define WATCHDOG_CONFIG_VARSTORE_ID       0x1234
#define WATCHDOG_CONFIG_FORM_ID           0x1235

#define NWDT_UEFI_DEFAULT_VALUE           300 // 5 minutes
#define SWDT_DEFAULT_VALUE                300 // 5 minutes

#pragma pack(1)
typedef struct {
  UINT32 WatchdogTimerUEFITimeout;
  UINT32 SecureWatchdogTimerTimeout;
} WATCHDOG_CONFIG_VARSTORE_DATA;
#pragma pack()

#endif /* WATCHDOG_CONFIG_NV_DATA_STRUCT_H_ */
