/** @file
  BMC Management setup screen

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/BmcInfoScreenHii.h>
#include <Guid/MdeModuleHii.h>
#include <IndustryStandard/Ipmi.h>
#include <IndustryStandard/IpmiNetFnApp.h>
#include <IndustryStandard/IpmiNetFnTransport.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/IpmiCommandLib.h>
#include <Library/IpmiCommandLibExt.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "BmcInfoScreenDxe.h"

//
// HII Handle for BMC Info Screen package
//
EFI_HII_HANDLE mHiiHandle;

/**
  This function updates info for Main form.

  @param[in] VOID

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval Other             Some error occurs when executing this entry point.

**/
EFI_STATUS
UpdateMainForm (
  VOID
  )
{
  EFI_IFR_GUID_LABEL          *EndLabel;
  EFI_IFR_GUID_LABEL          *StartLabel;
  EFI_STATUS                  Status;
  IPMI_GET_DEVICE_ID_RESPONSE DeviceId;
  BMC_LAN_INFO                BmcLanInfo;
  UINT16                      StrBuf[MAX_STRING_SIZE];
  UINT8                       BmcChannel;
  VOID                        *EndOpCodeHandle;
  VOID                        *StartOpCodeHandle;

  Status = IpmiGetDeviceId (&DeviceId);
  if (!EFI_ERROR (Status)
      && DeviceId.CompletionCode == IPMI_COMP_CODE_NORMAL)
  {
    //
    // Firmware Revision
    //
    UnicodeSPrint (
      StrBuf,
      sizeof (StrBuf),
      L"%d.%02d",
      DeviceId.FirmwareRev1.Bits.MajorFirmwareRev,
      BcdToDecimal8 (DeviceId.MinorFirmwareRev)
      );
    HiiSetString (mHiiHandle, STRING_TOKEN (STR_BMC_FIRMWARE_REV_VALUE), StrBuf, NULL);

    //
    // IPMI Version
    //
    UnicodeSPrint (
      StrBuf,
      sizeof (StrBuf),
      L"%d.%d",
      DeviceId.SpecificationVersion & 0x0F,
      (DeviceId.SpecificationVersion >> 4) & 0x0F
      );
    HiiSetString (mHiiHandle, STRING_TOKEN (STR_BMC_IPMI_VER_VALUE), StrBuf, NULL);
  }

  //
  // Initialize the container for dynamic opcodes
  //
  StartOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (StartOpCodeHandle != NULL);

  EndOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (EndOpCodeHandle != NULL);

  //
  // Create Hii Extend Label OpCode as the start opcode
  //
  StartLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                       StartOpCodeHandle,
                                       &gEfiIfrTianoGuid,
                                       NULL,
                                       sizeof (EFI_IFR_GUID_LABEL)
                                       );
  StartLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  StartLabel->Number       = LABEL_UPDATE;

  //
  // Create Hii Extend Label OpCode as the end opcode
  //
  EndLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                     EndOpCodeHandle,
                                     &gEfiIfrTianoGuid,
                                     NULL,
                                     sizeof (EFI_IFR_GUID_LABEL)
                                     );
  EndLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  EndLabel->Number       = LABEL_END;

  for (BmcChannel = 0; BmcChannel < BMC_MAX_CHANNEL; BmcChannel++) {
    ZeroMem (&BmcLanInfo, sizeof (BmcLanInfo));
    Status = IpmiGetBmcLanInfo (BmcChannel, &BmcLanInfo);
    if (EFI_ERROR (Status)
        || (BmcLanInfo.IpAddress.IpAddress[0] == 0))
    {
      continue;
    }

    UnicodeSPrint (
      StrBuf,
      sizeof (StrBuf),
      L"%d.%d.%d.%d",
      BmcLanInfo.IpAddress.IpAddress[0],
      BmcLanInfo.IpAddress.IpAddress[1],
      BmcLanInfo.IpAddress.IpAddress[2],
      BmcLanInfo.IpAddress.IpAddress[3]
      );

    HiiCreateTextOpCode (
      StartOpCodeHandle,
      STRING_TOKEN (STR_BMC_IP_ADDRESS_LABEL),
      STRING_TOKEN (STR_BMC_IP_ADDRESS_LABEL),
      HiiSetString (mHiiHandle, 0, StrBuf, NULL)
      );

    HiiUpdateForm (
      mHiiHandle,                 // HII handle
      &gBmcInfoScreenFormSetGuid, // Formset GUID
      MAIN_FORM_ID,               // Form ID
      StartOpCodeHandle,          // Label for where to insert opcodes
      EndOpCodeHandle             // Insert data
      );

    break;
  }

  return Status;
}

/**
  The user Entry Point for the BMC Screen driver.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval Other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
BmcInfoScreenEntry (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;
  EFI_HANDLE DriverHandle;

  Status = EFI_SUCCESS;
  DriverHandle = NULL;

  //
  // Publish our HII data
  //
  mHiiHandle = HiiAddPackages (
                 &gBmcInfoScreenFormSetGuid,
                 DriverHandle,
                 BmcInfoScreenDxeStrings,
                 VfrBin,
                 NULL
                 );

  ASSERT (mHiiHandle != NULL);

  Status = UpdateMainForm ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to update BMC Info Screen\n", __FUNCTION__));
  }

  return Status;
}
