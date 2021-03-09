/** @file

   Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

   SPDX-License-Identifier: BSD-2-Clause-Patent

 **/

#ifndef PCIE_HOTPLUG_H_
#define PCIE_HOTPLUG_H_

#define HOTPLUG_GUID            { \
                                  0x5598273c, 0x11ea, 0xa496, \
                                  { 0x42, 0x02, 0x37, 0xbb, 0x02, 0x00, 0x13, 0xac } \
                                }

#define ALERT_IRQ_CMD           1 /* Alert IRQ */
#define HOTPLUG_START_CMD       2 /* Stat monitor event */
#define HOTPLUG_CHG_CMD         3 /* Indicate PCIE port change state explicitly */
#define LED_CMD                 4 /* Control LED state */
#define PORTMAP_CLR_CMD         5 /* Clear all port map */
#define PORTMAP_SET_CMD         6 /* Set port map */
#define PORTMAP_LOCK_CMD        7 /* Lock port map */
#define GPIOMAP_CMD             8 /* Set GPIO reset map */

#define LED_FAULT               1 /* LED_CMD: LED type - Fault */
#define LED_ATT                 2 /* LED_CMD: LED type - Attention */

#define LED_SET_ON              1
#define LED_SET_OFF             2
#define LED_SET_BLINK           3

VOID
PcieHotPlugStart (
  VOID
  );

#endif
