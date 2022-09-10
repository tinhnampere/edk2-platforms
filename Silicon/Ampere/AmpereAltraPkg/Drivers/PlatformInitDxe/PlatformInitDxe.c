/** @file

   Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

   SPDX-License-Identifier: BSD-2-Clause-Patent

 **/

#include <Uefi.h>

#include <IndustryStandard/SmBios.h>
#include <Library/AmpereCpuLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FlashLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>

#include "PlatformInitDxe.h"

UINT16
CheckCrc16 (
    IN UINT8 *Pointer,
    IN INTN Count
    )
{
    INTN Crc = 0;
    INTN Index;

    while (--Count >= 0) {
        Crc = Crc ^ (INTN)*Pointer++ << 8;
        for (Index = 0; Index < 8; ++Index) {
            if ((Crc & 0x8000) != 0) {
                Crc = Crc << 1 ^ 0x1021;
            } else {
                Crc = Crc << 1;
            }
        }
    }

    return Crc & 0xFFFF;
}

BOOLEAN
FailSafeValidCRC (
    IN FAIL_SAFE_CONTEXT *FailSafeBuf
    )
{
    BOOLEAN Valid;
    UINT16 Crc;
    UINT32 Len;

    Len = sizeof (FAIL_SAFE_CONTEXT);
    Crc = FailSafeBuf->CRC16;
    FailSafeBuf->CRC16 = 0;

    Valid = (Crc == CheckCrc16 ((UINT8 *)FailSafeBuf, Len));
    FailSafeBuf->CRC16 = Crc;

    return Valid;
}

BOOLEAN
FailSafeFailureStatus (
    IN UINT8 Status
    )
{
    if ((Status == FAILSAFE_BOOT_LAST_KNOWN_SETTINGS) ||
        (Status == FAILSAFE_BOOT_DEFAULT_SETTINGS) ||
        (Status == FAILSAFE_BOOT_DDR_DOWNGRADE))
    {
        return TRUE;
    }

    return FALSE;
}

EFI_STATUS
FailSafeClearContext (
    VOID
    )
{
    EFI_STATUS Status;
    FAIL_SAFE_CONTEXT FailSafeBuf;
    UINT32 FailSafeSize;
    UINT64 FailSafeStartOffset;

    Status = FlashGetFailSafeInfo (&FailSafeStartOffset, &FailSafeSize);
    if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to get context region information\n", __FUNCTION__));
        return EFI_DEVICE_ERROR;
    }

    Status = FlashReadCommand (FailSafeStartOffset, (UINT8 *)&FailSafeBuf, sizeof (FAIL_SAFE_CONTEXT));
    if (EFI_ERROR (Status)) {
        return Status;
    }

    //
    // If failsafe context is valid, and:
    //    - The status indicate non-failure, then don't clear it
    //    - The status indicate a failure, then go and clear it
    //
    if (  FailSafeValidCRC (&FailSafeBuf)
          && !FailSafeFailureStatus (FailSafeBuf.Status))
    {
        return EFI_SUCCESS;
    }

    return FlashEraseCommand (FailSafeStartOffset, FailSafeSize);
}

/**
  Prepare Firmware Version for SMBIOS Type 0. "PcdFirmwareVersionString"
  will contains version of UEFI and SCP.

  @param[out] PcdFirmwareVersionString PCD of Firmware Version String is updated
**/
VOID
UpdateFirmwareVersionString (
  VOID
  )
{
  UINT8      UnicodeStrLen;
  UINT8      FirmwareVersionStrLen;
  UINT8      FirmwareVersionStrSize;
  UINT8      *ScpVersion;
  UINT8      *ScpBuild;
  CHAR16     UnicodeStr[SMBIOS_STRING_MAX_LENGTH * sizeof (CHAR16)];
  CHAR16     *FirmwareVersionPcdPtr;

  FirmwareVersionStrLen  = 0;
  ZeroMem (UnicodeStr, sizeof (UnicodeStr));
  FirmwareVersionPcdPtr  = (CHAR16 *)PcdGetPtr (PcdFirmwareVersionString);
  FirmwareVersionStrSize = StrLen (FirmwareVersionPcdPtr) * sizeof (CHAR16);
  //
  // Format of PcdFirmwareVersionString is
  // "(MAJOR_VER).(MINOR_VER).(BUILD) Build YYYY.MM.DD", we only need
  // "(MAJOR_VER).(MINOR_VER).(BUILD)" showed in BIOS version. Using
  // space character to determine this string. Another case uses null
  // character to end while loop.
  //
  while (*FirmwareVersionPcdPtr != ' ' && *FirmwareVersionPcdPtr != '\0') {
    FirmwareVersionStrLen++;
    FirmwareVersionPcdPtr++;
  }
  FirmwareVersionPcdPtr = (CHAR16 *)PcdGetPtr (PcdFirmwareVersionString);
  UnicodeStrLen = FirmwareVersionStrLen * sizeof (CHAR16);
  CopyMem (UnicodeStr, FirmwareVersionPcdPtr, UnicodeStrLen);

  GetScpVersion (&ScpVersion);
  GetScpBuild (&ScpBuild);
  if (ScpVersion == NULL || ScpBuild == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:%d: Fail to get SMpro/PMpro information\n",
      __FUNCTION__,
      __LINE__));
    UnicodeSPrint (
      FirmwareVersionPcdPtr,
      FirmwareVersionStrSize,
      L"TianoCore %.*s (SYS: 0.00.00000000)",
      FirmwareVersionStrLen,
      (UINT16 *)UnicodeStr
    );
  } else {
    UnicodeSPrint (
      FirmwareVersionPcdPtr,
      FirmwareVersionStrSize,
      L"TianoCore %.*s (SYS: %a.%a)",
      FirmwareVersionStrLen,
      (UINT16 *)UnicodeStr,
      ScpVersion,
      ScpBuild
    );
  }
}

EFI_STATUS
EFIAPI
PlatformInitDxeEntryPoint (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
    )
{
    EFI_STATUS Status;

    //
    // In FailSafe context, there's one field to indicate which setting is using
    // to boot (BOOT_LAST_KNOWN_SETTINGS, BOOT_DEFAULT_SETTINGS, BOOT_NORMAL).
    //
    // At SCP and ATF side, they will check their NVPARAM for Failsafe
    // (NV_SI_PMPRO_FAILURE_FAILSAFE - NV_SI_ATF_FAILURE_FAILSAFE) to decide
    // which setting they will use. If Failsafe occurs at SCP or ATF, a mission
    // of UEFI at DXE phase is to clear Failsafe context for normal behavior.
    //

    Status = FailSafeClearContext ();
    ASSERT_EFI_ERROR (Status);

    UpdateFirmwareVersionString ();

    return Status;
}
