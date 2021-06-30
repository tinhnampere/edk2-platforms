/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <IndustryStandard/RedfishHostInterface.h>
#include <Library/DebugLib.h>
#include <Library/IpmiCommandLibExt.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NetLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/IpmiProtocol.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/Smbios.h>

#define USB_CDC_ETHERNET_SIGNATURE   SIGNATURE_32('U', 'E', 't', 'h')
#define INSTANCE_FROM_SNP_THIS(a)    BASE_CR (a, COMMON_SNP_INSTANCE, Snp)

#define REDFISH_BMC_CHANNEL          3

//
// Most SNP protocols follow below structure for their private data instance.
//
typedef struct {
  UINTN                         Signature;
  EFI_HANDLE                    Controller;

  // EFI SNP protocol instances
  EFI_SIMPLE_NETWORK_PROTOCOL   Snp;
  EFI_SIMPLE_NETWORK_MODE       SnpMode;
} COMMON_SNP_INSTANCE;

VOID                            *mRegistration;
REDFISH_OVER_IP_PROTOCOL_DATA   *mRedfishOverIpProtocolData;
UINT8                           mRedfishProtocolDataLength;

/**
  Create SMBIOS type 42 record for Redfish host interface.

  @param[in] MacAddress[in]  NIC MAC address

  @retval EFI_SUCCESS        SMBIOS type 42 record is created.
  @retval Others             Fail to create SMBIOS 42 record.

**/
EFI_STATUS
CreateSmbiosTable42 (
  IN EFI_MAC_ADDRESS MacAddress
  )
{
  EFI_SMBIOS_HANDLE                 SmbiosHandle;
  EFI_SMBIOS_PROTOCOL               *Smbios;
  EFI_STATUS                        Status;
  MC_HOST_INTERFACE_PROTOCOL_RECORD *ProtocolRecord;
  REDFISH_INTERFACE_DATA            *InterfaceData;
  SMBIOS_TABLE_TYPE42               *Type42Record;
  UINT8                             InterfaceDataLength;
  UINT8                             ProtocolRecordLength;
  UINT8                             Type42RecordLength;

  //
  // Construct Interface Specific Data
  //
  InterfaceDataLength = sizeof (PCI_OR_PCIE_INTERFACE_DEVICE_DESCRIPTOR_V2) + sizeof (InterfaceData->DeviceType);
  InterfaceData = AllocateZeroPool (InterfaceDataLength);
  if (InterfaceData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  InterfaceData->DeviceType = REDFISH_HOST_INTERFACE_DEVICE_TYPE_PCI_PCIE_V2;
  CopyMem (
    (VOID *)&InterfaceData->DeviceDescriptor.PciPcieDeviceV2.MacAddress,
    (VOID *)MacAddress.Addr,
    sizeof (InterfaceData->DeviceDescriptor.PciPcieDeviceV2.MacAddress)
    );

  //
  // Construct Protocol Record
  //
  ProtocolRecordLength = sizeof (MC_HOST_INTERFACE_PROTOCOL_RECORD) - sizeof (ProtocolRecord->ProtocolTypeData)
                         + mRedfishProtocolDataLength;
  ProtocolRecord = AllocateZeroPool (ProtocolRecordLength);
  if (ProtocolRecord == NULL) {
    FreePool (InterfaceData);
    return EFI_OUT_OF_RESOURCES;
  }
  ProtocolRecord->ProtocolType = MCHostInterfaceProtocolTypeRedfishOverIP;
  ProtocolRecord->ProtocolTypeDataLen = mRedfishProtocolDataLength;
  CopyMem ((VOID *)&ProtocolRecord->ProtocolTypeData, (VOID *)mRedfishOverIpProtocolData, mRedfishProtocolDataLength);

  //
  // Construct SMBIOS Type 42h for Redfish host inteface.
  // Follow DSP0270 Spec, Version: 1.3.0
  //
  // SMBIOS type 42 Record for Redfish Interface
  // 00h Type BYTE 42 Management Controller Host Interface structure indicator
  // 01h Length BYTE Varies Length of the structure, a minimum of 09h
  // 02h Handle WORD Varies
  // 04h Interface Type BYTE Varies Management Controller Interface Type.
  // 05h Interface Specific Data Length (n)
  // 06h Interface Specific data
  // 06h+n number of protocols defined for the host interface (typically 1)
  // 07h+n Include a Protocol Record for each protocol supported.
  //
  Type42RecordLength = sizeof (SMBIOS_TABLE_TYPE42) - sizeof (Type42Record->InterfaceTypeSpecificData)
                       + InterfaceDataLength
                       + 1 // For Protocol Record Count
                       + ProtocolRecordLength;
  Type42Record = AllocateZeroPool (Type42RecordLength + 2); // Double NULL terminator
  if (Type42Record == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_EXIT;
  }

  Type42Record->Hdr.Type   = EFI_SMBIOS_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE;
  Type42Record->Hdr.Length = Type42RecordLength;
  Type42Record->Hdr.Handle = 0;
  Type42Record->InterfaceType = MCHostInterfaceTypeNetworkHostInterface;
  Type42Record->InterfaceTypeSpecificDataLength = InterfaceDataLength;
  CopyMem (Type42Record->InterfaceTypeSpecificData, InterfaceData, InterfaceDataLength);

  //
  // Fill in Protocol Record count
  //
  *(Type42Record->InterfaceTypeSpecificData + InterfaceDataLength) = 1; // One record only

  //
  // Fill in Protocol Record
  //
  CopyMem (
    Type42Record->InterfaceTypeSpecificData + InterfaceDataLength + 1,
    ProtocolRecord,
    ProtocolRecordLength
    );

  //
  // Add Redfish interface data record to SMBIOS table 42
  //
  Status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&Smbios);
  if (EFI_ERROR (Status)) {
    goto ON_EXIT;
  }

  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status = Smbios->Add (
                     Smbios,
                     NULL,
                     &SmbiosHandle,
                     (EFI_SMBIOS_TABLE_HEADER *)Type42Record
                     );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: Smbios->Add() - %r\n", __FUNCTION__, Status));
    goto ON_EXIT;
  }

  Status = EFI_SUCCESS;

