/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PCIE_BOARD_LIB_H_
#define PCIE_BOARD_LIB_H_

#include "Pcie.h"

/**

  Check if a PCIE port enabled in the board configuration.

  @param  Index                     The port index.

  @retval                           TRUE if the port enabled, otherwise FALSE.

**/
BOOLEAN
PcieBoardCheckSysSlotEnabled (
  IN UINTN Index
  );

VOID
PcieBoardGetRCSegmentNumber (
  IN  AC01_RC *RC,
  OUT UINTN   *SegmentNumber
  );

/**

  Check if SMM PMU enabled in board screen

  @retval                           TRUE if the SMMU PMU enabled, otherwise FALSE.

**/
BOOLEAN
PcieBoardCheckSmmuPmuEnabled (
  VOID
  );

/**

  Build UEFI menu.

  @param  ImageHandle               Handle.
  @param  SystemTable               Pointer to System Table.
  @param  RCList                    List of Root Complex with properties.

  @retval                           EFI_SUCCESS if successful, otherwise EFI_ERROR.

**/
EFI_STATUS
PcieBoardScreenInitialize (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN AC01_RC          *RCList
  );

BOOLEAN
IsEmptyRC (
  IN AC01_RC *RC
  );

VOID
PcieBoardSetupDevmap (
  IN AC01_RC *RC
  );

VOID
PcieBoardGetLaneAllocation (
  IN AC01_RC *RC
  );

VOID
PcieBoardGetSpeed (
  IN AC01_RC *RC
  );

VOID
PcieBoardParseRCParams (
  IN AC01_RC *RC
  );

VOID
PcieBoardAssertPerst (
  AC01_RC *RC,
  UINT32  PcieIndex,
  UINT8   Bifurcation,
  BOOLEAN isPullToHigh
  );

BOOLEAN
PcieBoardCheckSmmuPmuEnabled (
  VOID
  );

#endif /* PCIE_BOARD_LIB_H_ */
