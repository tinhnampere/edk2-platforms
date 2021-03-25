/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PCIE_DEVICE_CONFIG_H_
#define PCIE_DEVICE_CONFIG_H_

#include <Uefi.h>

#include <Library/HiiLib.h>
#include <Protocol/HiiConfigAccess.h>
#include <Protocol/HiiConfigKeyword.h>
#include <Protocol/HiiConfigRouting.h>
#include <Protocol/HiiDatabase.h>
#include <Protocol/HiiString.h>

#include "NVDataStruc.h"

#define MAX_STRING_SIZE              100

#define PRIVATE_DATA_SIGNATURE        SIGNATURE_32 ('P', 'E', 'D', 'C')
#define PRIVATE_DATA_FROM_THIS(a)     \
             CR (a, PRIVATE_DATA, ConfigAccess, PRIVATE_DATA_SIGNATURE)

#pragma pack(1)

///
/// HII specific Vendor Device Path definition.
///
typedef struct {
  VENDOR_DEVICE_PATH       VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL End;
} HII_VENDOR_DEVICE_PATH;

#pragma pack()

//
// This is the generated IFR binary data for each formset defined in VFR.
// This data array is ready to be used as input of HiiAddPackages() to
// create a packagelist (which contains Form packages, String packages, etc).
//
extern UINT8 VfrBin[];

//
// This is the generated String package data for all .UNI files.
// This data array is ready to be used as input of HiiAddPackages() to
// create a packagelist (which contains Form packages, String packages, etc).
//
extern UINT8 PcieDeviceConfigDxeStrings[];

typedef struct {
  UINTN Signature;

  EFI_HANDLE     DriverHandle;
  EFI_HII_HANDLE HiiHandle;
  VARSTORE_DATA  LastVarStoreConfig;
  VARSTORE_DATA  VarStoreConfig;

  //
  // Consumed protocol
  //
  EFI_HII_DATABASE_PROTOCOL           *HiiDatabase;
  EFI_HII_STRING_PROTOCOL             *HiiString;
  EFI_HII_CONFIG_ROUTING_PROTOCOL     *HiiConfigRouting;
  EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL *HiiKeywordHandler;
  EFI_FORM_BROWSER2_PROTOCOL          *FormBrowser2;

  //
  // Produced protocol
  //
  EFI_HII_CONFIG_ACCESS_PROTOCOL ConfigAccess;
} PRIVATE_DATA;

#endif // PCIE_DEVICE_CONFIG_H_
