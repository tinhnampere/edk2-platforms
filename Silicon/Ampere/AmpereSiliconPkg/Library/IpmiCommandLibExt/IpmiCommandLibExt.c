/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <IndustryStandard/Ipmi.h>
#include <IpmiNetFnAppExt.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IpmiCommandLibExt.h>
#include <Library/IpmiLib.h>
#include <Library/MemoryAllocationLib.h>

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
  )
{
  EFI_STATUS                                     Status;
  IPMI_GET_CHANNEL_INFO_REQUEST                  GetChannelInfoRequest;
  IPMI_GET_CHANNEL_INFO_RESPONSE                 GetChannelInfoResponse;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_REQUEST  GetConfigurationParametersRequest;
  IPMI_GET_LAN_CONFIGURATION_PARAMETERS_RESPONSE *GetConfigurationParametersResponse;
  UINT32                                         ResponseSize;
  UINT8                                          Idx;
  UINT8                                          ChannelNumber;
  UINT8                                          Count;

  if (IpAddressBuffer == NULL
      || IpAddressSize == NULL
      || *IpAddressSize > BMC_MAX_CHANNEL)
  {
    return EFI_INVALID_PARAMETER;
  }

  Count = 0;
  GetConfigurationParametersResponse = AllocateZeroPool (
                                         sizeof (*GetConfigurationParametersResponse)
                                         + sizeof (IPMI_LAN_IP_ADDRESS)
                                         );
  if (GetConfigurationParametersResponse == NULL) {
    *IpAddressSize = 0;
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Get Channel Information
  //
  ZeroMem (&GetChannelInfoRequest, sizeof (GetChannelInfoRequest));
  for (ChannelNumber = 0; ChannelNumber < BMC_MAX_CHANNEL; ChannelNumber++) {
    ResponseSize = sizeof (GetChannelInfoResponse);
    GetChannelInfoRequest.Bits.ChannelNumber = ChannelNumber;
    Status = IpmiSubmitCommand (
               IPMI_NETFN_APP,
               IPMI_APP_GET_CHANNEL_INFO,
               (VOID *)&GetChannelInfoRequest,
               sizeof (GetChannelInfoRequest),
               (VOID *)&GetChannelInfoResponse,
               &ResponseSize
               );

    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a Failed to get info of channel %d\n",
        __FUNCTION__,
        ChannelNumber
        ));

      *IpAddressSize = 0;
      goto Exit;
    }

    //
    // Check for Lan interface
    //
    if (GetChannelInfoResponse.CompletionCode != IPMI_COMP_CODE_NORMAL
        || GetChannelInfoResponse.MediumType.Bits.ChannelMediumType != BMC_CHANNEL_MEDIUM_TYPE_ETHERNET)
    {
      continue;
    }

    //
    // Get LAN IP Address
    //
    ZeroMem (&GetConfigurationParametersRequest, sizeof (GetConfigurationParametersRequest));
    GetConfigurationParametersRequest.ChannelNumber.Uint8 = ChannelNumber;
    GetConfigurationParametersRequest.ParameterSelector = IpmiLanIpAddress;
    GetConfigurationParametersRequest.SetSelector = 0;
    GetConfigurationParametersRequest.BlockSelector = 0;

    ResponseSize = sizeof (*GetConfigurationParametersResponse)
                   + sizeof (IPMI_LAN_IP_ADDRESS);

    Status = IpmiSubmitCommand (
               IPMI_NETFN_TRANSPORT,
               IPMI_TRANSPORT_GET_LAN_CONFIG_PARAMETERS,
               (VOID *)&GetConfigurationParametersRequest,
               sizeof (GetConfigurationParametersRequest),
               (VOID *)GetConfigurationParametersResponse,
               &ResponseSize
               );

    if (EFI_ERROR (Status)
        || GetConfigurationParametersResponse->CompletionCode != IPMI_COMP_CODE_NORMAL)
    {
      DEBUG ((
        DEBUG_ERROR,
        "%a Failed to get IP address of channel %d\n",
        __FUNCTION__,
        ChannelNumber
        ));

      *IpAddressSize = 0;
      goto Exit;
    }

    DEBUG ((DEBUG_INFO, "IP DataSize %d\n", ResponseSize));
    for (Idx = 0; Idx < ResponseSize; Idx++) {
      DEBUG ((DEBUG_INFO, "0x%02x ", ((UINT8 *)GetConfigurationParametersResponse)[Idx]));
    }
    DEBUG ((DEBUG_INFO, "\n", ResponseSize));

    //
    // Copy IP Address to return buffer
    //
    if (Count <= *IpAddressSize) {
      IpAddressBuffer[Count] = AllocateZeroPool (sizeof (IPMI_LAN_IP_ADDRESS));
      if (IpAddressBuffer[Count] != NULL) {
        CopyMem (
          IpAddressBuffer[Count],
          GetConfigurationParametersResponse->ParameterData,
          sizeof (IPMI_LAN_IP_ADDRESS)
          );
        Count++;
      }
    }
  }

  *IpAddressSize = Count;

