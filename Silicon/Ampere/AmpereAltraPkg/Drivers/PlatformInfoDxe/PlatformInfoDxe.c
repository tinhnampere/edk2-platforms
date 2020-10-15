/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/HiiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformInfo.h>

//
// uni string and Vfr Binary data.
//
extern UINT8  VfrBin[];
extern UINT8  PlatformInfoDxeStrings[];

EFI_HANDLE                   mDriverHandle = NULL;
EFI_HII_HANDLE               mHiiHandle = NULL;

#pragma pack(1)

//
// HII specific Vendor Device Path definition.
//
typedef struct {
  VENDOR_DEVICE_PATH             VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL       End;
} HII_VENDOR_DEVICE_PATH;

#pragma pack()

// PLATFORM_INFO_FORMSET_GUID
CONST EFI_GUID gPlatformInfoFormSetGuid = {
  0x8DF0F6FB, 0x65A5, 0x434B, { 0xB2, 0xA6, 0xCE, 0xDF, 0xD2, 0x0A, 0x96, 0x8A }
  };

HII_VENDOR_DEVICE_PATH  mPlatformInfoHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8) (sizeof (VENDOR_DEVICE_PATH)),
        (UINT8) ((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    gPlatformInfoFormSetGuid
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8) (END_DEVICE_PATH_LENGTH),
      (UINT8) ((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

#define MAX_STRING_SIZE     64
#define MHZ_SCALE_FACTOR    1000000

STATIC
EFI_STATUS
UpdatePlatformInfoScreen (
  IN EFI_HII_HANDLE *HiiHandle
  )
{
  VOID                                *Hob;
  PlatformInfoHob_V2                  *PlatformHob;
  CONST EFI_GUID                      PlatformHobGuid = PLATFORM_INFO_HOB_GUID_V2;
  CHAR16                              Str[MAX_STRING_SIZE];

  /* Get the Platform HOB */
  Hob = GetFirstGuidHob (&PlatformHobGuid);
  if (Hob == NULL) {
    return EFI_DEVICE_ERROR;
  }
  PlatformHob = (PlatformInfoHob_V2 *) GET_GUID_HOB_DATA (Hob);

  /* SCP Version */
  AsciiStrToUnicodeStr((const CHAR8 *) PlatformHob->SmPmProVer, Str);
  HiiSetString (HiiHandle,
    STRING_TOKEN(STR_PLATFORM_INFO_SCPVER_VALUE),
    Str, NULL);

    /* SCP build */
  AsciiStrToUnicodeStr((const CHAR8 *) PlatformHob->SmPmProBuild, Str);
  HiiSetString (HiiHandle,
    STRING_TOKEN(STR_PLATFORM_INFO_SCPBUILD_VALUE),
    Str, NULL);

  /* CPU Info */
  AsciiStrToUnicodeStr((const CHAR8 *) PlatformHob->CpuInfo, Str);
  UnicodeSPrint (Str, sizeof (Str), L"%s", Str);
  HiiSetString (HiiHandle,
    STRING_TOKEN(STR_PLATFORM_INFO_CPUINFO_VALUE),
    Str, NULL);

  /* CPU clock */
  UnicodeSPrint (Str, sizeof (Str), L"%dMHz", PlatformHob->CpuClk / MHZ_SCALE_FACTOR);
  HiiSetString (HiiHandle,
    STRING_TOKEN (STR_PLATFORM_INFO_CPUCLK_VALUE),
    Str, NULL);

  /* PCP clock */
  UnicodeSPrint (Str, sizeof (Str), L"%dMHz", PlatformHob->PcpClk / MHZ_SCALE_FACTOR);
  HiiSetString (HiiHandle,
    STRING_TOKEN (STR_PLATFORM_INFO_PCPCLK_VALUE),
    Str, NULL);

  /* SOC clock */
  UnicodeSPrint (Str, sizeof (Str), L"%dMHz", PlatformHob->SocClk / MHZ_SCALE_FACTOR);
  HiiSetString (HiiHandle,
    STRING_TOKEN (STR_PLATFORM_INFO_SOCCLK_VALUE),
    Str, NULL);

  /* L1 Cache */
  UnicodeSPrint (Str, sizeof (Str), L"64KB");
  HiiSetString (HiiHandle,
    STRING_TOKEN (STR_PLATFORM_INFO_L1ICACHE_VALUE),
    Str, NULL);
  HiiSetString (HiiHandle,
    STRING_TOKEN (STR_PLATFORM_INFO_L1DCACHE_VALUE),
    Str, NULL);

  /* L2 Cache */
  UnicodeSPrint (Str, sizeof (Str), L"1MB");
  HiiSetString (HiiHandle,
    STRING_TOKEN (STR_PLATFORM_INFO_L2CACHE_VALUE),
    Str, NULL);

  /* AHB clock */
  UnicodeSPrint (Str, sizeof (Str), L"%dMHz", PlatformHob->AhbClk / MHZ_SCALE_FACTOR);
  HiiSetString (HiiHandle,
    STRING_TOKEN (STR_PLATFORM_INFO_AHBCLK_VALUE),
    Str, NULL);

  /* SYS clock */
  UnicodeSPrint (Str, sizeof (Str), L"%dMHz", PlatformHob->SysClk / MHZ_SCALE_FACTOR);
  HiiSetString (HiiHandle,
    STRING_TOKEN (STR_PLATFORM_INFO_SYSCLK_VALUE),
    Str, NULL);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PlatformInfoUnload (
  VOID
  )
{
  if (mDriverHandle != NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           mDriverHandle,
           &gEfiDevicePathProtocolGuid,
           &mPlatformInfoHiiVendorDevicePath,
           NULL
           );
    mDriverHandle = NULL;
  }

  if (mHiiHandle != NULL) {
    HiiRemovePackages (mHiiHandle);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PlatformInfoEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS                      Status;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mDriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mPlatformInfoHiiVendorDevicePath,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Publish our HII data
  //
  mHiiHandle = HiiAddPackages (
                 &gPlatformInfoFormSetGuid,
                 mDriverHandle,
                 PlatformInfoDxeStrings,
                 VfrBin,
                 NULL
                 );
  if (mHiiHandle == NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           mDriverHandle,
           &gEfiDevicePathProtocolGuid,
           &mPlatformInfoHiiVendorDevicePath,
           NULL
           );
    return EFI_OUT_OF_RESOURCES;
  }

  Status = UpdatePlatformInfoScreen (mHiiHandle);
  if (EFI_ERROR (Status)) {
    PlatformInfoUnload ();
    DEBUG ((DEBUG_ERROR, "%a %d Fail to update the platform info screen \n",
      __FUNCTION__, __LINE__));
    return Status;
  }

  return EFI_SUCCESS;
}
