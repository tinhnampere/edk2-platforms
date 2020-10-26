/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _ACPI_NV_DATA_STRUC_H_
#define _ACPI_NV_DATA_STRUC_H_

#pragma pack(1)

typedef struct {
  UINT32  EnableApeiSupport;
  UINT32  AcpiCppcEnable;
  UINT32  AcpiLpiEnable;
  UINT32  AcpiTurboSupport;
  UINT32  AcpiTurboMode;
  UINT32  Reserved[4];
} ACPI_CONFIG_VARSTORE_DATA;

#pragma pack()

#endif /* _ACPI_NV_DATA_STRUC_H_ */
