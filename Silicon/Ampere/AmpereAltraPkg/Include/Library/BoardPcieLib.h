/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef BOARD_PCIE_LIB_H_
#define BOARD_PCIE_LIB_H_

#include <Ac01PcieCommon.h>

/**
  Override the segment number for a root complex with a board specific number.

  @param[in]  RC                    Root Complex instance with properties.
  @param[out] SegmentNumber         Return segment number.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
EFIAPI
BoardPcieGetRCSegmentNumber (
  IN  AC01_RC *RC,
  OUT UINTN   *SegmentNumber
  );

/**
  Check if SMM PMU enabled in board screen.

  @param[out]  IsSmmuPmuEnabled     TRUE if the SMMU PMU enabled, otherwise FALSE.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
EFIAPI
BoardPcieCheckSmmuPmuEnabled (
  OUT BOOLEAN *IsSmmuPmuEnabled
  );

/**
  Build PCIe menu screen.

  @param[in]  RCList                List of Root Complex with properties.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
EFIAPI
BoardPcieScreenInitialize (
  IN AC01_RC *RCList
  );

/**
  Parse platform Root Complex information.

  @param[out]  RC                   Root Complex instance to store the platform information.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
EFIAPI
BoardPcieParseRCParams (
  OUT AC01_RC *RC
  );

/**
  Assert PERST of input PCIe controller

  @param[in]  RC                    Root Complex instance.
  @param[in]  PcieIndex             PCIe controller index of input Root Complex.
  @param[in]  Bifurcation           Bifurcation mode of input Root Complex.
  @param[in]  IsPullToHigh          Target status for the PERST.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
EFIAPI
BoardPcieAssertPerst (
  IN AC01_RC *RC,
  IN UINT32  PcieIndex,
  IN UINT8   Bifurcation,
  IN BOOLEAN IsPullToHigh
  );

#endif /* BOARD_PCIE_LIB_H_ */
