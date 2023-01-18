/** @file
  Configuration Manager Dxe

  Copyright (c) 2023, Ampere Computing. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CONFIGURATION_MANAGER_H_
#define CONFIGURATION_MANAGER_H_
#include <Uefi.h>
#include <Protocol/ConfigurationManagerProtocol.h>

extern CHAR8 dsdt_aml_code[];

/** The configuration manager version.
*/
#define CONFIGURATION_MANAGER_REVISION CREATE_REVISION (1, 0)

/** The OEM ID
*/
#define CFG_MGR_OEM_ID    { 'A', 'M', 'P', 'E', 'R', 'E' }

typedef struct PlatformRepositoryInfo {
  /// Configuration Manager Information
  CM_STD_OBJ_CONFIGURATION_MANAGER_INFO CmInfo;

  /// List of ACPI tables
  CM_STD_OBJ_ACPI_TABLE_INFO            CmAcpiTableList[1];

  /// Boot architecture information
  CM_ARM_BOOT_ARCH_INFO                 BootArchInfo;

  /// Fixed feature flag information
  CM_ARM_FIXED_FEATURE_FLAGS            FixedFeatureFlags;

} EDKII_PLATFORM_REPOSITORY_INFO;


#endif /* CONFIGURATION_MANAGER_H */
