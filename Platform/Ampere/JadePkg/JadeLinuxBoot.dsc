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
  gAmpereTokenSpaceGuid.PcdSmbiosTables0BiosVendor|"Ampere(R)"
  gAmpereTokenSpaceGuid.PcdSmbiosTables0BiosVersion|"TianoCore EDKII"
  gAmpereTokenSpaceGuid.PcdSmbiosTables0BiosReleaseDate|"MM/DD/YYYY"

  # SMBIOS Type 1 - System Information
  gAmpereTokenSpaceGuid.PcdSmbiosTables1SystemManufacturer|"Ampere(R)"
  gAmpereTokenSpaceGuid.PcdSmbiosTables1SystemProductName|"Mt Jade"
  gAmpereTokenSpaceGuid.PcdSmbiosTables1SystemVersion|"0.3"
  gAmpereTokenSpaceGuid.PcdSmbiosTables1SystemSerialNumber|"0123-4567-89AB-CDEF"
  gAmpereTokenSpaceGuid.PcdSmbiosTables1SystemSkuNumber|"01234567"

  # FRU Chassis Information
  gAmpereTokenSpaceGuid.PcdFruChassisPartNumber|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruChassisSerialNumber|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruChassisExtra|"To be filled by O.E.M.                             "

  # FRU Board Information
  gAmpereTokenSpaceGuid.PcdFruBoardManufacturerName|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruBoardProductName|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruBoardSerialNumber|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruBoardPartNumber|"To be filled by O.E.M.                             "

  # FRU Product Information
  gAmpereTokenSpaceGuid.PcdFruProductManufacturerName|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruProductName|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruProductPartNumber|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruProductVersion|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruProductSerialNumber|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruProductAssetTag|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruProductFruFileId|"To be filled by O.E.M.                             "
  gAmpereTokenSpaceGuid.PcdFruProductExtra|"To be filled by O.E.M.                             "

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
