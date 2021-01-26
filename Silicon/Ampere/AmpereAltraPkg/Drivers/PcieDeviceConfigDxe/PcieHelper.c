/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/PciIo.h>

#include "PcieHelper.h"
#include "PcieDeviceConfigDxe.h"

EFI_STATUS
WriteMps (
  PCIE_NODE  *Node,
  UINT8      Value
  )
{
  EFI_PCI_IO_PROTOCOL *PciIo;
  EFI_STATUS          Status;
  UINT16              TmpValue;
  UINT8               PcieCapOffset;

  if (Node == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PciIo = Node->PciIo;
  PcieCapOffset = Node->PcieCapOffset;

  // Get current device control reg
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint16,
                        PcieCapOffset + PCIE_CONTROL_REG,
                        1,
                        &TmpValue
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Pci.Read error line 0x%d\n", __LINE__));
    return Status;
  }

  // Update value and write to device
  TmpValue = (TmpValue & ~(PCIE_MAX_PAYLOAD_MASK << PCIE_CONTROL_MAX_PAYLOAD_OFF))
              | Value << PCIE_CONTROL_MAX_PAYLOAD_OFF;
  Status = PciIo->Pci.Write (
                        PciIo,
                        EfiPciIoWidthUint16,
                        PcieCapOffset + PCIE_CONTROL_REG,
                        1,
                        &TmpValue
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Pci.Write error line 0x%d\n", __LINE__));
    return Status;
  }
  DEBUG ((
    DEBUG_INFO,
    "%a: Write MPS %d to device 0x%04x:0x%02x:0x%02x:0x%02x\n",
    __FUNCTION__,
    Value,
    Node->Seg,
    Node->Bus,
    Node->Dev,
    Node->Fun
    ));

  return EFI_SUCCESS;
}

EFI_STATUS
WriteMrr (
  PCIE_NODE  *Node,
  UINT8      Value
  )
{
  EFI_PCI_IO_PROTOCOL *PciIo;
  EFI_STATUS          Status;
  UINT16              TmpValue;
  UINT8               PcieCapOffset;

  if (Node == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PciIo = Node->PciIo;
  PcieCapOffset = Node->PcieCapOffset;

  // Get current device control reg
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint16,
                        PcieCapOffset + PCIE_CONTROL_REG,
                        1,
                        &TmpValue
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Pci.Read error line 0x%d\n", __LINE__));
    return Status;
  }

  // Update value and write to device
  TmpValue = (TmpValue & ~(PCIE_MAX_READ_REQUEST_MASK << PCIE_CONTROL_READ_REQUEST_OFF))
              | Value << PCIE_CONTROL_READ_REQUEST_OFF;
  Status = PciIo->Pci.Write (
                        PciIo,
                        EfiPciIoWidthUint16,
                        PcieCapOffset + PCIE_CONTROL_REG,
                        1,
                        &TmpValue
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Pci.Write error line 0x%d\n", __LINE__));
    return Status;
  }
  DEBUG ((
    DEBUG_INFO,
    "%a: Write MRR %d to device 0x%04x:0x%02x:0x%02x:0x%02x\n",
    __FUNCTION__,
    Value,
    Node->Seg,
    Node->Bus,
    Node->Dev,
    Node->Fun
    ));

  return EFI_SUCCESS;
}
