/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef IPMI_NETFN_GROUP_EXTENSION_EXT_H_
#define IPMI_NETFN_GROUP_EXTENSION_EXT_H_

#pragma pack (1)

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
#endif
