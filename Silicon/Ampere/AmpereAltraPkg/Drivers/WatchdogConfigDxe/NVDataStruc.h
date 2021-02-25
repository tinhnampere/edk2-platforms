/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef NVDATASTRUC_H_
#define NVDATASTRUC_H_

#include <Guid/WatchdogConfigHii.h>

#define WATCHDOG_CONFIG_VARSTORE_ID       0x1234
#define WATCHDOG_CONFIG_FORM_ID           0x1235

#define NWDT_UEFI_DEFAULT_VALUE           300  // 5 minutes
#define SWDT_DEFAULT_VALUE                300  // 5 minutes

#pragma pack(1)
typedef struct {
    UINT32 WatchdogTimerUEFITimeout;
    UINT32 SecureWatchdogTimerTimeout;
} WATCHDOG_CONFIG_VARSTORE_DATA;
#pragma pack()

#endif
