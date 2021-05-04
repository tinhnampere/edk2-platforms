/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef AC01_PCIE_LIB_H_
#define AC01_PCIE_LIB_H_

#include <Library/PciHostBridgeLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>

/**
  Get RootBridge disable status.

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.
  @param[in]  RBIndex               Index to identify of underneath PCIE Root bridge.

  @retval BOOLEAN                   Return RootBridge disable status.
**/
BOOLEAN
Ac01PcieCheckRootBridgeDisabled (
  IN UINTN HBIndex,
  IN UINTN RBIndex
  );

/**
  Prepare to start PCIE core BSP driver

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
Ac01PcieSetup (
  VOID
  );

/**
  Prepare to end PCIE core BSP driver.
**/
VOID
Ac01PcieEnd (
  VOID
  );

/**
  Get Total HostBridge.

  @retval UINTN                     Return Total HostBridge.
**/
UINT8
Ac01PcieGetTotalHBs (
  VOID
  );

/**
  Get Total RootBridge per HostBridge.

  @param[in]  RCIndex               Index to identify of Root Complex.

  @retval UINTN                     Return Total RootBridge per HostBridge.
**/
UINT8
Ac01PcieGetTotalRBsPerHB (
  IN UINTN RCIndex
  );

/**
  Get RootBridge Attribute.

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.
  @param[in]  RBIndex               Index to identify of underneath PCIE Root bridge.

  @retval UINTN                     Return RootBridge Attribute.
**/
UINTN
Ac01PcieGetRootBridgeAttribute (
  IN UINTN HBIndex,
  IN UINTN RBIndex
  );

/**
  Get RootBridge Segment number

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.
  @param[in]  RBIndex               Index to identify of underneath PCIE Root bridge.

  @retval UINTN                     Return RootBridge Segment number.
**/
UINTN
Ac01PcieGetRootBridgeSegmentNumber (
  IN UINTN HBIndex,
  IN UINTN RBIndex
  );

/**
  Initialize Host bridge

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
Ac01PcieSetupHostBridge (
  IN UINTN HBIndex
  );

/**
  Initialize Root bridge

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.
  @param[in]  RBIndex               Index to identify of underneath PCIE Root bridge.
  @param[in]  RootBridgeInstance    The pointer of instance of the Root bridge IO.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
Ac01PcieSetupRootBridge (
  IN UINTN           HBIndex,
  IN UINTN           RBIndex,
  IN PCI_ROOT_BRIDGE *RootBridge
  );

/**
  Reads/Writes an PCI configuration register.

  @param[in]  RootInstance          Pointer to RootInstance structure.
  @param[in]  Address               Address which want to read or write to.
  @param[in]  Write                 Indicate that this is a read or write command.
  @param[in]  Width                 Specify the width of the data.
  @param[in, out]  Data             The buffer to hold the data.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
Ac01PcieConfigRW (
  IN     VOID    *RootInstance,
  IN     UINT64  Address,
  IN     BOOLEAN Write,
  IN     UINTN   Width,
  IN OUT VOID    *Data
  );

/**
  Callback funciton for EndEnumeration notification from PCI stack.

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.
  @param[in]  RBIndex               Index to identify of underneath PCIE Root bridge.
  @param[in]  Phase                 The phase of enumeration as informed from PCI stack.
**/
VOID
Ac01PcieHostBridgeNotifyPhase (
  IN UINTN                                         HBIndex,
  IN UINTN                                         RBIndex,
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE Phase
  );

#endif /* AC01_PCIE_LIB_H_ */
