/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef IPMI_COMMAND_LIB_EXT_
#define IPMI_COMMAND_LIB_EXT_

#pragma pack (1)

//
// Max BMC channel
//
#define BMC_MAX_CHANNEL 0xF // 4 bits

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
  );

#pragma pack()
#endif /* IPMI_COMMAND_LIB_EXT_ */
