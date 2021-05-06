/** @file

  Copyright (c) 2021, Ampere Computing. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UsbCdcEthernet.h"

/**
  Check whether USB device is CDC ECM type

  @param[in]  UsbIo           Protocol instance pointer.

  @retval TRUE                Device is CDC-ECM type.
  @retval TRUE                Otherwise.

**/
BOOLEAN
IsUsbCdcEcm (
  IN  EFI_USB_IO_PROTOCOL     *UsbIo
  )
{
  EFI_STATUS                    Status;
  EFI_USB_DEVICE_DESCRIPTOR     DeviceDescriptor;
  EFI_USB_CONFIG_DESCRIPTOR     ConfigDescriptor;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;

  //
  // Get the interface descriptor
  //
  Status = UsbIo->UsbGetDeviceDescriptor (
                    UsbIo,
                    &DeviceDescriptor
                    );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  if (DeviceDescriptor.IdVendor != LINUX_USB_GADGET_CDC_ECM_VENDOR_ID
      || DeviceDescriptor.IdProduct != LINUX_USB_GADGET_CDC_ECM_PRODUCT_ID) {
    return FALSE;
  }

  //
  // Get the configuration descriptor
  //
  Status = UsbIo->UsbGetConfigDescriptor (
                    UsbIo,
                    &ConfigDescriptor
                    );
  if (EFI_ERROR (Status)
      || ConfigDescriptor.NumInterfaces > USB_CDC_ECM_USB_INTERFACE_MAX) {
    //
    // This driver only supports CDC USB device
    // with maximum 2 USB interfaces.
    //
    return FALSE;
  }

  //
  // Get the default interface descriptor
  //
  Status = UsbIo->UsbGetInterfaceDescriptor (
                    UsbIo,
                    &InterfaceDescriptor
                    );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  if (InterfaceDescriptor.InterfaceClass == USB_CDC_COMMUNICATION_CLASS
      && InterfaceDescriptor.InterfaceSubClass == USB_CDC_COMMUNICATION_SUBCLASS_ECM
      && InterfaceDescriptor.InterfaceProtocol == USB_CDC_PROTOCOL_NONE) {
    return TRUE;
  }

  return FALSE;
}

EFI_STATUS
IsSameUsbCdcDevice (
  IN EFI_DEVICE_PATH_PROTOCOL    *UsbIoDevicePath,
  IN EFI_DEVICE_PATH_PROTOCOL    *UsbCdcDataPath
  )
{
  while (TRUE) {
    if (UsbIoDevicePath->Type == ACPI_DEVICE_PATH
        && UsbIoDevicePath->SubType == ACPI_DP) {
      if (CompareMem (
            (ACPI_HID_DEVICE_PATH *)UsbCdcDataPath,
            (ACPI_HID_DEVICE_PATH *)UsbIoDevicePath,
            sizeof (ACPI_HID_DEVICE_PATH)
            ) != 0) {
        return EFI_NOT_FOUND;
      }
    }

    if (UsbIoDevicePath->Type == HARDWARE_DEVICE_PATH
        && UsbIoDevicePath->SubType == HW_PCI_DP) {
      if (CompareMem (
            (PCI_DEVICE_PATH *)UsbCdcDataPath,
            (PCI_DEVICE_PATH *)UsbIoDevicePath,
            sizeof (PCI_DEVICE_PATH)
            ) != 0) {
        return EFI_NOT_FOUND;
      }
    }

    if (UsbIoDevicePath->Type == MESSAGING_DEVICE_PATH
        && UsbIoDevicePath->SubType == MSG_USB_DP) {
      if (IsDevicePathEnd (NextDevicePathNode (UsbIoDevicePath))) {
        if (((USB_DEVICE_PATH *)UsbIoDevicePath)->ParentPortNumber ==
              ((USB_DEVICE_PATH *)UsbCdcDataPath)->ParentPortNumber) {
          return EFI_SUCCESS;
        } else {
          return EFI_NOT_FOUND;
        }
      } else {
        if (CompareMem (
              (USB_DEVICE_PATH *)UsbCdcDataPath,
              (USB_DEVICE_PATH *)UsbIoDevicePath,
              sizeof (USB_DEVICE_PATH)
              ) != 0) {
          return EFI_NOT_FOUND;
        }
      }
    }

    UsbIoDevicePath = NextDevicePathNode (UsbIoDevicePath);
    UsbCdcDataPath = NextDevicePathNode (UsbCdcDataPath);
  }
}

