/** @file
*
*  Copyright (c) 2021, Ampere Computing. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "UsbCdcEthernet.h"

/**
  Get CDC header descriptor

**/
EFI_STATUS
UsbCdcEnumFunctionalDescriptor (
  IN OUT USB_CDC_ETHERNET_PRIVATE_DATA   *PrivateData
  )
{
  EFI_STATUS                  Status;
  EFI_USB_IO_PROTOCOL         *UsbIo;
  EFI_USB_CONFIG_DESCRIPTOR   ConfigDesc;
  USB_DESC_HEAD               *Head;
  VOID                        *Buffer;
  UINT32                      TransferResult;
  BOOLEAN                     Start;
  BOOLEAN                     CdcDescriptorAvailable;
  UINT32                      Total;

  if (PrivateData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  UsbIo = PrivateData->UsbControlIo;
  ASSERT (UsbIo != NULL);

  //
  // Get config descriptors of USB device and walk through the all interfaces to
  // find all CDC Functional Interface descriptor.
  //
  Status = UsbIo->UsbGetConfigDescriptor (UsbIo, &ConfigDesc);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Buffer = AllocateZeroPool (ConfigDesc.TotalLength);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Targeting to get all CDC Functional Interface descriptors
  Status = UsbGetDescriptor (
             UsbIo,
             (UINT16)((USB_DESC_TYPE_CONFIG << 8) | (ConfigDesc.ConfigurationValue - 1)),
             0,
             ConfigDesc.TotalLength,
             Buffer,
             &TransferResult
             );
  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return Status;
  }

  Total = 0;
  Start = FALSE;
  CdcDescriptorAvailable = FALSE;
  Head  = (USB_DESC_HEAD *)Buffer;

  while (Total < ConfigDesc.TotalLength) {
    if (Head->Type == USB_DESC_TYPE_INTERFACE) {
      if ((((USB_INTERFACE_DESCRIPTOR *)Head)->InterfaceNumber == 0)
         && (((USB_INTERFACE_DESCRIPTOR *)Head)->AlternateSetting == 0)) {
        // CDC descriptors will be found in next iteration. Some USB devices store
        // CDC descriptors after endpoint descriptor, but it is not supported currently.
        Start = TRUE;
        goto NextDesc;
      }
    }

    if (Start && (Head->Type == (USB_REQ_TYPE_CLASS | USB_DESC_TYPE_INTERFACE))) {
      switch (((USB_CDC_HEADER_FUNCTIONAL_DESCRIPTOR *)Head)->DescriptorSubType) {
      case USB_CDC_HEADER_TYPE:
        CopyMem (
          &PrivateData->UsbCdcDesc.UsbCdcHeaderDesc,
          (USB_CDC_HEADER_FUNCTIONAL_DESCRIPTOR *)Head,
          sizeof (USB_CDC_HEADER_FUNCTIONAL_DESCRIPTOR)
          );

        CdcDescriptorAvailable = TRUE;
        goto NextDesc;

      case USB_CDC_UNION_TYPE:
        CopyMem (
          &PrivateData->UsbCdcDesc.UsbCdcUnionDesc,
          (USB_CDC_UNION_FUNCTIONAL_DESCRIPTOR *)Head,
          sizeof (USB_CDC_UNION_FUNCTIONAL_DESCRIPTOR)
          );
        goto NextDesc;

      case USB_CDC_ETHERNET_TYPE:
        CopyMem (
          &PrivateData->UsbCdcDesc.UsbCdcEtherDesc,
          (USB_CDC_ETHERNET_FUNCTIONAL_DESCRIPTOR *)Head,
          sizeof (USB_CDC_ETHERNET_FUNCTIONAL_DESCRIPTOR)
          );
        goto NextDesc;

      default:
        //
        // Other CDC types will be supported in the future.
        //
        DEBUG ((
          DEBUG_INFO,
          "%a: Ignoring descriptor Subtype Interface 0x%x\n",
          __FUNCTION__,
          ((USB_CDC_HEADER_FUNCTIONAL_DESCRIPTOR *)Head)->DescriptorSubType
          ));
        goto NextDesc;
      }
    }

NextDesc:
    Total = Total + (UINT16)Head->Len;
    Head  = (USB_DESC_HEAD *)((UINT8 *)Buffer + Total);
  }

  //
  // Cannot find CDC Header descriptor
  //
  if (!CdcDescriptorAvailable) {
    Status = EFI_UNSUPPORTED;
  }

  FreePool (Buffer);
  return Status;
}

