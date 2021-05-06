/** @file

  Copyright (c) 2021, Ampere Computing. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef USB_CDC_ETHERNET_H_
#define USB_CDC_ETHERNET_H_

#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/NetworkInterfaceIdentifier.h>
#include <Protocol/SimpleNetwork.h>
#include <Protocol/UsbIo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NetLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/UefiUsbLib.h>


//
// Product/Vendor ID of the supported USB device
//
#define LINUX_USB_GADGET_CDC_ECM_VENDOR_ID    0x1d6b /* Linux Foundation */
#define LINUX_USB_GADGET_CDC_ECM_PRODUCT_ID   0x0103 /* NCM (Ethernet) Gadget */

//
// Class-Specific Codes
// USB CDC 1.2, Section 4
//
#define USB_CDC_COMMUNICATION_CLASS             0x2 /* Communications Device Class */
#define USB_CDC_COMMUNICATION_SUBCLASS_ECM      0x6 /* Ethernet Networking Control Model */

#define USB_CDC_DATA_CLASS                      0xA /* Data Interface Class */
#define USB_CDC_DATA_SUBCLASS_UNUSED            0x0 /* Unused */
#define USB_CDC_DATA_INTERFACE_NONE             0x0 /* Unused */
#define USB_CDC_DATA_INTERFACE_ETHERNET_DATA    0x7 /* Unused */

#define USB_CDC_PROTOCOL_NONE                   0x0 /* No class specific protocol required */

//
// Management Element Notifications
// USB CDC 1.2, Section 6.3
//
#define USB_CDC_NOTIFY_NETWORK_CONNECTION 0x00
#define USB_CDC_NOTIFY_RESPONSE_AVAILABLE 0x01
#define USB_CDC_NOTIFY_SERIAL_STATE       0x20
#define USB_CDC_NOTIFY_SPEED_CHANGE       0x2A

//
// Functional Descriptor Types
// USB CDC 1.2, Section 5.2.3
//
#define USB_CDC_HEADER_TYPE                 0x00
#define USB_CDC_UNION_TYPE                  0x01
#define USB_CDC_ETHERNET_TYPE               0x0F

#define USB_LANG_ID                         0x0409 // English

//
// Table 6: Class-Specific Request Codes for Ethernet subclass
// USB CDC ECM Subclass 1.2, Section 6.2
//
typedef enum {
  USB_CDC_ECM_SET_ETHERNET_MULTICAST_FILTERS = 0x40,
  USB_CDC_ECM_SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER,
  USB_CDC_ECM_GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER,
  USB_CDC_ECM_SET_ETHERNET_PACKET_FILTER,
  USB_CDC_ECM_GET_ETHERNET_STATISTIC
} USB_CDC_ECM_REQUEST_CODE;

//
// Table 62; bits in multicast filter
// USB CDC ECM Subclass 1.2, Section 6.2
//
#define USB_CDC_ECM_PACKET_TYPE_PROMISCUOUS   BIT0
#define USB_CDC_ECM_PACKET_TYPE_ALL_MULTICAST BIT1 // no filter
#define USB_CDC_ECM_PACKET_TYPE_DIRECTED      BIT2
#define USB_CDC_ECM_PACKET_TYPE_BROADCAST     BIT3
#define USB_CDC_ECM_PACKET_TYPE_MULTICAST     BIT4 // filtered

//
// USB CDC ECM endpoint address
//
#define USB_CDC_ECM_EP_CONTROL    0     /* Control endpoint */
#define USB_CDC_ECM_EP_BULK_IN    1     /* Receive endpoint */
#define USB_CDC_ECM_EP_BULK_OUT   2     /* Transmit endpoint */
#define USB_CDC_ECM_EP_INTERRUPT  3     /* Interrupt endpoint */

//
// USB CDC ECM Packet Size
//
#define USB_CDC_ECM_DATA_PACKET_SIZE_MAX  512

#define MAX_ETHERNET_PKT_SIZE     1514  /* including ethernet header */

//
// USB CDC ECM Timeout in miliseconds, set by experience
//
#define USB_CDC_ECM_CONTROL_TRANSFER_TIMEOUT  1000  // ms
#define USB_CDC_ECM_BULK_TRANSFER_TIMEOUT     3     // ms

#define USB_CDC_ECM_USB_INTERFACE_MAX         2

#pragma pack(1)

typedef struct {
  UINT8  RequestType;
  UINT8  NotificationType;
  UINT16 Value;
  UINT16 Index;
  UINT16 Length;
} USB_CDC_NOTIFICATION;

typedef struct {
  UINT8    Len;
  UINT8    Type;
} USB_DESC_HEAD;

typedef struct {
  UINT8 Length;
  UINT8 DescriptorType;
  UINT8 DescriptorSubType;

  UINT16 BcdCDC;
} USB_CDC_HEADER_FUNCTIONAL_DESCRIPTOR;

typedef struct {
  UINT8 Length;
  UINT8 DescriptorType;
  UINT8 DescriptorSubType;

  UINT8 MasterInterface0;
  UINT8 SlaveInterface0;
} USB_CDC_UNION_FUNCTIONAL_DESCRIPTOR;

typedef struct {
  UINT8   Length;
  UINT8   DescriptorType;
  UINT8   DescriptorSubType;

  UINT8   MACAddress;
  UINT32  EthernetStatistics;
  UINT16  MaxSegmentSize;
  UINT16  NumberMACFilters;
  UINT8   NumberPowerFilters;
} USB_CDC_ETHERNET_FUNCTIONAL_DESCRIPTOR;

