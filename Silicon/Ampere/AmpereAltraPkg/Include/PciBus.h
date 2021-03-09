/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PCI_BUS_H_
#define PCI_BUS_H_

/**
 * This ENUM value definitions used to identify PCI Device different Resource types.
 *
 * Fields:
 *  Name            Type                    Description
 *  ------------------------------------------------------------------
 *  rtBus           ENUM                Bus resources
 *  rtIo16          ENUM                I/O 16bit Resources
 *  rtIo32          ENUM                I/O 32bit Resources
 *  rtMmio32        ENUM                MMIO 32bit Resources
 *  rtMmio32p       ENUM                Prefetchable MMIO 32bit
 *  rtMmio64        ENUM                MMIO 64bit Resources
 *  rtMmio64p       ENUM                Prefetchable MMIO 64bit
 *  rtMaxRes        ENUM                Last valid value of this ENUM
 **/
typedef enum {
  rtBus=0,
  rtIo16,        // 1
  rtIo32,        // 2
  rtMmio32,      // 3
  rtMmio32p,     // 4
  rtMmio64,      // 5
  rtMmio64p,     // 6
  rtMaxRes
} MRES_TYPE;

#endif /* PCI_BUS_H_ */