/**
  Get BULK Endpoint descriptors

**/
EFI_STATUS
UsbCdcEnumBulkEndpointDescriptor (
  IN OUT USB_CDC_ETHERNET_PRIVATE_DATA   *PrivateData
  )
{
  EFI_STATUS                    Status;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;
  EFI_USB_ENDPOINT_DESCRIPTOR   EndpointDescriptor;
  BOOLEAN                       FoundIn;
  BOOLEAN                       FoundOut;
  UINTN                         Index;

  if (PrivateData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  UsbIo = PrivateData->UsbDataIo;
  ASSERT (UsbIo != NULL);

  //
  // Get number of endpoints
  //
  Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &InterfaceDescriptor);
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  //
  // Traverse endpoints to find bulk endpoint
  //
  FoundOut = FALSE;
  FoundIn = FALSE;

  for (Index = 0; Index < InterfaceDescriptor.NumEndpoints; Index++) {
    UsbIo->UsbGetEndpointDescriptor (
             UsbIo,
             Index,
             &EndpointDescriptor
             );

    if ((EndpointDescriptor.Attributes & (BIT0 | BIT1)) == USB_ENDPOINT_BULK) {
      if (!FoundOut && (EndpointDescriptor.EndpointAddress & BIT7) == 0) {
        CopyMem (
          &PrivateData->UsbCdcDesc.UsbCdcOutEndpointDesc,
          &EndpointDescriptor,
          sizeof (EndpointDescriptor)
          );
        FoundOut = TRUE;
      } else if (!FoundIn && (EndpointDescriptor.EndpointAddress & BIT7) == BIT7) {
        CopyMem (
          &PrivateData->UsbCdcDesc.UsbCdcInEndpointDesc,
          &EndpointDescriptor,
          sizeof (EndpointDescriptor)
          );
        FoundIn = TRUE;
      }
    }
  }

  // Need both BULK IN and OUT endpoints
  if (!FoundIn || !FoundOut) {
    return EFI_UNSUPPORTED;
  }

  return Status;

}

/**
  Get INTERRUPT Endpoint descriptor

**/
EFI_STATUS
UsbCdcEnumInterruptEndpointDescriptor (
  IN OUT USB_CDC_ETHERNET_PRIVATE_DATA   *PrivateData
  )
{
  EFI_STATUS                    Status;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;
  EFI_USB_ENDPOINT_DESCRIPTOR   EndpointDescriptor;
  BOOLEAN                       Found;
  UINTN                         Index;

  if (PrivateData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  UsbIo = PrivateData->UsbControlIo;
  ASSERT (UsbIo != NULL);

  // Get number of endpoints
  Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &InterfaceDescriptor);
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  //
  // Traverse endpoints to find interrupt endpoint IN
  //
  Found = FALSE;

  for (Index = 0; Index < InterfaceDescriptor.NumEndpoints; Index++) {
    Status = UsbIo->UsbGetEndpointDescriptor (UsbIo, Index, &EndpointDescriptor);
    if (EFI_ERROR (Status)) {
      return EFI_UNSUPPORTED;
    }

    if (((EndpointDescriptor.Attributes & (BIT0 | BIT1)) == USB_ENDPOINT_INTERRUPT)
        && ((EndpointDescriptor.EndpointAddress & USB_ENDPOINT_DIR_IN) != 0)) {
      // We only care interrupt endpoint here
      CopyMem (
        &PrivateData->UsbCdcDesc.UsbCdcNotiEndPointDesc,
        &EndpointDescriptor,
        sizeof (EndpointDescriptor)
        );
      Found = TRUE;
      break;
    }
  }

  if (!Found) {
    return EFI_UNSUPPORTED;
  }

  return Status;
}