//
// Functional Descriptors
//
typedef struct {
  USB_CDC_HEADER_FUNCTIONAL_DESCRIPTOR    UsbCdcHeaderDesc;
  USB_CDC_UNION_FUNCTIONAL_DESCRIPTOR     UsbCdcUnionDesc;
  USB_CDC_ETHERNET_FUNCTIONAL_DESCRIPTOR  UsbCdcEtherDesc;
  EFI_USB_ENDPOINT_DESCRIPTOR             UsbCdcNotiEndPointDesc;
  EFI_USB_ENDPOINT_DESCRIPTOR             UsbCdcInEndpointDesc;
  EFI_USB_ENDPOINT_DESCRIPTOR             UsbCdcOutEndpointDesc;
} USB_CDC_ECM_DESCRIPTOR;

#define USB_CDC_ETHERNET_SIGNATURE   SIGNATURE_32('U', 'E', 't', 'h')

typedef struct {
  UINTN                         Signature;
  EFI_HANDLE                    Controller;

  //
  // Simple Network Protocol
  //
  EFI_SIMPLE_NETWORK_PROTOCOL   Snp;
  EFI_SIMPLE_NETWORK_MODE       SnpMode;

  //
  // USB IO Protocol
  //
  EFI_HANDLE                    UsbCdcDataHandle;
  EFI_USB_IO_PROTOCOL           *UsbControlIo;
  EFI_USB_IO_PROTOCOL           *UsbDataIo;

  //
  // USB Descriptors
  //
  EFI_USB_CONFIG_DESCRIPTOR     ConfigDesc;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceControlDesc;
  EFI_USB_INTERFACE_DESCRIPTOR  InterfaceDataDesc;
  USB_CDC_ECM_DESCRIPTOR        UsbCdcDesc;
  UINTN                         ActiveAltSetting;

  //
  // Ethernet controller data
  //
  BOOLEAN                       Initialized;

  //
  //  Link state
  //
  BOOLEAN                       LinkUp;
  VOID                          *TxBuffer;

  //
  //  Receive buffer list
  //
  UINT8                         *BulkInBuffer;
  UINTN                         BulkInLength;

  UINT8                         *BulkOutBuffer;

  UINT8                         MulticastHash[8];

  EFI_DEVICE_PATH_PROTOCOL      *MacDevicePath;
} USB_CDC_ETHERNET_PRIVATE_DATA;

#define USB_CDC_ETHERNET_PRIVATE_DATA_FROM_THIS_SNP(a) \
  CR(a, USB_CDC_ETHERNET_PRIVATE_DATA, Snp, USB_CDC_ETHERNET_SIGNATURE)

#pragma pack()

//
// Global Variables
//
extern EFI_DRIVER_BINDING_PROTOCOL   gUsbCdcEthernetDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL   gUsbCdcEthernetComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gUsbCdcEthernetComponentName2;


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
  );

/**
  Get CDC Functional descriptor from Interface 0
**/
EFI_STATUS
UsbCdcEnumFunctionalDescriptor (
  IN OUT USB_CDC_ETHERNET_PRIVATE_DATA   *PrivateData
  );

/**
  Get BULK Endpoint descriptors
**/
EFI_STATUS
UsbCdcEnumBulkEndpointDescriptor (
  IN OUT USB_CDC_ETHERNET_PRIVATE_DATA   *PrivateData
  );

/**
  Get INTERRUPT Endpoint descriptor
**/
EFI_STATUS
UsbCdcEnumInterruptEndpointDescriptor (
  IN OUT USB_CDC_ETHERNET_PRIVATE_DATA   *PrivateData
  );

EFI_STATUS
UsbCdcSelectAltSetting (
  IN EFI_USB_IO_PROTOCOL    *UsbIo,
  IN UINTN                  InterfaceNumber,
  IN UINTN                  AltSettingIndex
  );

EFI_STATUS
UsbCdcGetAltSetting (
  IN  EFI_USB_IO_PROTOCOL    *UsbIo,
  IN  UINTN                  InterfaceNumber,
  OUT UINTN                  *AltSettingIndex
  );

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
  );

/**
  Read connection status from interrupt endpoint.

  @param[in]  PrivateData     Point to USB_CDC_ETHERNET_PRIVATE_DATA structure.

  @return EFI_SUCCESS         USB Link Status notification returns.
  @return EFI_UNSUPPORTED     Other notification types return.
  @return otherwise           UsbSyncInterruptTransfer returns IO error.

**/
EFI_STATUS
UsbCdcGetLinkStatus (
  IN USB_CDC_ETHERNET_PRIVATE_DATA *PrivateData
  );

/**
  Write to control Endpoint to update Packet Filter Setting.

  @param [in] PrivateData       Pointer to USB_CDC_ETHERNET_PRIVATE_DATA structure
  @param [in] FilterMask        Mask of Packet Filter

  @retval EFI_SUCCESS           Be able to update Packet Filter Setting.
  @retval other                 USB device does not support this request.

**/
EFI_STATUS
UsbCdcUpdateFilterSetting (
  IN USB_CDC_ETHERNET_PRIVATE_DATA *PrivateData,
  IN UINT16 FilterMask
  );

/**
  Setup a Simple Network Protocol (SNP)
**/
EFI_STATUS
UsbCdcEthernetSnpSetup (
  IN USB_CDC_ETHERNET_PRIVATE_DATA *PrivateData
  );

#endif  /* USB_CDC_ETHERNET_H_ */
