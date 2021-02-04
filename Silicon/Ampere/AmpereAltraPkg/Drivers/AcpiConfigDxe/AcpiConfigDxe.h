/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _DRIVER_SAMPLE_H_
#define _DRIVER_SAMPLE_H_

#include <Uefi.h>
#include <Protocol/HiiConfigRouting.h>
#include <Protocol/HiiConfigAccess.h>
#include <Guid/MdeModuleHii.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/HiiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/HobLib.h>

#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Guid/AcpiConfigFormSet.h>
#include <Guid/PlatformInfoHobGuid.h>
#include <Library/AcpiHelperLib.h>
#include <Library/PMProLib.h>
#include <Library/AcpiHelperLib.h>
#include <PlatformInfoHob.h>
#include <AcpiNVDataStruc.h>

//
// This is the generated IFR binary data for each formset defined in VFR.
//
extern UINT8  VfrBin[];

//
// This is the generated String package data for all .UNI files.
//
extern UINT8  AcpiConfigDxeStrings[];

#define ACPI_CONFIG_PRIVATE_SIGNATURE SIGNATURE_32 ('A', 'C', 'P', 'I')

typedef struct {
  UINTN                            Signature;

  EFI_HANDLE                       DriverHandle;
  EFI_HII_HANDLE                   HiiHandle;
  ACPI_CONFIG_VARSTORE_DATA        Configuration;
  PlatformInfoHob_V2               *PlatformHob;
  EFI_ACPI_SDT_PROTOCOL            *AcpiTableProtocol;
  EFI_ACPI_HANDLE                  AcpiTableHandle;

  //
  // Consumed protocol
  //
  EFI_HII_CONFIG_ROUTING_PROTOCOL  *HiiConfigRouting;

  //
  // Produced protocol
  //
  EFI_HII_CONFIG_ACCESS_PROTOCOL   ConfigAccess;
} ACPI_CONFIG_PRIVATE_DATA;

#define ACPI_CONFIG_PRIVATE_FROM_THIS(a)  CR (a, ACPI_CONFIG_PRIVATE_DATA, ConfigAccess, ACPI_CONFIG_PRIVATE_SIGNATURE)

#pragma pack(1)

///
/// HII specific Vendor Device Path definition.
///
typedef struct {
  VENDOR_DEVICE_PATH             VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL       End;
} HII_VENDOR_DEVICE_PATH;

#pragma pack()

#endif