/**
  Get the MAC address from endpoint descriptor.

  @param[in]  PrivateData    Point to USB_CDC_ETHERNET_PRIVATE_DATA structure.
  @param[out] MacAddress     Address of a six byte buffer to receive the MAC address.

  @retval EFI_SUCCESS        The MAC address is available.
  @retval other              The MAC address is not valid.

**/
EFI_STATUS
UsbCdcMacAddressGet (
  IN  USB_CDC_ETHERNET_PRIVATE_DATA   *PrivateData,
  OUT UINT8                           *MacAddress
  )
{
  EFI_USB_IO_PROTOCOL                     *UsbIo;
  USB_CDC_ETHERNET_FUNCTIONAL_DESCRIPTOR  *CdcEtherDesc;
  CHAR16                                  *MacAddressStr;
  EFI_STATUS                              Status;

  if (PrivateData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  UsbIo = PrivateData->UsbControlIo;
  ASSERT (UsbIo != NULL);

  CdcEtherDesc = &PrivateData->UsbCdcDesc.UsbCdcEtherDesc;

  //
  // Get USB String Descriptor from the iMACAddress
  //
  Status = UsbIo->UsbGetStringDescriptor (
                    UsbIo,
                    USB_LANG_ID, // Consider using protocol to get LangId
                    CdcEtherDesc->MACAddress,
                    &MacAddressStr
                    );

  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  StrHexToBytes (
    MacAddressStr,
    StrLen (MacAddressStr),
    MacAddress,
    StrLen (MacAddressStr) / 2
    );

  return Status;

}

EFI_STATUS
UsbCdcSelectAltSetting (
  IN EFI_USB_IO_PROTOCOL    *UsbIo,
  IN UINTN                  InterfaceNumber,
  IN UINTN                  AltSettingIndex
  )
{
  EFI_STATUS                Status;
  UINT32                    UsbStatus;
  EFI_USB_DEVICE_REQUEST    Request;

  if (UsbIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&Request, sizeof (Request));
  Request.RequestType = USB_DEV_SET_INTERFACE_REQ_TYPE;
  Request.Request = USB_REQ_SET_INTERFACE;
  Request.Index = InterfaceNumber;
  Request.Value = (UINT16)AltSettingIndex;

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbNoData,
                    USB_CDC_ECM_CONTROL_TRANSFER_TIMEOUT,
                    NULL,
                    0,
                    &UsbStatus
                    );

  if (EFI_ERROR (Status)) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to select alt setting %d on interface (code %r, USB status x%x).\n",
      __FUNCTION__,
      AltSettingIndex,
      Status,
      UsbStatus
      ));
  }

  return Status;
}

EFI_STATUS
UsbCdcGetAltSetting (
  IN  EFI_USB_IO_PROTOCOL    *UsbIo,
  IN  UINTN                  InterfaceNumber,
  OUT UINTN                  *AltSettingIndex
  )
{
  EFI_STATUS                Status;
  UINT32                    UsbStatus;
  EFI_USB_DEVICE_REQUEST    Request;

  if (UsbIo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (&Request, sizeof (Request));
  Request.RequestType = USB_DEV_GET_INTERFACE_REQ_TYPE;
  Request.Request = USB_REQ_GET_INTERFACE;
  Request.Index = InterfaceNumber;
  Request.Length = 1;

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbDataIn,
                    USB_CDC_ECM_CONTROL_TRANSFER_TIMEOUT,
                    AltSettingIndex,
                    1,
                    &UsbStatus
                    );

  if (EFI_ERROR (Status)) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get alt setting on interface (code %r, USB status x%x).\n",
      __FUNCTION__,
      Status,
      UsbStatus
      ));
  }

  return Status;
}

