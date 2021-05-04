/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_PCIE_HELPER_H_
#define PLATFORM_PCIE_HELPER_H_

#define PCIE_MAX_PAYLOAD_MASK         0x07
#define PCIE_CONTROL_MAX_PAYLOAD_OFF  5
#define PCIE_MAX_READ_REQUEST_MASK    0x07
#define PCIE_CONTROL_READ_REQUEST_OFF 12

#define PCI_EXPRESS_CAPABILITY_DEVICE_CAPABILITIES_REG 0x04
#define PCI_EXPRESS_CAPABILITY_DEVICE_CONTROL_REG      0x08

#define FOR_EACH(Node, Tail, Type) \
        for (Node = Tail->Type; Node != NULL; Node = Node->Type)

struct _PCIE_NODE {
  EFI_PCI_IO_PROTOCOL *PciIo;
  UINT8               MaxMps;
  UINT8               PcieCapOffset;
  UINT16              Vid;
  UINT16              Did;
  UINT8               Seg;
  UINT8               Bus;
  UINT8               Dev;
  UINT8               Fun;
  struct _PCIE_NODE   *Parent;
  struct _PCIE_NODE   *Brother;
};

typedef struct _PCIE_NODE PCIE_NODE;

EFI_STATUS
WriteMps (
  PCIE_NODE *Node,
  UINT8     Value
  );

EFI_STATUS
WriteMrr (
  PCIE_NODE *Node,
  UINT8     Value
  );

EFI_STATUS
FindCapabilityPtr (
  IN  EFI_PCI_IO_PROTOCOL *PciIo,
  IN  UINT8               CapabilityId,
  OUT UINT8               *CapabilityPtr
  );

#endif // PLATFORM_PCIE_HELPER_H_
