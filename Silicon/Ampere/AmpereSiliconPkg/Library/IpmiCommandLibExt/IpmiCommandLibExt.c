/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <IndustryStandard/Ipmi.h>
#include <IndustryStandard/IpmiNetFnApp.h>
#include <IndustryStandard/IpmiNetFnTransport.h>
#include <IpmiNetFnAppExt.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IpmiCommandLibExt.h>
#include <Library/IpmiLib.h>
#include <Library/MemoryAllocationLib.h>

/**
  Get BMC Lan IP Address

  @param[in, out]    IpAddressBuffer   A pointer to array of pointers to callee allocated buffer that returns information about BMC Lan Ip
  @param[in, out]    IpAddressSize     Size of input IpAddressBuffer. Size of Lan channel in return

  @retval EFI_SUCCESS                  The command byte stream was successfully submit to the device and a response was successfully received.
  @retval other                        Failed to write data to the device.
**/
EFI_STATUS
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

  ASSERT (IpAddressBuffer != NULL && IpAddressSize != NULL);
  ASSERT (*IpAddressSize <= BMC_MAX_CHANNEL);

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
        || GetConfigurationParametersResponse->CompletionCode != IPMI_COMP_CODE_NORMAL) {
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
