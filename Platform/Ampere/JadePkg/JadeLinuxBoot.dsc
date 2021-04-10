## @file
#
# Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = Jade
  PLATFORM_GUID                  = 7BDD00C0-68F3-4CC1-8775-F0F00572019F
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x0001001B
  OUTPUT_DIRECTORY               = Build/Jade
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/Ampere/JadePkg/JadeLinuxBoot.fdf

  #
  # Defines for default states.  These can be changed on the command line.
  # -D FLAG=VALUE
  #
  DEFINE DEBUG_PRINT_ERROR_LEVEL = 0x8000000F
  DEFINE FIRMWARE_VER            = 0.01.001
  DEFINE EDK2_SKIP_PEICORE       = TRUE

# Include default Ampere Platform DSC file
!include Silicon/Ampere/AmpereAltraPkg/AmpereAltraLinuxBootPkg.dsc.inc

#
# Specific Platform Library
#
[LibraryClasses.common]
  #
  # RTC Library: Common RTC
  #
  RealTimeClockLib|Platform/Ampere/JadePkg/Library/PCF85063RealTimeClockLib/PCF85063RealTimeClockLib.inf

  #
  # Libraries needed byFailSafeDxe
  #
  FailSafeLib|Platform/Ampere/AmperePlatformPkg/Library/FailSafeLib/FailSafeLib.inf

[LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.DXE_DRIVER]
  SmbusLib|Platform/Ampere/JadePkg/Library/DxePlatformSmbusLib/DxePlatformSmbusLib.inf

[PcdsDynamicDefault.common.DEFAULT]
  # SMBIOS Type 0 - BIOS Information
  gAmpereTokenSpaceGuid.PcdSmbiosTables0BiosReleaseDate|"MM/DD/YYYY"

#
# Specific Platform Component
#
[Components.common]

  #
  # ACPI
  #
  MdeModulePkg/Universal/Acpi/AcpiTableDxe/AcpiTableDxe.inf
  Platform/Ampere/JadePkg/Drivers/AcpiPlatformDxe/AcpiPlatformDxe.inf
  Platform/Ampere/JadePkg/AcpiTables/AcpiTables.inf

  #
  # SMBIOS
  #
  MdeModulePkg/Universal/SmbiosDxe/SmbiosDxe.inf
  Platform/Ampere/JadePkg/Drivers/SmbiosPlatformDxe/SmbiosPlatformDxe.inf
  Platform/Ampere/JadePkg/Drivers/SmbiosCpuDxe/SmbiosCpuDxe.inf
  Platform/Ampere/JadePkg/Drivers/SmbiosMemInfoDxe/SmbiosMemInfoDxe.inf

  #
  # FailSafeDxe added to prevent watchdog from resetting the system
  #
  Platform/Ampere/AmperePlatformPkg/Drivers/FailSafeDxe/FailSafeDxe.inf
