/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _WATCHDOG_CONFIG_HII_H_
#define _WATCHDOG_CONFIG_HII_H_

#define WATCHDOG_CONFIG_FORMSET_GUID \
  { \
    0xC3F8EC6E, 0x95EE, 0x460C, { 0xA4, 0x8D, 0xEA, 0x54, 0x2F, 0xFF, 0x01, 0x61 } \
  }

extern EFI_GUID gWatchdogConfigFormSetGuid;

#endif /* _WATCHDOG_CONFIG_HII_H_ */
