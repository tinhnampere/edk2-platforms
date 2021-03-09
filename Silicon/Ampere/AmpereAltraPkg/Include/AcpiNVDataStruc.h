/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ACPI_NV_DATA_STRUC_H_
#define ACPI_NV_DATA_STRUC_H_

#pragma pack(1)

//
// ACPI Configuration NV data structure definition
//
typedef struct {
  UINT32 EnableApeiSupport;
  UINT32 AcpiCppcEnable;
  UINT32 AcpiLpiEnable;
  UINT32 AcpiTurboSupport;
  UINT32 AcpiTurboMode;
  UINT32 Reserved[4];
} ACPI_CONFIG_VARSTORE_DATA;

//
// Ras Configuration NV data structure definition
//
typedef struct {
  UINT32 EnableApeiSupport;
  UINT32 RasHardwareEinj;
  UINT32 RasBertEnabled;
  UINT32 RasSdeiEnabled;
  UINT32 RasDdrCeThreshold;
  UINT32 Ras2pCeThreshold;
  UINT32 RasCpmCeThreshold;
  UINT32 RasLinkErrThreshold;
} RAS_CONFIG_VARSTORE_DATA;

#pragma pack()

#endif /* ACPI_NV_DATA_STRUC_H_ */