ON_EXIT:
  if (InterfaceData != NULL) {
    FreePool (InterfaceData);
  }
  if (ProtocolRecord != NULL) {
    FreePool (ProtocolRecord);
  }
  if (Type42Record != NULL) {
    FreePool (Type42Record);
  }
  return Status;
}

VOID
SnpInstallCallback (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  COMMON_SNP_INSTANCE          *Instance;
  EFI_HANDLE                   Handle;
  EFI_SIMPLE_NETWORK_PROTOCOL  *Snp;
  EFI_STATUS                   Status;
  UINTN                        BufferSize;
  UINTN                        MacLength;
  EFI_MAC_ADDRESS              MacAddress;

  while (TRUE) {
    BufferSize = sizeof (EFI_HANDLE);
    Status = gBS->LocateHandle (
                    ByRegisterNotify,
                    NULL,
                    mRegistration,
                    &BufferSize,
                    &Handle
                    );
    if (EFI_ERROR (Status)) {
      return;
    }

    Status = gBS->HandleProtocol (
                    Handle,
                    &gEfiSimpleNetworkProtocolGuid,
                    (VOID **)&Snp
                    );
    if (EFI_ERROR (Status)) {
      return;
    }

    Instance = INSTANCE_FROM_SNP_THIS (Snp);

    //
    // Check if this is a CDC ethernet
    // Assume that there is only one instance of this kind of device
    //
    if (Instance->Signature == USB_CDC_ETHERNET_SIGNATURE) {
      Status = NetLibGetMacAddress (Handle, &MacAddress, &MacLength);
      if (!EFI_ERROR (Status)) {
        Status = CreateSmbiosTable42 (MacAddress);
        if (!EFI_ERROR (Status)) {
          gBS->CloseEvent (Event);
          return;
        }
      }
    }
  }
}