Exit:
  FreePool (GetConfigurationParametersResponse);
  return Status;
}

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
  )
{
  EFI_STATUS                    Status;
  UINT32                        DataSize;

  if (SetBootOptionsRequest == NULL
      || CompletionCode == NULL)
  {
    return EFI_INVALID_PARAMETER;
  }

  DataSize = sizeof (*CompletionCode);
  Status = IpmiSubmitCommand (
             IPMI_NETFN_CHASSIS,
             IPMI_CHASSIS_SET_SYSTEM_BOOT_OPTIONS,
             (VOID *)SetBootOptionsRequest,
             SetBootOptionsRequestSize,
             (VOID *)CompletionCode,
             &DataSize
             );

  switch (*CompletionCode) {
  case IPMI_COMP_CODE_NORMAL:
    break;

  case IPMI_COMP_CODE_PARAM_UNSUPPORTED:
    Status = EFI_UNSUPPORTED;
    break;

  case IPMI_COMP_CODE_SET_IN_PROGRESS:
    Status = EFI_NOT_READY;
    break;

  case IPMI_COMP_CODE_READ_ONLY:
    Status = EFI_ACCESS_DENIED;
    break;

  default:
    Status = EFI_DEVICE_ERROR;
    break;
  }

  return Status;
}

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
  )
{
  EFI_STATUS                    Status;
  IPMI_GET_BOOT_OPTIONS_REQUEST GetBootOptionsRequest;

  if (GetBootOptionsResponse == NULL
      || GetBootOptionsResponseSize == NULL)
  {
    return EFI_INVALID_PARAMETER;
  }

  GetBootOptionsRequest.ParameterSelector.Bits.ParameterSelector = ParameterSelector;
  GetBootOptionsRequest.ParameterSelector.Bits.Reserved = 0;
  GetBootOptionsRequest.SetSelector = 0;
  GetBootOptionsRequest.BlockSelector = 0;

  Status = IpmiSubmitCommand (
             IPMI_NETFN_CHASSIS,
             IPMI_CHASSIS_GET_SYSTEM_BOOT_OPTIONS,
             (VOID *)&GetBootOptionsRequest,
             sizeof (GetBootOptionsRequest),
             (VOID *)GetBootOptionsResponse,
             GetBootOptionsResponseSize
             );

  switch (GetBootOptionsResponse->CompletionCode) {
  case IPMI_COMP_CODE_NORMAL:
    break;

  case IPMI_COMP_CODE_PARAM_UNSUPPORTED:
    Status = EFI_UNSUPPORTED;
    break;

  default:
    Status = EFI_DEVICE_ERROR;
    break;
  }

  return Status;
}

