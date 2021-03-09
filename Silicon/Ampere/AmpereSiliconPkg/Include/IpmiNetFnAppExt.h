/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef IPMI_NETFN_APP_EXT_H_
#define IPMI_NETFN_APP_EXT_H_

#pragma pack (1)

//
//  Definitions for system interface type
//
#define IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_INTERFACE_TYPE_SSIF 0x0
#define IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_INTERFACE_TYPE_KCS  0x1
#define IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_INTERFACE_TYPE_SMIC 0x2

typedef union {
  struct {
    UINT8 InterfaceType : 4;
    UINT8 Reserved : 4;
  } Bits;
  UINT8 Uint8;
} IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_REQUEST;

typedef union {
  struct {
    UINT8 Version : 3;
    UINT8 PecSupport : 1;
    UINT8 Reserved : 2;
    UINT8 TransactionSupport : 2;
  } Bits;
  UINT8 Uint8;
} IPMI_SYSTEM_INTERFACE_SSIF_CAPABILITIES;

typedef struct {
  UINT8                                   CompletionCode;
  UINT8                                   Reserved;
  IPMI_SYSTEM_INTERFACE_SSIF_CAPABILITIES InterfaceCap;
  UINT8                                   InputMsgSize;
  UINT8                                   OutputMsgSize;
} IPMI_GET_SYSTEM_INTERFACE_SSIF_CAPABILITIES_RESPONSE;

//
//  Constants and Structure definitions for "Get Channel Info" command to follow here
//
#define BMC_CHANNEL_MEDIUM_TYPE_ETHERNET 0x04

typedef union {
  struct {
    UINT8 ChannelNumber : 4;
    UINT8 Reserved : 4;
  } Bits;
  UINT8 Uint8;
} IPMI_GET_CHANNEL_INFO_REQUEST;

#pragma pack()
#endif