/**
The driver entry point.

@param [in] ImageHandle       Handle for the image.
@param [in] SystemTable       Address of the system table.

@retval EFI_SUCCESS           Image successfully loaded.

**/
EFI_STATUS
EFIAPI
EntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  CHAR8                           RedfishHostName[20];
  EFI_EVENT                       SnpInstallEvent;
  EFI_STATUS                      Status;
  BMC_LAN_INFO                    BmcLanInfo;
  UINT8                           HostNameLength;

  //
  // Get the Service IP and Netmask
  //
  Status = IpmiGetBmcLanInfo (REDFISH_BMC_CHANNEL, &BmcLanInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get BMC info %r\n", __FUNCTION__, Status));
    return Status;
  }

  AsciiSPrint (
    RedfishHostName,
    sizeof (RedfishHostName),
    "%d.%d.%d.%d",
    BmcLanInfo.IpAddress.IpAddress[0],
    BmcLanInfo.IpAddress.IpAddress[1],
    BmcLanInfo.IpAddress.IpAddress[2],
    BmcLanInfo.IpAddress.IpAddress[3]
    );
  HostNameLength = (UINT8)AsciiStrLen (RedfishHostName) + 1;

  mRedfishProtocolDataLength = sizeof (REDFISH_OVER_IP_PROTOCOL_DATA) - sizeof (mRedfishOverIpProtocolData->RedfishServiceHostname)
                               + HostNameLength;
  mRedfishOverIpProtocolData = AllocateZeroPool (mRedfishProtocolDataLength);
  if (mRedfishOverIpProtocolData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyGuid (&mRedfishOverIpProtocolData->ServiceUuid, &gAmpereRedfishServiceGuid);

  mRedfishOverIpProtocolData->HostIpAssignmentType = REDFISH_HOST_INTERFACE_HOST_IP_ASSIGNMENT_TYPE_STATIC;
  mRedfishOverIpProtocolData->HostIpAddressFormat = REDFISH_HOST_INTERFACE_HOST_IP_ADDRESS_FORMAT_IP4;

  mRedfishOverIpProtocolData->HostIpAddress[0] = BmcLanInfo.IpAddress.IpAddress[0];
  mRedfishOverIpProtocolData->HostIpAddress[1] = BmcLanInfo.IpAddress.IpAddress[1];
  mRedfishOverIpProtocolData->HostIpAddress[2] = BmcLanInfo.IpAddress.IpAddress[2];
  mRedfishOverIpProtocolData->HostIpAddress[3] = BmcLanInfo.IpAddress.IpAddress[3] + 1;

  mRedfishOverIpProtocolData->HostIpMask[0] = BmcLanInfo.SubnetMask.IpAddress[0];
  mRedfishOverIpProtocolData->HostIpMask[1] = BmcLanInfo.SubnetMask.IpAddress[1];
  mRedfishOverIpProtocolData->HostIpMask[2] = BmcLanInfo.SubnetMask.IpAddress[2];
  mRedfishOverIpProtocolData->HostIpMask[3] = BmcLanInfo.SubnetMask.IpAddress[3];

  mRedfishOverIpProtocolData->RedfishServiceIpDiscoveryType = 1;  // Use static IP address
  mRedfishOverIpProtocolData->RedfishServiceIpAddressFormat = 1;  // Only support IPv4

  mRedfishOverIpProtocolData->RedfishServiceIpAddress[0] = BmcLanInfo.IpAddress.IpAddress[0];
  mRedfishOverIpProtocolData->RedfishServiceIpAddress[1] = BmcLanInfo.IpAddress.IpAddress[1];
  mRedfishOverIpProtocolData->RedfishServiceIpAddress[2] = BmcLanInfo.IpAddress.IpAddress[2];
  mRedfishOverIpProtocolData->RedfishServiceIpAddress[3] = BmcLanInfo.IpAddress.IpAddress[3];

  mRedfishOverIpProtocolData->RedfishServiceIpMask[0] = BmcLanInfo.SubnetMask.IpAddress[0];
  mRedfishOverIpProtocolData->RedfishServiceIpMask[1] = BmcLanInfo.SubnetMask.IpAddress[1];
  mRedfishOverIpProtocolData->RedfishServiceIpMask[2] = BmcLanInfo.SubnetMask.IpAddress[2];
  mRedfishOverIpProtocolData->RedfishServiceIpMask[3] = BmcLanInfo.SubnetMask.IpAddress[3];

  mRedfishOverIpProtocolData->RedfishServiceIpPort = 443;         // HTTPS
  mRedfishOverIpProtocolData->RedfishServiceVlanId = 0xffffffff;

  mRedfishOverIpProtocolData->RedfishServiceHostnameLength = HostNameLength;
  AsciiStrCpyS ((CHAR8 *)(mRedfishOverIpProtocolData->RedfishServiceHostname), HostNameLength, RedfishHostName);

  //
  // Register callback event for SimpleNetworkProtocol
  //
  SnpInstallEvent = EfiCreateProtocolNotifyEvent (
                      &gEfiSimpleNetworkProtocolGuid,
                      TPL_CALLBACK,
                      SnpInstallCallback,
                      NULL,
                      &mRegistration
                      );
  ASSERT (SnpInstallEvent != NULL);

  return EFI_SUCCESS;
}