/**
  Read connection status from interrupt endpoint.

  @param [in] PrivateData     Point to USB_CDC_ETHERNET_PRIVATE_DATA structure.

  @return EFI_SUCCESS         USB Link Status notification returns.
  @return EFI_UNSUPPORTED     Other notification types return.
  @return otherwise           UsbSyncInterruptTransfer returns IO error.

**/
EFI_STATUS
UsbCdcGetLinkStatus (
  IN USB_CDC_ETHERNET_PRIVATE_DATA *PrivateData
  )
{
  UINT32                 TransferStatus;
  EFI_USB_IO_PROTOCOL    *UsbIo;
  EFI_STATUS             Status;
  EFI_USB_DEVICE_REQUEST Request;
  UINTN                  RequestLength;
  USB_CDC_NOTIFICATION   *Event;

  if (PrivateData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  UsbIo = PrivateData->UsbControlIo;
  ASSERT (UsbIo != NULL);

  ZeroMem (&Request, sizeof (Request));
  RequestLength = sizeof (EFI_USB_DEVICE_REQUEST);

  //
  // Issue the command
  //
  Status = UsbIo->UsbSyncInterruptTransfer (
                    UsbIo,
                    PrivateData->UsbCdcDesc.UsbCdcNotiEndPointDesc.EndpointAddress,
                    &Request,
                    &RequestLength,
                    USB_CDC_ECM_CONTROL_TRANSFER_TIMEOUT,
                    &TransferStatus
                    );

  if (EFI_ERROR (Status)
      || EFI_ERROR (TransferStatus)
      || 0 == RequestLength) {
    return Status;
  }

  Event = (USB_CDC_NOTIFICATION *)&Request;
  switch (Event->NotificationType) {
  case USB_CDC_NOTIFY_NETWORK_CONNECTION:
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: Notify Network Connection: Event->Value = %d \n",
      __FUNCTION__,
      Event->Value
      ));
    PrivateData->LinkUp = !!Event->Value;
    Status = EFI_SUCCESS;
    break;

  case USB_CDC_NOTIFY_SPEED_CHANGE:
    DEBUG ((DEBUG_VERBOSE, "%a: Notify Speed Change. Unsupported!\n", __FUNCTION__));
    Status = EFI_UNSUPPORTED;
    break;

  default:
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: Unexpected Notification Type %02x!\n",
      __FUNCTION__,
      Event->NotificationType
      ));
    Status = EFI_UNSUPPORTED;
    break;
  }

  return Status;
}

/**
  Write to control Endpoint to update Packet Filter Setting.

  @param[in]  PrivateData       Points to USB_CDC_ETHERNET_PRIVATE_DATA structure.
  @param[in]  FilterMask        Mask of Packet Filter.

  @retval EFI_SUCCESS           Be able to update Packet Filter Setting.
  @retval other                 USB device does not support this request.

**/
EFI_STATUS
UsbCdcUpdateFilterSetting (
  IN USB_CDC_ETHERNET_PRIVATE_DATA  *PrivateData,
  IN UINT16                         FilterMask
  )
{
  EFI_STATUS                    Status;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  UINT32                        TransferStatus;
  EFI_USB_DEVICE_REQUEST        Request;

  if (PrivateData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  UsbIo = PrivateData->UsbControlIo;
  ASSERT (UsbIo != NULL);

  ZeroMem (&Request, sizeof (Request));
  Request.RequestType = USB_REQ_TYPE_CLASS | USB_TARGET_INTERFACE;
  Request.Request = USB_CDC_ECM_SET_ETHERNET_PACKET_FILTER;
  Request.Index = PrivateData->InterfaceControlDesc.InterfaceNumber;
  Request.Value = FilterMask;
  Request.Length = 0;

  Status = UsbIo->UsbControlTransfer (
                    UsbIo,
                    &Request,
                    EfiUsbNoData,
                    USB_CDC_ECM_CONTROL_TRANSFER_TIMEOUT,
                    NULL,
                    0,
                    &TransferStatus
                    );

  if (EFI_ERROR (Status)) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((DEBUG_ERROR, "%a: Failed to update filter settings.\n", __FUNCTION__));
  }

  return Status;
}

