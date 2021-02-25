/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef WATCHDOG_CONFIG_DXE_H_
#define WATCHDOG_CONFIG_DXE_H_

#include <Uefi.h>
#include <NVParamDef.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVParamLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/HiiConfigAccess.h>
#include <Protocol/HiiConfigRouting.h>

#include "NVDataStruc.h"

//
// This is the generated IFR binary data for each formset defined in VFR.
//
extern UINT8  VfrBin[];

//
// This is the generated String package data for all .UNI files.
//
extern UINT8  WatchdogConfigDxeStrings[];

#define WATCHDOG_CONFIG_PRIVATE_SIGNATURE SIGNATURE_32 ('W', 'D', 'T', 'C')

typedef struct {
  UINTN                            Signature;

  EFI_HANDLE                       DriverHandle;
  EFI_HII_HANDLE                   HiiHandle;
  WATCHDOG_CONFIG_VARSTORE_DATA    Configuration;

  //
  // Consumed protocol
  //
  EFI_HII_CONFIG_ROUTING_PROTOCOL  *HiiConfigRouting;

  //
  // Produced protocol
  //
  EFI_HII_CONFIG_ACCESS_PROTOCOL   ConfigAccess;
} WATCHDOG_CONFIG_PRIVATE_DATA;

#define WATCHDOG_CONFIG_PRIVATE_FROM_THIS(a)  CR (a, WATCHDOG_CONFIG_PRIVATE_DATA, ConfigAccess, WATCHDOG_CONFIG_PRIVATE_SIGNATURE)

#pragma pack(1)

///
/// HII specific Vendor Device Path definition.
///
typedef struct {
  VENDOR_DEVICE_PATH             VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL       End;
} HII_VENDOR_DEVICE_PATH;

#pragma pack()

EFI_STATUS
WatchdogConfigNvParamSet (
  IN WATCHDOG_CONFIG_VARSTORE_DATA *VarStoreConfig
  );

EFI_STATUS
WatchdogConfigNvParamGet (
  OUT WATCHDOG_CONFIG_VARSTORE_DATA *VarStoreConfig
  );

#endif /* WATCHDOG_CONFIG_DXE_H_ */