/**
  Check whether USB device is CDC Data type

  @param[in]  UsbIo           Protocol instance pointer.

  @retval BOOLEAN             Return TRUE if interface is CDC Data, FALSE otherwise.

**/
BOOLEAN
IsUsbCdcData (
  IN EFI_DEVICE_PATH_PROTOCOL  *UsbIoDevicePath,
  IN EFI_HANDLE                *UsbCdcDataHandle
  )
{
  EFI_STATUS                    Status;
  UINTN                         Index;
  UINTN                         HandleCount;
  EFI_HANDLE                    *HandleBuffer;
  EFI_DEVICE_PATH_PROTOCOL      *UsbCdcDataPath;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiUsbIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiUsbIoProtocolGuid,
                    (VOID *)&UsbIo
                    );
    if (EFI_ERROR (Status)) {
      return FALSE;
    }

    Status = UsbIo->UsbGetInterfaceDescriptor (UsbIo, &InterfaceDescriptor);
    if (EFI_ERROR (Status)) {
      return FALSE;
    }

    if ((InterfaceDescriptor.InterfaceClass == USB_CDC_DATA_CLASS)
        && (InterfaceDescriptor.InterfaceSubClass == USB_CDC_DATA_SUBCLASS_UNUSED)
        && (InterfaceDescriptor.InterfaceProtocol == USB_CDC_PROTOCOL_NONE)) {
      Status = gBS->HandleProtocol (
                      HandleBuffer[Index],
                      &gEfiDevicePathProtocolGuid,
                      (VOID *)&UsbCdcDataPath
                      );
      if (EFI_ERROR (Status)) {
        continue;
      }

      Status = IsSameUsbCdcDevice (UsbIoDevicePath, UsbCdcDataPath);
      if (!EFI_ERROR (Status)) {
        CopyMem (UsbCdcDataHandle, &HandleBuffer[Index], sizeof (EFI_HANDLE));
        gBS->FreePool (HandleBuffer);
        return TRUE;
      }
    }
  }

  gBS->FreePool (HandleBuffer);
  return FALSE;

}

BOOLEAN
IsUsbCdcEthDataInterface (
  IN  EFI_USB_INTERFACE_DESCRIPTOR  *InterfaceDescriptor,
  OUT UINTN                         *ActiveAltSetting
  )
{
  if (ActiveAltSetting == NULL) {
    return FALSE;
  }

  if (InterfaceDescriptor->InterfaceClass == USB_CDC_DATA_CLASS
      && InterfaceDescriptor->InterfaceSubClass == USB_CDC_DATA_SUBCLASS_UNUSED
      && InterfaceDescriptor->InterfaceProtocol == USB_CDC_PROTOCOL_NONE
      && InterfaceDescriptor->Interface == USB_CDC_DATA_INTERFACE_ETHERNET_DATA) {
    *ActiveAltSetting = InterfaceDescriptor->AlternateSetting;
    return TRUE;
  }

  *ActiveAltSetting = (InterfaceDescriptor->AlternateSetting == 0) ? 1 : 0;
  return FALSE;
}

/**
  Tests to see if this driver supports a given controller.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the controller to test.
  @param[in]  RemainingDevicePath  The remaining device path.
                                   (Ignored - this is not a bus driver.)

  @retval EFI_SUCCESS              The driver supports this controller.
  @retval EFI_ALREADY_STARTED      The device specified by ControllerHandle is
                                   already being managed by the driver specified
                                   by This.
  @retval EFI_UNSUPPORTED          The device specified by ControllerHandle is
                                   not supported by the driver specified by This.

**/
EFI_STATUS
EFIAPI
UsbCdcEthernetDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL *This,
  IN EFI_HANDLE                  ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath
  )
{
  EFI_USB_IO_PROTOCOL            *UsbIo;
  EFI_STATUS                     Status;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Use the USB I/O Protocol interface to check whether Controller is
  // an USB CDC-ECM device that can be managed by this driver.
  //
  Status = EFI_SUCCESS;
  if (!IsUsbCdcEcm (UsbIo)) {
    Status = EFI_UNSUPPORTED;
  }

  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiUsbIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );

  return Status;
}

