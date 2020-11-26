/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ACPIAPEI_H__
#define __ACPIAPEI_H__

#include <Base.h>
#include <IndustryStandard/Acpi63.h>
#include <Library/AcpiHelperLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/AcpiTable.h>
#include <Guid/AcpiConfigFormSet.h>
#include <AcpiNVDataStruc.h>

EFI_STATUS
EFIAPI
AcpiApeiUpdate (VOID);

#endif /* __ACPIAPEI_H__ */