/**
  Set Boot Info Acknowledge to notify BMC that the Boot Flags has been handled by UEFI.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiSetBootInfoAck (
  VOID
  )
{
  EFI_STATUS                             Status;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4 *ParameterData;
  IPMI_SET_BOOT_OPTIONS_REQUEST          *SetBootOptionsRequest;
  UINT32                                 SetBootOptionsRequestSize;
  UINT8                                  CompletionCode;

  SetBootOptionsRequestSize = sizeof (*SetBootOptionsRequest) + sizeof (*ParameterData);
  SetBootOptionsRequest = AllocateZeroPool (SetBootOptionsRequestSize);
  if (SetBootOptionsRequest == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  SetBootOptionsRequest->ParameterValid.Bits.ParameterSelector = IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK;
  SetBootOptionsRequest->ParameterValid.Bits.MarkParameterInvalid = 0x0;

  ParameterData = (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4 *)&SetBootOptionsRequest->ParameterData;
  ParameterData->WriteMask = BIT0;
  ParameterData->BootInitiatorAcknowledgeData = 0x0;

  Status = IpmiSetSystemBootOptions (
             SetBootOptionsRequest,
             SetBootOptionsRequestSize,
             &CompletionCode
             );

  FreePool (SetBootOptionsRequest);
  return Status;
}

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
  )
{
  EFI_STATUS                              Status;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4  *ParameterData;
  IPMI_GET_BOOT_OPTIONS_RESPONSE          *GetBootOptionsResponse;
  UINT32                                  GetBootOptionsResponseSize;

  if (BootInitiatorAcknowledgeData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  GetBootOptionsResponseSize = sizeof (*GetBootOptionsResponse) + sizeof (*ParameterData);
  GetBootOptionsResponse = AllocateZeroPool (GetBootOptionsResponseSize);
  if (GetBootOptionsResponse == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ParameterData = (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_4 *)&GetBootOptionsResponse->ParameterData;

  Status = IpmiGetSystemBootOptions (
             IPMI_BOOT_OPTIONS_PARAMETER_BOOT_INFO_ACK,
             GetBootOptionsResponse,
             &GetBootOptionsResponseSize
             );

  if (!EFI_ERROR (Status)) {
    *BootInitiatorAcknowledgeData = ParameterData->BootInitiatorAcknowledgeData;
  }

  FreePool (GetBootOptionsResponse);
  return Status;
}

/**
  Send command to clear BMC Boot Flags parameter.

  @retval EFI_SUCCESS                           The command byte stream was successfully submit to the device and a response was successfully received.
  @retval other                                 Failed to write data to the device.
**/
EFI_STATUS
EFIAPI
IpmiClearBootFlags (
  VOID
  )
{
  EFI_STATUS                              Status;
  IPMI_SET_BOOT_OPTIONS_REQUEST           *SetBootOptionsRequest;
  UINT32                                  SetBootOptionsRequestSize;
  UINT8                                   CompletionCode;

  SetBootOptionsRequestSize = sizeof (*SetBootOptionsRequest) + sizeof (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5);
  SetBootOptionsRequest = AllocateZeroPool (SetBootOptionsRequestSize);
  if (SetBootOptionsRequest == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  SetBootOptionsRequest->ParameterValid.Bits.ParameterSelector = IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS;

  Status = IpmiSetSystemBootOptions (
             SetBootOptionsRequest,
             SetBootOptionsRequestSize,
             &CompletionCode
             );

  FreePool (SetBootOptionsRequest);
  return Status;
}

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
  )
{
  EFI_STATUS                              Status;
  IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5  *ParameterData;
  IPMI_GET_BOOT_OPTIONS_RESPONSE          *GetBootOptionsResponse;
  UINT32                                  GetBootOptionsResponseSize;

  if (BootFlags == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  GetBootOptionsResponseSize = sizeof (*GetBootOptionsResponse) + sizeof (*ParameterData);
  GetBootOptionsResponse = AllocateZeroPool (GetBootOptionsResponseSize);
  if (GetBootOptionsResponse == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ParameterData = (IPMI_BOOT_OPTIONS_RESPONSE_PARAMETER_5 *)&GetBootOptionsResponse->ParameterData;

  Status = IpmiGetSystemBootOptions (
             IPMI_BOOT_OPTIONS_PARAMETER_BOOT_FLAGS,
             GetBootOptionsResponse,
             &GetBootOptionsResponseSize
             );

  if (!EFI_ERROR (Status)) {
    BootFlags->IsPersistent = ParameterData->Data1.Bits.PersistentOptions;
    BootFlags->IsBootFlagsValid = ParameterData->Data1.Bits.BootFlagValid;
    BootFlags->DeviceSelector = ParameterData->Data2.Bits.BootDeviceSelector;
    BootFlags->InstanceSelector = ParameterData->Data5.Bits.DeviceInstanceSelector;
  }

  FreePool (GetBootOptionsResponse);
  return Status;
}