/**
  Starts a device controller.

  @param[in]  This                 A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle     The handle of the device to start.
  @param[in]  RemainingDevicePath  The remaining portion of the device path.
                                   (Ignored - this is not a bus driver.)

  @retval EFI_SUCCESS              The device was started.
  @retval EFI_DEVICE_ERROR         The device could not be started due to a
                                   device error.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a
                                   lack of resources.

**/
EFI_STATUS
EFIAPI
UsbCdcEthernetDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL *This,
  IN EFI_HANDLE                  ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath
  )
{
  EFI_STATUS                    Status;
  USB_CDC_ETHERNET_PRIVATE_DATA *PrivateData;
  EFI_USB_IO_PROTOCOL           *UsbIo;
  EFI_USB_IO_PROTOCOL           *UsbDataIo;
  EFI_HANDLE                    UsbCdcDataHandle;
  UINTN                         BufferSize;
  EFI_DEVICE_PATH_PROTOCOL      *ParentDevicePath;
  MAC_ADDR_DEVICE_PATH          MacDeviceNode;
  EFI_USB_CONFIG_DESCRIPTOR     ConfigDescriptor;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDescriptor;

  BufferSize = sizeof (USB_CDC_ETHERNET_PRIVATE_DATA);
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  BufferSize,
                  (VOID **)&PrivateData
                  );
  if (EFI_ERROR (Status)) {
    goto FreePrivateData;
  }

  ZeroMem (PrivateData, BufferSize);
  PrivateData->Signature = USB_CDC_ETHERNET_SIGNATURE;

  //
  // Initialize USB IO protocol for each USB interface
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&UsbIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto FreePrivateData;
  }

  //
  // Control interface is always at index 0,
  // so assign first UsIo with the default interface.
  //
  PrivateData->UsbControlIo = UsbIo;

  //
  // Caching USB configuration descriptor
  //
  Status = UsbIo->UsbGetConfigDescriptor (
                    UsbIo,
                    &ConfigDescriptor
                    );
  if (EFI_ERROR (Status)) {
    goto FreePrivateData;
  }

  CopyMem (
    &PrivateData->ConfigDesc,
    &ConfigDescriptor,
    sizeof (EFI_USB_CONFIG_DESCRIPTOR)
    );

  //
  // Caching USB control interface descriptor
  //
  Status = UsbIo->UsbGetInterfaceDescriptor (
                    UsbIo,
                    &InterfaceDescriptor
                    );
  if (EFI_ERROR (Status)) {
    goto FreePrivateData;
  }

  CopyMem (
    &PrivateData->InterfaceControlDesc,
    &InterfaceDescriptor,
    sizeof (EFI_USB_INTERFACE_DESCRIPTOR)
    );

  //
  // Set Device Path
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&ParentDevicePath,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status) && Status != EFI_ALREADY_STARTED) {
    goto CloseUsbIo;
  }
  ASSERT (ParentDevicePath != NULL);

  //
  // Get USB CDC Data handle and USB IO Data protocol handle.
  //
  if (!IsUsbCdcData (ParentDevicePath, &UsbCdcDataHandle)) {
    Status = EFI_UNSUPPORTED;
    goto CloseUsbIo;
  } else {
    Status = gBS->HandleProtocol (
                    UsbCdcDataHandle,
                    &gEfiUsbIoProtocolGuid,
                    (VOID *)&UsbDataIo
                    );
    if (EFI_ERROR (Status)) {
      Status = EFI_UNSUPPORTED;
      goto CloseUsbIo;
    }

    // Assign USB Data IO Protocol
    PrivateData->UsbDataIo = UsbDataIo;
  }

  Status = UsbDataIo->UsbGetInterfaceDescriptor (
                        UsbDataIo,
                        &InterfaceDescriptor
                        );
  if (EFI_ERROR (Status)) {
    goto CloseUsbIo;
  }

  if (!IsUsbCdcEthDataInterface (&InterfaceDescriptor, &PrivateData->ActiveAltSetting)) {
    Status = UsbCdcSelectAltSetting (
               UsbDataIo,
               InterfaceDescriptor.InterfaceNumber,
               PrivateData->ActiveAltSetting
               );
    if (EFI_ERROR (Status)) {
      goto CloseUsbIo;
    }

    // Update Interface Descriptor
    Status = UsbDataIo->UsbGetInterfaceDescriptor (
                          UsbDataIo,
                          &InterfaceDescriptor
                          );
    if (EFI_ERROR (Status)) {
      goto CloseUsbIo;
    }
  }

  //
  // Caching USB CDC Data Interface descriptor
  //
  CopyMem (
    &PrivateData->InterfaceDataDesc,
    &InterfaceDescriptor,
    sizeof (EFI_USB_INTERFACE_DESCRIPTOR)
    );

  //
  // Caching all CDC Functional Descriptors and Endpoint Descriptors
  //
  Status = UsbCdcEnumFunctionalDescriptor (PrivateData);
  if (EFI_ERROR (Status)) {
    goto CloseUsbIo;
  }

  Status = UsbCdcEnumInterruptEndpointDescriptor (PrivateData);
  if (EFI_ERROR (Status)) {
    goto CloseUsbIo;
  }

  Status = UsbCdcEnumBulkEndpointDescriptor (PrivateData);
  if (EFI_ERROR (Status)) {
    goto CloseUsbIo;
  }

  //
  //  Initialize the Simple Network Protocol
  //
  Status = UsbCdcEthernetSnpSetup (PrivateData);
  if (EFI_ERROR (Status)){
    goto CloseUsbIo;
  }
  ZeroMem (&MacDeviceNode, sizeof (MAC_ADDR_DEVICE_PATH));
  MacDeviceNode.Header.Type = MESSAGING_DEVICE_PATH;
  MacDeviceNode.Header.SubType = MSG_MAC_ADDR_DP;

  SetDevicePathNodeLength (&MacDeviceNode.Header, sizeof (MAC_ADDR_DEVICE_PATH));

  CopyMem (
    &MacDeviceNode.MacAddress,
    &PrivateData->SnpMode.CurrentAddress,
    NET_ETHER_ADDR_LEN
    );

  MacDeviceNode.IfType = PrivateData->SnpMode.IfType;

  PrivateData->MacDevicePath = AppendDevicePathNode (
                                 ParentDevicePath,
                                 (EFI_DEVICE_PATH_PROTOCOL *)&MacDeviceNode
                                 );

  PrivateData->Controller = NULL;

  //
  //  Install both the simple network and device path protocols.
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &PrivateData->Controller,
                  &gEfiCallerIdGuid,
                  PrivateData,
                  &gEfiSimpleNetworkProtocolGuid,
                  &PrivateData->Snp,
                  &gEfiDevicePathProtocolGuid,
                  PrivateData->MacDevicePath,
                  NULL
                  );

  if (EFI_ERROR (Status)){
    goto CloseDevicePath;
  }

  //
  // Open For Child Device
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbIoProtocolGuid,
                  (VOID **)&PrivateData->UsbControlIo,
                  This->DriverBindingHandle,
                  PrivateData->Controller,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );

  if (EFI_ERROR (Status)){
    goto ErrorExit;
  }

  //
  // Successfull completion
  //
  return Status;


