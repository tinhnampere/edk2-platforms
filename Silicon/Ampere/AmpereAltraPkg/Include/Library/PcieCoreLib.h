/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PCI_CORE_LIB_H_
#define PCI_CORE_LIB_H_

#include <Library/PciHostBridgeLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>

/**
  Get RootBridge disable status.

  @param  HBIndex[In]           Index to identify of PCIE Host bridge.
  @param  RBIndex[In]           Index to identify of underneath PCIE Root bridge.

  @retval BOOLEAN               Return RootBridge disable status.
**/
BOOLEAN
Ac01PcieCheckRootBridgeDisabled (
  IN UINTN HBIndex,
  IN UINTN RBIndex
  );

/**
  Prepare to start PCIE core BSP driver

  @param ImageHandle[in]        Handle for the image.
  @param SystemTable[in]        Address of the system table.

  @retval EFI_SUCCESS           Initialize successfully.
**/
EFI_STATUS
Ac01PcieSetup (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
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

  @retval UINTN                 Return Total HostBridge.
**/
UINT8
Ac01PcieGetTotalHBs (
  VOID
  );

/**
  Get Total RootBridge per HostBridge.

  @param  RCIndex[in]           Index to identify of Root Complex.

  @retval UINTN                 Return Total RootBridge per HostBridge.
**/
UINT8
Ac01PcieGetTotalRBsPerHB (
  IN UINTN RCIndex
  );

/**
  Get RootBridge Attribute.

  @param  HBIndex[in]           Index to identify of PCIE Host bridge.
  @param  RBIndex[in]           Index to identify of underneath PCIE Root bridge.

  @retval UINTN                 Return RootBridge Attribute.
**/
UINTN
Ac01PcieGetRootBridgeAttribute (
  IN UINTN HBIndex,
  IN UINTN RBIndex
  );

/**
  Get RootBridge Segment number

  @param  HBIndex[in]           Index to identify of PCIE Host bridge.
  @param  RBIndex[in]           Index to identify of underneath PCIE Root bridge.

  @retval UINTN                 Return RootBridge Segment number.
**/
UINTN
Ac01PcieGetRootBridgeSegmentNumber (
  IN UINTN HBIndex,
  IN UINTN RBIndex
  );

/**
  Initialize Host bridge

  @param  HBIndex[in]           Index to identify of PCIE Host bridge.

  @retval EFI_SUCCESS           Initialize successfully.
**/
EFI_STATUS
Ac01PcieSetupHostBridge (
  IN UINTN HBIndex
  );

/**
  Initialize Root bridge

  @param  HBIndex[in]            Index to identify of PCIE Host bridge.
  @param  RBIndex[in]            Index to identify of underneath PCIE Root bridge.
  @param  RootBridgeInstance[in] The pointer of instance of the Root bridge IO.

  @retval EFI_SUCCESS           Initialize successfully.
  @retval EFI_DEVICE_ERROR      Error when initializing.
**/
EFI_STATUS
Ac01PcieSetupRootBridge (
  IN UINTN           HBIndex,
  IN UINTN           RBIndex,
  IN PCI_ROOT_BRIDGE *RootBridge
  );

/**
  Reads/Writes an PCI configuration register.

  @param  RootInstance[in]      Pointer to RootInstance structure.
  @param  Address[in]           Address which want to read or write to.
  @param  Write[in]             Indicate that this is a read or write command.
  @param  Width[in]             Specify the width of the data.
  @param  Data[in, out]         The buffer to hold the data.

  @retval EFI_SUCCESS           Read/Write successfully.
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

  @param  HBIndex[in]           Index to identify of PCIE Host bridge.
  @param  RBIndex[in]           Index to identify of underneath PCIE Root bridge.
  @param  Phase[in]             The phase of enumeration as informed from PCI stack.
**/
VOID
Ac01PcieHostBridgeNotifyPhase (
  IN UINTN                                         HBIndex,
  IN UINTN                                         RBIndex,
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE Phase
  );

#endif /* PCI_CORE_LIB_H_ */
