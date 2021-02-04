/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _ACPI_PLATFORM_H_
#define _ACPI_PLATFORM_H_

#include <Uefi.h>
#include <IndustryStandard/Acpi63.h>
#include <Guid/EventGroup.h>
#include <Guid/PlatformInfoHobGuid.h>
#include <Protocol/AcpiTable.h>
#include <Library/UefiLib.h>
#include <Library/AcpiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/HobLib.h>

#include <Library/AcpiHelperLib.h>
#include <Library/AmpereCpuLib.h>
#include <Platform/Ac01.h>
#include <PlatformInfoHob.h>
#include <AcpiHeader.h>

EFI_STATUS
AcpiPatchDsdtTable (
  VOID
  );

EFI_STATUS
AcpiInstallMadtTable (
  VOID
  );

EFI_STATUS
AcpiInstallNfitTable (
  VOID
  );

EFI_STATUS
AcpiPcctInit (
  VOID
  );

EFI_STATUS
AcpiInstallPcctTable (
  VOID
  );

EFI_STATUS
AcpiInstallPpttTable (
  VOID
  );

EFI_STATUS
AcpiInstallSlitTable (
  VOID
  );

EFI_STATUS
AcpiInstallSratTable (
  VOID
  );

#endif /* _ACPI_PLATFORM_H_ */