ErrorExit:
  gBS->UninstallMultipleProtocolInterfaces (
         &PrivateData->Controller,
         &gEfiCallerIdGuid,
         PrivateData,
         &gEfiSimpleNetworkProtocolGuid,
         &PrivateData->Snp,
         &gEfiDevicePathProtocolGuid,
         PrivateData->MacDevicePath,
         NULL
         );

CloseDevicePath:
  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiDevicePathProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );

CloseUsbIo:
  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiUsbIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );

FreePrivateData:
  if (PrivateData->BulkInBuffer != NULL) {
    gBS->FreePool (PrivateData->BulkInBuffer);
  }

  if (PrivateData->MacDevicePath != NULL) {
    gBS->FreePool (PrivateData->MacDevicePath);
  }

  if (PrivateData != NULL) {
    gBS->FreePool (PrivateData);
  }

  return Status;
}

/**
  Stops a device controller.

  @param[in]  This              A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
  @param[in]  ControllerHandle  A handle to the device being stopped.
  @param[in]  NumberOfChildren  The number of child device handles in
                                ChildHandleBuffer.
  @param[in]  ChildHandleBuffer An array of child handles to be freed. May be
                                NULL if NumberOfChildren is 0.

  @retval EFI_SUCCESS           The device was stopped.
  @retval EFI_DEVICE_ERROR      The device could not be stopped due to a device error.

**/
EFI_STATUS
EFIAPI
UsbCdcEthernetDriverStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                    ControllerHandle,
  IN  UINTN                         NumberOfChildren,
  IN  EFI_HANDLE                    *ChildHandleBuffer  OPTIONAL
  )
{
  BOOLEAN                         AllChildrenStopped;
  EFI_SIMPLE_NETWORK_PROTOCOL     *Snp;
  EFI_STATUS                      Status;
  USB_CDC_ETHERNET_PRIVATE_DATA   *PrivateData;
  UINTN                           Index;

  //
  // Complete all outstanding transactions to Controller.
  // Don't allow any new transaction to Controller to be started.
  //
  if (NumberOfChildren == 0) {
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiSimpleNetworkProtocolGuid,
                    (VOID **)&Snp,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );

    if (EFI_ERROR (Status)) {
      //
      // This is a 2nd type handle (multi-lun root), it needs to close
      // Device Path and USB IO Protocol.
      //
      gBS->CloseProtocol (
             ControllerHandle,
             &gEfiDevicePathProtocolGuid,
             This->DriverBindingHandle,
             ControllerHandle
             );
      gBS->CloseProtocol (
             ControllerHandle,
             &gEfiUsbIoProtocolGuid,
             This->DriverBindingHandle,
             ControllerHandle
             );
      return EFI_SUCCESS;
    }

    PrivateData = USB_CDC_ETHERNET_PRIVATE_DATA_FROM_THIS_SNP (Snp);

    Status = gBS->UninstallMultipleProtocolInterfaces (
                    ControllerHandle,
                    &gEfiCallerIdGuid,
                    PrivateData,
                    &gEfiSimpleNetworkProtocolGuid,
                    &PrivateData->Snp,
                    &gEfiDevicePathProtocolGuid,
                    PrivateData->MacDevicePath,
                    NULL
                    );

    if (EFI_ERROR (Status)) {
      return Status;
    }
    //
    // Close the bus driver
    //
    Status = gBS->CloseProtocol (
                    ControllerHandle,
                    &gEfiDevicePathProtocolGuid,
                    This->DriverBindingHandle,
                    ControllerHandle
                    );

    Status = gBS->CloseProtocol (
                    ControllerHandle,
                    &gEfiUsbIoProtocolGuid,
                    This->DriverBindingHandle,
                    ControllerHandle
                    );

    return EFI_SUCCESS;
  }

  AllChildrenStopped = TRUE;

  for (Index = 0; Index < NumberOfChildren; Index++) {
    Status = gBS->OpenProtocol (
                    ChildHandleBuffer[Index],
                    &gEfiSimpleNetworkProtocolGuid,
                    (VOID **)&Snp,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );

    if (EFI_ERROR (Status)) {
      AllChildrenStopped = FALSE;
      continue;
    }

    PrivateData = USB_CDC_ETHERNET_PRIVATE_DATA_FROM_THIS_SNP (Snp);

    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiUsbIoProtocolGuid,
           This->DriverBindingHandle,
           ChildHandleBuffer[Index]
           );

    Status = gBS->UninstallMultipleProtocolInterfaces (
                    ChildHandleBuffer[Index],
                    &gEfiCallerIdGuid,
                    PrivateData,
                    &gEfiSimpleNetworkProtocolGuid,
                    &PrivateData->Snp,
                    &gEfiDevicePathProtocolGuid,
                    PrivateData->MacDevicePath,
                    NULL
                    );

    if (EFI_ERROR (Status)) {
      Status = gBS->OpenProtocol (
                      ControllerHandle,
                      &gEfiUsbIoProtocolGuid,
                      (VOID **)&PrivateData->UsbControlIo,
                      This->DriverBindingHandle,
                      ChildHandleBuffer[Index],
                      EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                      );
    } else {
      if (PrivateData->BulkInBuffer != NULL) {
        gBS->FreePool (PrivateData->BulkInBuffer);
      }

      if (PrivateData->MacDevicePath != NULL) {
        gBS->FreePool (PrivateData->MacDevicePath);
      }

      if (PrivateData != NULL) {
        gBS->FreePool (PrivateData);
      }
    }
  }

  if (!AllChildrenStopped) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

