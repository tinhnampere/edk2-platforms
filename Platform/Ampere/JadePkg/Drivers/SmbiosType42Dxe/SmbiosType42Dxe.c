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
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/IpmiProtocol.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/Smbios.h>

#define USB_CDC_ETHERNET_SIGNATURE   SIGNATURE_32('U', 'E', 't', 'h')
#define INSTANCE_FROM_SNP_THIS(a)    BASE_CR (a, COMMON_SNP_INSTANCE, Snp)

#define REDFISH_BMC_CHANNEL                 3
#define REDFISH_BMC_POLL_INTERVAL_US        (500 * 1000)
#define REDFISH_HOST_NAME_IP4_STR_MAX_SIZE  16
#define REDFISH_HTTPS_DEFAULT_PORT          443
#define REDFISH_RETRY                       20
#define REDFISH_VLAN_ID_RESERVE             0xFFFFFFFF

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

/**
  Create SMBIOS type 42 record for Redfish host interface.

  @param[in] MacAddress                NIC MAC address.
  @param[in] RedfishOverIpProtocolData Pointer to protocol-specific
                                       data for the "Redfish Over IP" protocol.
  @param[in] RedfishProtocolDataLength The length in bytes of RedfishOverIpProtocolData pool.

  @retval EFI_SUCCESS        SMBIOS type 42 record is created.
  @retval Others             Fail to create SMBIOS 42 record.

**/
EFI_STATUS
CreateSmbiosTable42 (
  IN EFI_MAC_ADDRESS               MacAddress,
  IN REDFISH_OVER_IP_PROTOCOL_DATA *RedfishOverIpProtocolData,
  IN UINT8                         RedfishProtocolDataLength
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

  ASSERT (RedfishOverIpProtocolData != NULL);
  ASSERT (RedfishProtocolDataLength != 0);

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
                         + RedfishProtocolDataLength;
  ProtocolRecord = AllocateZeroPool (ProtocolRecordLength);
  if (ProtocolRecord == NULL) {
    FreePool (InterfaceData);
    return EFI_OUT_OF_RESOURCES;
  }
  ProtocolRecord->ProtocolType = MCHostInterfaceProtocolTypeRedfishOverIP;
  ProtocolRecord->ProtocolTypeDataLen = RedfishProtocolDataLength;
  CopyMem ((VOID *)&ProtocolRecord->ProtocolTypeData, (VOID *)RedfishOverIpProtocolData, RedfishProtocolDataLength);

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

/**
  Get information of BMC network for Redfish.

  @param[out] RedfishOverIpProtocolData Return a pointer to pool which contains information for
                                        Redfish over IP protocol. Caller must free it.
  @param[out] RedfishProtocolDataLength Return the length in bytes of RedfishOverIpProtocolData pool.

  @retval  EFI_SUCCESS            Get Network information successfully.
  @retval  EFI_INVALID_PARAMETER  Input parameters are not a valid.
  @retval  EFI_OUT_OF_RESOURCES   Memory allocation failed.
  @retval  EFI_UNSUPPORTED        This is not USB CDC device or some error occurred.
**/
EFI_STATUS
GetRedfishRecordFromBmc (
  OUT REDFISH_OVER_IP_PROTOCOL_DATA **RedfishOverIpProtocolData,
  OUT UINT8                         *RedfishProtocolDataLength
  )
{
  BMC_LAN_INFO      BmcLanInfo;
  CHAR8             RedfishHostName[REDFISH_HOST_NAME_IP4_STR_MAX_SIZE] = {0};
  UINT8             RedfishIpv4NotRead[] = {0, 0, 0, 0};
  UINT8             HostNameLength;
  UINTN             Index;

  if (RedfishProtocolDataLength == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Get the Service IP and Netmask
  //
  for (Index = 0; Index <= REDFISH_RETRY; ++Index) {
    if (EFI_ERROR (IpmiGetBmcLanInfo (REDFISH_BMC_CHANNEL, &BmcLanInfo))
       || (Index == REDFISH_RETRY))
    {
      return EFI_UNSUPPORTED;
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
    DEBUG ((DEBUG_INFO, "Redfish Host Name IPv4: %s\n", RedfishHostName));

    if (!EFI_IP4_EQUAL (BmcLanInfo.IpAddress.IpAddress, &RedfishIpv4NotRead)) {
      break;
    }

    MicroSecondDelay (REDFISH_BMC_POLL_INTERVAL_US);
  }

  HostNameLength = (UINT8)AsciiStrLen (RedfishHostName) + 1;

  *RedfishProtocolDataLength = sizeof (REDFISH_OVER_IP_PROTOCOL_DATA) - sizeof ((*RedfishOverIpProtocolData)->RedfishServiceHostname)
                               + HostNameLength;
  *RedfishOverIpProtocolData = AllocateZeroPool (*RedfishProtocolDataLength);
  if (*RedfishOverIpProtocolData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyGuid (&((*RedfishOverIpProtocolData)->ServiceUuid), &gAmpereRedfishServiceGuid);

  (*RedfishOverIpProtocolData)->HostIpAssignmentType = REDFISH_HOST_INTERFACE_HOST_IP_ASSIGNMENT_TYPE_STATIC;
  (*RedfishOverIpProtocolData)->HostIpAddressFormat  = REDFISH_HOST_INTERFACE_HOST_IP_ADDRESS_FORMAT_IP4;

  IP4_COPY_ADDRESS ((*RedfishOverIpProtocolData)->HostIpAddress, BmcLanInfo.IpAddress.IpAddress);
  (*RedfishOverIpProtocolData)->HostIpAddress[3]++;

  IP4_COPY_ADDRESS ((*RedfishOverIpProtocolData)->HostIpMask, BmcLanInfo.SubnetMask.IpAddress);

  (*RedfishOverIpProtocolData)->RedfishServiceIpDiscoveryType = 1;  // Use static IP address
  (*RedfishOverIpProtocolData)->RedfishServiceIpAddressFormat = 1;  // Only support IPv4

  IP4_COPY_ADDRESS ((*RedfishOverIpProtocolData)->RedfishServiceIpAddress, BmcLanInfo.IpAddress.IpAddress);

  IP4_COPY_ADDRESS ((*RedfishOverIpProtocolData)->RedfishServiceIpMask, BmcLanInfo.SubnetMask.IpAddress);

  (*RedfishOverIpProtocolData)->RedfishServiceIpPort = REDFISH_HTTPS_DEFAULT_PORT;
  (*RedfishOverIpProtocolData)->RedfishServiceVlanId = REDFISH_VLAN_ID_RESERVE;

  (*RedfishOverIpProtocolData)->RedfishServiceHostnameLength = HostNameLength;
  AsciiStrCpyS ((CHAR8 *)((*RedfishOverIpProtocolData)->RedfishServiceHostname), HostNameLength, RedfishHostName);

  return EFI_SUCCESS;
}

VOID
SnpInstallCallback (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  COMMON_SNP_INSTANCE           *Instance;
  EFI_HANDLE                    Handle;
  EFI_MAC_ADDRESS               MacAddress;
  EFI_SIMPLE_NETWORK_PROTOCOL   *Snp;
  EFI_STATUS                    Status;
  REDFISH_OVER_IP_PROTOCOL_DATA *RedfishOverIpProtocolData;
  UINT8                         RedfishProtocolDataLength;
  UINTN                         BufferSize;
  UINTN                         MacLength;

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
      Status = GetRedfishRecordFromBmc (
                 &RedfishOverIpProtocolData,
                 &RedfishProtocolDataLength
                 );
      if (EFI_ERROR (Status)) {
        //
        // Redfish service is based on SMBIOS type 42
        // Should not create when we can't get proper information
        //
        continue;
      }

      Status = NetLibGetMacAddress (Handle, &MacAddress, &MacLength);
      if (!EFI_ERROR (Status) && (RedfishOverIpProtocolData != NULL)) {
        Status = CreateSmbiosTable42 (MacAddress, RedfishOverIpProtocolData, RedfishProtocolDataLength);
        FreePool (RedfishOverIpProtocolData);
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
  EFI_EVENT  SnpInstallEvent;

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
