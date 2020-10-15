/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCIEPATCHACPI_H__
#define __PCIEPATCHACPI_H__

EFI_STATUS
EFIAPI AcpiPatchPciMem32 (INT8 *PciSegEnabled);

EFI_STATUS
EFIAPI AcpiInstallMcfg (INT8 *PciSegEnabled);

EFI_STATUS
EFIAPI AcpiInstallIort (INT8 *PciSegEnabled);
#endif /* __PCIEPATCHACPI_H__ */
