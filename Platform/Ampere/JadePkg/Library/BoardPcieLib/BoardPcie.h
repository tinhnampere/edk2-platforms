/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef BOARD_PCIE_H_
#define BOARD_PCIE_H_

#include <Ac01PcieCommon.h>

#define BITS_PER_BYTE           8
#define BYTE_MASK               0xFF
#define PCIE_ERRATA_SPEED1      0x0001 // Limited speed errata

enum DEV_MAP_MODE {
  DEV_MAP_1_CONTROLLER = 0,
  DEV_MAP_2_CONTROLLERS,
  DEV_MAP_3_CONTROLLERS,
  DEV_MAP_4_CONTROLLERS,
};

BOOLEAN
IsEmptyRC (
  IN AC01_RC *RC
  );

VOID
BoardPcieSetupDevmap (
  IN AC01_RC *RC
  );

VOID
BoardPcieGetLaneAllocation (
  IN AC01_RC *RC
  );

VOID
BoardPcieGetSpeed (
  IN AC01_RC *RC
  );

#endif /* BOARD_PCIE_H_ */
