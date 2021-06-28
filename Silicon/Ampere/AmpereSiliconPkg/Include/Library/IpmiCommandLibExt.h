/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef IPMI_COMMAND_LIB_EXT_
#define IPMI_COMMAND_LIB_EXT_

#include <IndustryStandard/Ipmi.h>

#pragma pack (1)

//
// Max BMC channel
//
#define BMC_MAX_CHANNEL 0xF // 4 bits

//
// IPMI Boot Option CompletionCode
//
#define IPMI_COMP_CODE_PARAM_UNSUPPORTED  0x80
#define IPMI_COMP_CODE_SET_IN_PROGRESS    0x81
#define IPMI_COMP_CODE_READ_ONLY          0x82

//
// IPMI Boot Option param 5 (Boot Flags) info
//
typedef struct {
  BOOLEAN IsBootFlagsValid;
  BOOLEAN IsPersistent;
  UINT8   DeviceSelector;
  UINT8   InstanceSelector;
} IPMI_BOOT_FLAGS_INFO;

//
// Structure definition for IPMI "Get System GUID" response data
//
typedef struct {
  UINT8  CompletionCode;
  UINT8  Guid[16];
} IPMI_GET_SYSTEM_GUID_RESPONSE;

/**
  Get BMC Lan IP Address.

  @param[in, out]    IpAddressBuffer   A pointer to array of pointers to callee allocated buffer that returns information about BMC Lan Ip
  @param[in, out]    IpAddressSize     Size of input IpAddressBuffer. Size of Lan channel in return

  @retval EFI_SUCCESS                  The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_INVALID_PARAMETER        IpAddressBuffer or IpAddressSize was NULL or *IpAddressSize > Max BMC Channel
  @retval other                        Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiGetBmcIpAddress (
  IN OUT IPMI_LAN_IP_ADDRESS **IpAddressBuffer,
  IN OUT UINT8               *IpAddressSize
  );

/**
  Set BMC Boot Options parameter.

  @param[in]    SetBootOptionsRequest      A pointer to an callee allocated buffer containing request data.
  @param[in]    SetBootOptionsRequestSize  Size of SetBootOptionsRequest.
  @param[out]   CompletionCode             Returned Completion Code from BMC.

  @retval EFI_SUCCESS                      The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_INVALID_PARAMETER            SetBootOptionsRequest or CompletionCode was NULL.
  @retval other                            Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiSetSystemBootOptions (
  IN  IPMI_SET_BOOT_OPTIONS_REQUEST  *SetBootOptionsRequest,
  IN  UINT32                         SetBootOptionsRequestSize,
  OUT UINT8                          *CompletionCode
  );

/**
  Get BMC Boot Options parameter.

  @param[in]        ParameterSelector           Boot Options parameter which want to get.
  @param[out]       GetBootOptionsResponse      A pointer to an callee allocated buffer to store response data.
  @param[in, out]   GetBootOptionsResponseSize  Size of GetBootOptionsResponse.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_INVALID_PARAMETER                 GetBootOptionsResponse or GetBootOptionsResponseSize was NULL.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiGetSystemBootOptions (
  IN     UINT8                          ParameterSelector,
  OUT    IPMI_GET_BOOT_OPTIONS_RESPONSE *GetBootOptionsResponse,
  IN OUT UINT32                         *GetBootOptionsResponseSize
  );

/**
  Set Boot Info Acknowledge to notify BMC that the Boot Flags has been handled by UEFI.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiSetBootInfoAck (
  VOID
  );

/**
  Retrieve Boot Info Acknowledge from BMC.

  @param[out] BootInitiatorAcknowledgeData      Pointer to returned buffer.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_INVALID_PARAMETER                 BootInitiatorAcknowledgeData was NULL.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiGetBootInfoAck (
  OUT UINT8 *BootInitiatorAcknowledgeData
  );

/**
  Send command to clear BMC Boot Flags parameter.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiClearBootFlags (
  VOID
  );

/**
  Retrieve Boot Flags parameter from BMC.

  @param[out] BootFlags                         Pointer to returned buffer.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_INVALID_PARAMETER                 BootFlags was NULL.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiGetBootFlags (
  OUT IPMI_BOOT_FLAGS_INFO *BootFlags
  );

//
//  Definitions for Get Bootstrap Account Credentials.
//  Follow Section 9. Credential bootstrapping via IPMI commands
//         of Redfish Host Interface Specification v1.3.0
//
#define IPMI_GET_BOOTSTRAP_ACCOUNT_CREDENTIALS 0x02
#define IPMI_GET_ACCOUNT_DEFAULT_GROUP         0x52
#define IPMI_GET_ACCOUNT_KEEP_BOOTSTRAP_ENABLE 0xA5
#define IPMI_BOOTSTRAP_MAX_STRING_SIZE         16

//
//  Constants and Structure definitions for "Get Bootstrap Account Credentials" command to follow here
//
typedef struct {
  UINT8  GroupExtensionId;
  UINT8  DisableCredentialControl;
} IPMI_GET_BOOTSTRAP_ACCOUNT_CREDENTIALS_REQUEST;

typedef struct {
  UINT8  CompletionCode;
  UINT8  GroupExtensionId;

  //
  // The UserName as a UTF-8 string
  //
  CHAR8  UserName[IPMI_BOOTSTRAP_MAX_STRING_SIZE];

  //
  // The Password as a UTF-8 string
  //
  CHAR8  Password[IPMI_BOOTSTRAP_MAX_STRING_SIZE];
} IPMI_GET_BOOTSTRAP_ACCOUNT_CREDENTIALS_RESPONSE;

#pragma pack()
#endif /* IPMI_COMMAND_LIB_EXT_ */
