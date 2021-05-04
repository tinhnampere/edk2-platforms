/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef AC01_PCIE_PATCH_ACPI_H_
#define AC01_PCIE_PATCH_ACPI_H_

EFI_STATUS
EFIAPI
AcpiPatchPciMem32 (
  INT8 *PciSegEnabled
  );

EFI_STATUS
EFIAPI
AcpiInstallMcfg (
  INT8 *PciSegEnabled
  );

EFI_STATUS
EFIAPI
AcpiInstallIort (
  INT8 *PciSegEnabled
  );

#endif /* AC01_PCIE_PATCH_ACPI_H_ */