/**
  Get a USB Bulk In transfer.

  @param[in]  PrivateData         Points to USB CDC Ethernet private data.

  @retval EFI_SUCCESS             This operation is successful.
  @retval EFI_NOT_READY           The device has no data.
  @retval EFI_DEVICE_ERROR        The transfer failed.
  @retval EFI_INVALID_PARAMETER   The PrivateData was NULL.

**/
EFI_STATUS
UsbCdcBulkIn (
  IN USB_CDC_ETHERNET_PRIVATE_DATA *PrivateData
  )
{
  EFI_STATUS          Status;
  UINT32              TransferStatus;
  EFI_USB_IO_PROTOCOL *UsbIo;
  UINTN               TmpLen;
  UINTN               ReadLen;
  UINT8               *TmpAddr;
  UINT8               DeviceEndpoint;

  if (PrivateData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  UsbIo = PrivateData->UsbDataIo;
  ASSERT (UsbIo != NULL);

  DeviceEndpoint = PrivateData->UsbCdcDesc.UsbCdcInEndpointDesc.EndpointAddress;
  TmpAddr = PrivateData->BulkInBuffer;
  TransferStatus = 0;
  ReadLen = 0;

  while (ReadLen < MAX_ETHERNET_PKT_SIZE) {
    TmpLen = USB_CDC_ECM_DATA_PACKET_SIZE_MAX;
    Status = UsbIo->UsbBulkTransfer (
                      UsbIo,
                      DeviceEndpoint,
                      TmpAddr,
                      &TmpLen,
                      USB_CDC_ECM_BULK_TRANSFER_TIMEOUT,
                      &TransferStatus
                      );

    if (EFI_ERROR (Status) || EFI_ERROR (TransferStatus)) {
      if (Status == EFI_TIMEOUT && EFI_USB_ERR_TIMEOUT == TransferStatus) {
        DEBUG ((DEBUG_VERBOSE, "%a %d Timeout occurred!\n", __FUNCTION__, __LINE__));
        ReadLen = 0;
        Status = EFI_NOT_READY;
        goto Exit;
      }
      Status = EFI_DEVICE_ERROR;
      goto Exit;
    }

    //
    // The maximum size of an Ethernet frame is greater than the maximum packet size
    // of the USB endpoints, therefore an Ethernet frame may be split into multiple
    // USB packet transfers. A USB short packet notifies the end of an Ethernet frame,
    // if the frame size is exactly a multiple of the maximum packet size of USB then
    // a zero length packet is used to notify the end of frame.
    //
    if (TmpLen != 0) {
      if (TmpLen == USB_CDC_ECM_DATA_PACKET_SIZE_MAX) {
        ReadLen += TmpLen;
        TmpAddr += TmpLen;
      } else {
        ReadLen += TmpLen;
        Status = EFI_SUCCESS;
        goto Exit;
      }
    } else {
      if (ReadLen == 0) {
        DEBUG ((DEBUG_INFO, "%a %d Bulk transfer failed!\n", __FUNCTION__, __LINE__));
        Status = EFI_NOT_READY;
        goto Exit;
      }

      Status = EFI_SUCCESS;
      goto Exit;
    }
  }

Exit:
  PrivateData->BulkInLength = ReadLen;
  return Status;
}