//
// USB CDC Ethernet Driver Binding Protocol Instance
//
EFI_DRIVER_BINDING_PROTOCOL gUsbCdcEthernetDriverBinding = {
  UsbCdcEthernetDriverSupported,
  UsbCdcEthernetDriverStart,
  UsbCdcEthernetDriverStop,
  0x0A,
  NULL,
  NULL
};

/**
  USB CDC Ethernet driver unload routine.

  @param[in]  ImageHandle       Handle for the image.

  @retval EFI_SUCCESS           Image may be unloaded

**/
EFI_STATUS
EFIAPI
UsbCdcEthernetDriverUnload (
  IN EFI_HANDLE ImageHandle
  )
{
  UINTN       BufferSize;
  UINTN       Index;
  EFI_HANDLE  *Handle;
  EFI_STATUS  Status;

  //
  //  Determine which devices are using this driver
  //
  BufferSize = 0;
  Handle = NULL;
  Status = gBS->LocateHandle (
                  ByProtocol,
                  &gEfiCallerIdGuid,
                  NULL,
                  &BufferSize,
                  NULL
                  );
  if (Status == EFI_BUFFER_TOO_SMALL) {
    for (; ;) {
      //
      //  One or more block IO devices are present
      //
      Status = gBS->AllocatePool (
                      EfiBootServicesData,
                      BufferSize,
                      (VOID **)&Handle
                      );
      if (EFI_ERROR (Status)) {
        break;
      }

      //
      //  Locate the block IO devices
      //
      Status = gBS->LocateHandle (
                      ByProtocol,
                      &gEfiCallerIdGuid,
                      NULL,
                      &BufferSize,
                      Handle);
      if (EFI_ERROR (Status)) {
        //
        //  Error getting handles
        //

        break;
      }

      //
      //  Remove any use of the driver
      //
      for (Index = 0; Index < BufferSize / sizeof (Handle[0]); Index++) {
        Status = UsbCdcEthernetDriverStop (
                   &gUsbCdcEthernetDriverBinding,
                   Handle[Index],
                   0,
                   NULL
                   );
        if (EFI_ERROR (Status)) {
          break;
        }
      }
      break;
    }
  } else {
    if (Status == EFI_NOT_FOUND) {
      //
      //  No devices were found
      //
      Status = EFI_SUCCESS;
    }
  }

  //
  //  Free the handle array
  //
  if (Handle != NULL) {
    gBS->FreePool (Handle);
  }

  //
  //  Remove the protocols installed by the EntryPoint routine.
  //
  if (!EFI_ERROR (Status)) {
    gBS->UninstallMultipleProtocolInterfaces (
           ImageHandle,
           &gEfiDriverBindingProtocolGuid,
           &gUsbCdcEthernetDriverBinding,
           &gEfiComponentNameProtocolGuid,
           &gUsbCdcEthernetComponentName,
           &gEfiComponentName2ProtocolGuid,
           &gUsbCdcEthernetComponentName2,
           NULL
           );
  }

  //
  //  Return the unload status
  //
  return Status;
}

/**
  USB CDC Ethernet driver entry point.

  @param[in]  ImageHandle       Handle for the image.
  @param[in]  SystemTable       Address of the system table.

  @retval EFI_SUCCESS           Image successfully loaded.

**/
EFI_STATUS
EFIAPI
UsbCdcEthernetEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  EFI_STATUS                Status;

  //
  //  Enable unload support
  //
  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (!EFI_ERROR (Status)) {
    LoadedImage->Unload = UsbCdcEthernetDriverUnload;
  }

  //
  //  Add the driver to the list of drivers
  //
  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gUsbCdcEthernetDriverBinding,
             ImageHandle,
             &gUsbCdcEthernetComponentName,
             &gUsbCdcEthernetComponentName2
             );

  return Status;
}
