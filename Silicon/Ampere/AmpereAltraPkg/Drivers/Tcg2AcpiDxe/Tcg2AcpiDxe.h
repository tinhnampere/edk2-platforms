/** @file
  The header file for Tcg2 SMM driver.

Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef TCG2_ACPI_DXE_H_
#define TCG2_ACPI_DXE_H_

#include <PiDxe.h>

#include <Guid/TpmInstance.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/Tpm2Acpi.h>
#include <IndustryStandard/TpmPtp.h>
#include <Library/AcpiHelperLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/Tcg2PhysicalPresenceLib.h>
#include <Library/Tpm2CommandLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/TpmMeasurementLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <PlatformInfoHob.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/Tcg2Protocol.h>

//
// PNP _HID for TPM2 device
//
#define TPM_HID_TAG                        "NNNN0000"
#define TPM_HID_PNP_SIZE                   8
#define TPM_HID_ACPI_SIZE                  9

#define TPM_ACPI_OBJECT_PATH_LENGTH_MAX    256

#endif  // TCG2_ACPI_DXE_H_
