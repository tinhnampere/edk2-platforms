/** @file

  Copyright (c) 2021 - 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <IndustryStandard/Pci.h>
#include <Protocol/PciIo.h>

#include "PcieDeviceConfigDxe.h"
#include "PcieHelper.h"

#define AC01_PCIE_ENABLE_SERR        BIT8
#define AC01_PCIE_BRIDGE_ENABLE_SERR BIT1

EFI_STATUS
FindCapabilityPtr (
  IN  EFI_PCI_IO_PROTOCOL *PciIo,
  IN  UINT8               CapabilityId,
  OUT UINT8               *CapabilityPtr
  )
{
  EFI_STATUS Status;
  UINT8      NextPtr;
  UINT16     TmpValue;

  ASSERT (PciIo != NULL);

  //
  // Get pointer to first PCI Capability header
  //
  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        PCI_CAPBILITY_POINTER_OFFSET,
                        1,
                        &NextPtr
                        );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  while (TRUE) {
    if (NextPtr == 0x00) {
      Status = EFI_NOT_FOUND;
      goto Exit;
    }

    //
    // Retrieve PCI Capability header
    //
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint16,
                          NextPtr,
                          1,
                          &TmpValue
                          );
    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    if ((TmpValue & 0xFF) == CapabilityId) {
      *CapabilityPtr = NextPtr;
      Status = EFI_SUCCESS;
      goto Exit;
    }

    NextPtr = (TmpValue >> 8) & 0xFF;
  }

Exit:
  return Status;
}

EFI_STATUS
WriteMps (
  PCIE_NODE *Node,
  UINT8     Value
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
                        PcieCapOffset + PCI_EXPRESS_CAPABILITY_DEVICE_CONTROL_REG,
                        1,
                        &TmpValue
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Update value and write to device
  TmpValue = (TmpValue & ~(PCIE_MAX_PAYLOAD_MASK << PCIE_CONTROL_MAX_PAYLOAD_OFF))
             | Value << PCIE_CONTROL_MAX_PAYLOAD_OFF;
  Status = PciIo->Pci.Write (
                        PciIo,
                        EfiPciIoWidthUint16,
                        PcieCapOffset + PCI_EXPRESS_CAPABILITY_DEVICE_CONTROL_REG,
                        1,
                        &TmpValue
                        );
  if (EFI_ERROR (Status)) {
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
  PCIE_NODE *Node,
  UINT8     Value
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
                        PcieCapOffset + PCI_EXPRESS_CAPABILITY_DEVICE_CONTROL_REG,
                        1,
                        &TmpValue
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Update value and write to device
  TmpValue = (TmpValue & ~(PCIE_MAX_READ_REQUEST_MASK << PCIE_CONTROL_READ_REQUEST_OFF))
             | Value << PCIE_CONTROL_READ_REQUEST_OFF;
  Status = PciIo->Pci.Write (
                        PciIo,
                        EfiPciIoWidthUint16,
                        PcieCapOffset + PCI_EXPRESS_CAPABILITY_DEVICE_CONTROL_REG,
                        1,
                        &TmpValue
                        );
  if (EFI_ERROR (Status)) {
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

EFI_STATUS
SetSerr (
  PCIE_NODE *Node,
  UINT8     Value
  )
{
  EFI_PCI_IO_PROTOCOL  *PciIo;
  EFI_STATUS           Status;
  UINT16               TmpValue;
  UINT8                HeaderType;

  if (Node == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PciIo = Node->PciIo;

  //
  // Enable SERR
  //
  TmpValue = 0;
  Status   = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint16,
                          PCI_COMMAND_OFFSET,
                          1,
                          &TmpValue
                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: Failed to read Command register - %r\n", __FUNCTION__, Status));
    return Status;
  } else {
    if (Value == PCIE_SERR_ENABLE) {
      TmpValue |= AC01_PCIE_ENABLE_SERR;
    } else {
      TmpValue &= ~AC01_PCIE_ENABLE_SERR;
    }

    Status = PciIo->Pci.Write (
                          PciIo,
                          EfiPciIoWidthUint16,
                          PCI_COMMAND_OFFSET,
                          1,
                          &TmpValue
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: Failed to write Command register - %r\n", __FUNCTION__, Status));
      return Status;
    }
  }

  //
  // For Switch and Root Port, need to configure `Bridge Control` register
  //
  HeaderType = 0;
  Status     = PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint8,
                            PCI_HEADER_TYPE_OFFSET,
                            1,
                            &HeaderType
                            );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: Failed to read Header Type - %r\n", __FUNCTION__, Status));
    return Status;
  }

  if ((HeaderType & HEADER_LAYOUT_CODE) == HEADER_TYPE_PCI_TO_PCI_BRIDGE) {
    TmpValue = 0;
    Status   = PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint16,
                            PCI_BRIDGE_CONTROL_REGISTER_OFFSET,
                            1,
                            &TmpValue
                            );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: Failed to read Bridge Control Register - %r\n", __FUNCTION__, Status));
      return Status;
    } else {
      if (Value == PCIE_SERR_ENABLE) {
        TmpValue |= AC01_PCIE_BRIDGE_ENABLE_SERR;
      } else {
        TmpValue &= ~AC01_PCIE_BRIDGE_ENABLE_SERR;
      }

      Status = PciIo->Pci.Write (
                            PciIo,
                            EfiPciIoWidthUint16,
                            PCI_BRIDGE_CONTROL_REGISTER_OFFSET,
                            1,
                            &TmpValue
                            );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "%a: Failed to write Bridge Control Register - %r\n", __FUNCTION__, Status));
        return Status;
      }
    }
  }

  return EFI_SUCCESS;
}
