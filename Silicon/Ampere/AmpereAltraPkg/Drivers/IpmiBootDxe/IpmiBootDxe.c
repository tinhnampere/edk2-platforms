
/** @file
  Retrieve Boot Flags settings from BMC and force UEFI to boot with configured option.

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/GlobalVariable.h>
#include <IndustryStandard/Ipmi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IpmiCommandLibExt.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/DevicePathToText.h>


//
// BBS Type for UEFI MENU
//
#define BBS_TYPE_MENU 0xFE

//
// Modules Variables
//
UINTN                     mBootOrderSize;
IPMI_BOOT_FLAGS_INFO      mBootFlags;
EFI_GUID                  mRestoreEventGuid;

/**
  The GetBBSTypeFromMessagingDevicePath() function retrieves BBS Type from a message device path.

  @param DevicePath[In]       Indicates the USB device path.

  @retval UINT16              Return BBS Type.

**/
UINT16
GetBBSTypeFromMessagingDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL *Node
  )
{
  UINT16                   Result;
  VENDOR_DEVICE_PATH       *VendorDevicePathNode;

  ASSERT (Node != NULL);

  DEBUG ((DEBUG_INFO, "%a Entry\n", __FUNCTION__));

  switch (DevicePathSubType (Node)) {
  case MSG_MAC_ADDR_DP:
    Result = BBS_TYPE_EMBEDDED_NETWORK;
    break;

  case MSG_USB_DP:
    //
    // TODO: Add support for USB devices
    //
    break;

  case MSG_SATA_DP:
  case MSG_NVME_NAMESPACE_DP:
    Result = BBS_TYPE_HARDDRIVE;
    break;

  case MSG_VENDOR_DP:
    VendorDevicePathNode = (VENDOR_DEVICE_PATH *)Node;
    if (&VendorDevicePathNode->Guid != NULL) {
      if (CompareGuid (&VendorDevicePathNode->Guid, &((EFI_GUID) DEVICE_PATH_MESSAGING_SAS))) {
        Result = BBS_TYPE_HARDDRIVE;
      }
    }
    break;

  default:
    Result = BBS_TYPE_UNKNOWN;
    break;
  }

  return Result;
}

/**
  The GetBBSTypeFromMediaDevicePath() function retrieves BBS Type from a Media device path.

  @param DevicePath[In]    Indicates the Media device path.
  @param Node[In]          Current Node of Media device path.

  @retval UINT16           Return BBS Type.

**/
UINT16
GetBBSTypeFromMediaDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL *Node
  )
{
  UINT16                             Result;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  *FvDevicePathNode;

  DEBUG ((DEBUG_INFO, "%a Entry\n", __FUNCTION__));

  ASSERT (Node != NULL);

  switch (DevicePathSubType (Node)) {
  case MEDIA_CDROM_DP:
    Result = BBS_TYPE_CDROM;
    break;

  case MEDIA_PIWG_FW_FILE_DP:
    FvDevicePathNode = (MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)Node;
    if (&FvDevicePathNode->FvFileName != NULL) {
      if (CompareGuid (&FvDevicePathNode->FvFileName, PcdGetPtr (PcdBootManagerMenuFile))) {
        Result = BBS_TYPE_MENU;
      }
    }
    break;

  default:
    Result = BBS_TYPE_UNKNOWN;
    break;
  }

  return Result;
}

/**
  The GetBBSTypeByDevicePath() function retrieves BBS Type from a device path.

  @param DevicePath[In]    Indicates the device path.

  @retval UINT16           Return BBS Type.

**/
UINT16
GetBBSTypeByDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL *DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL *Node;
  UINT16                   Result;

  DEBUG ((DEBUG_INFO, "%a Entry\n", __FUNCTION__));

  ASSERT (DevicePath != NULL);

  Result = BBS_TYPE_UNKNOWN;
  if (DevicePath == NULL) {
    return Result;
  }

  Node = DevicePath;
  while (!IsDevicePathEnd (Node)) {
    switch (DevicePathType (Node)) {
    case MEDIA_DEVICE_PATH:
      Result = GetBBSTypeFromMediaDevicePath (Node);
      break;

    case MESSAGING_DEVICE_PATH:
      Result = GetBBSTypeFromMessagingDevicePath (Node);
      break;

    default:
      Result = BBS_TYPE_UNKNOWN;
      break;
    }

    if (Result != BBS_TYPE_UNKNOWN) {
      break;
    }

    Node = NextDevicePathNode (Node);
  }

  return Result;
}

/**
  Callback function to restore old BootOrder if Persistent field of BootFlags is not set.
  is installed.

  @param  Event         Event whose notification function is being invoked.
  @param  Context       Pointer to the notification function's context, which is
                        always zero in current implementation.

**/
VOID
EFIAPI
RestoreBootOrderCallback (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  EFI_STATUS                Status;
  UINT16                    *BootOrder;

  DEBUG ((DEBUG_INFO, "%a Entry\n", __FUNCTION__));

  ASSERT (Context != NULL);
  BootOrder = (UINT16 *)Context;

  DEBUG_CODE (
    UINT8   Index;
    for (Index = 0; Index < mBootOrderSize; Index++) {
      DEBUG ((DEBUG_INFO, "0x%x\n", BootOrder[Index]));
    }
    );

  Status = gRT->SetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS
                  | EFI_VARIABLE_NON_VOLATILE,
                  mBootOrderSize,
                  BootOrder
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: SetVariable BootOrder %r\n", __FUNCTION__, Status));
  }

  FreePool (BootOrder);
}

/**
  Update BootOrder variable to let BDS boot with correct order specified by mBootFlags.

  @param  NewBootOrder[In]         Pointer to the buffer stored desired BootOrder.
  @param  BootOrder[In]            Pointer to current BootOrder.

**/
VOID
UpdateBootOrder (
  IN UINT16  *NewBootOrder,
  IN UINT16  *BootOrder
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   Event;

  DEBUG ((DEBUG_INFO, "%a Entry\n", __FUNCTION__));

  ASSERT (NewBootOrder != NULL);
  ASSERT (BootOrder != NULL);

  Status = gRT->SetVariable (
                  L"BootOrder",
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS
                  | EFI_VARIABLE_NON_VOLATILE,
                  mBootOrderSize,
                  NewBootOrder
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Set BootOrder Variable %r\n", __FUNCTION__, Status));
    return;
  }

  //
  // Register function to restore BootOrder if Persistent is not set
  //
  if (!mBootFlags.IsPersistent) {
    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    RestoreBootOrderCallback,
                    BootOrder,
                    &mRestoreEventGuid,
                    &Event
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Register event %r\n", __FUNCTION__, Status));
    }
  } else {
    FreePool (BootOrder);
  }
}

/**
  This function retrieve and check the BBS Type of a Boot Options with input BBS Type.

  @param  OptionNumber[In]        Pointer to the buffer stored desired BootOrder.
  @param  BootType[In]            BBS Type to compare with.

  @retval BOOLEAN                 TRUE if BBS Type of input option is identical with BootType.
                                  Otherwise, return FALSE.
**/
BOOLEAN
CompareBBSType (
  UINT16 OptionNumber,
  UINT8  BootType
  )
{
  EFI_STATUS                   Status;
  EFI_BOOT_MANAGER_LOAD_OPTION Option;
  CHAR16                       OptionName[sizeof ("Boot####")];

  UnicodeSPrint (OptionName, sizeof (OptionName), L"Boot%04x", OptionNumber);
  Status = EfiBootManagerVariableToLoadOption (OptionName, &Option);
  if (!EFI_ERROR (Status)) {

    DEBUG_CODE (
      CHAR16 *Str = ConvertDevicePathToText (Option.FilePath, TRUE, TRUE);
      if (Str != NULL) {
        DEBUG ((DEBUG_INFO, "Boot0x%04x: %s\n", OptionNumber, Str));
        FreePool (Str);
      }
      );

    if (GetBBSTypeByDevicePath (Option.FilePath) == BootType) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  This function iterate over all boot option in current BootOrder and build a new BootOrder.

  @param  BootType[In]            BBS Type to find for.
**/
VOID
SetBootOrder (
  IN UINT16 BootType
  )
{
  UINT16                       *NewBootOrder;
  UINT16                       *RemainOptions;
  UINT16                       *BootOrder;
  UINTN                        Index;
  UINTN                        SelectCnt;
  UINTN                        RemainCnt;

  DEBUG ((DEBUG_INFO, "%a Entry\n", __FUNCTION__));

  GetEfiGlobalVariable2 (L"BootOrder", (VOID **) &BootOrder, &mBootOrderSize);
  if (BootOrder == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: BootOrder NULL\n", __FUNCTION__));
    return;
  }

  NewBootOrder = AllocatePool (mBootOrderSize);
  RemainOptions = AllocatePool (mBootOrderSize);

  if ((NewBootOrder == NULL) || (RemainOptions == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Out of resources", __FUNCTION__));
    goto Exit;
  }

  SelectCnt = 0;
  RemainCnt = 0;

  for (Index = 0; Index < mBootOrderSize / sizeof (UINT16); Index++) {
    if (CompareBBSType (BootOrder[Index], BootType)) {
      NewBootOrder[SelectCnt++] = BootOrder[Index];
    } else {
      RemainOptions[RemainCnt++] = BootOrder[Index];
    }
  }

  if (SelectCnt != 0) {
    //
    // Append RemainOptions to NewBootOrder
    //
    for (Index = 0; Index < RemainCnt; Index++) {
      NewBootOrder[SelectCnt + Index] = RemainOptions[Index];
    }

    if (CompareMem (NewBootOrder, BootOrder, mBootOrderSize) != 0) {
      UpdateBootOrder (NewBootOrder, BootOrder);
    }
  }

Exit:
  if (NewBootOrder != NULL) {
    FreePool (NewBootOrder);
  }
  if (RemainOptions != NULL) {
    FreePool (RemainOptions);
  }
}

/**
  This function retrieve BootFlags and BootInfoAck to force Boot Order.

  @param  Event         Event whose notification function is being invoked.
  @param  Context       Pointer to the notification function's context, which is
                        always zero in current implementation.
**/
VOID
EFIAPI
HandleBootTypeCallback (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_STATUS                              Status;
  UINT16                                  *BootOrder;
  UINT16                                  BootType;
  UINT64                                  OsIndication;
  UINT8                                   BootInfoAck;
  UINTN                                   DataSize;

  DEBUG ((DEBUG_INFO, "%a Entry\n", __FUNCTION__));

  BootType = 0;
  BootInfoAck = 0;

  Status = IpmiGetBootFlags (&mBootFlags);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get Boot Flags\n", __FUNCTION__));
    goto Exit;
  }

  Status = IpmiGetBootInfoAck (&BootInfoAck);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get Boot Info Ack\n", __FUNCTION__));
    goto Exit;
  }

  DEBUG ((
    DEBUG_INFO,
    "Boot Flags Valid %d, Boot Info Ack %d\n",
    mBootFlags.IsBootFlagsValid, BootInfoAck
    ));

  if (!mBootFlags.IsBootFlagsValid
      || BootInfoAck != BOOT_OPTION_HANDLED_BY_BIOS)
  {
    goto Exit;
  }

  mRestoreEventGuid = gEfiEventReadyToBootGuid;

  switch (mBootFlags.DeviceSelector) {
  case IPMI_BOOT_DEVICE_SELECTOR_PXE:
    BootType = BBS_TYPE_EMBEDDED_NETWORK;
    break;

  case IPMI_BOOT_DEVICE_SELECTOR_HARDDRIVE:
    BootType = BBS_TYPE_HARDDRIVE;
    //
    // FIXME: CentOS Shim might change BootOrder, so wait until ExitBootService is called.
    //
    mRestoreEventGuid = gEfiEventExitBootServicesGuid;
    break;

  case IPMI_BOOT_DEVICE_SELECTOR_CD_DVD:
    BootType = BBS_TYPE_CDROM;
    break;

  case IPMI_BOOT_DEVICE_SELECTOR_BIOS_SETUP:
    BootType = BBS_TYPE_MENU;
    //
    // FIXME: Restore BootOrder as soon as possible.
    //
    mRestoreEventGuid = gPlatformManagerEntryEventGuid;
    break;

  case IPMI_BOOT_DEVICE_SELECTOR_FLOPPY:
    BootType = BBS_TYPE_USB;
    break;

  default:
    goto Exit;
  }

  SetBootOrder (BootType);

  //
  // Update BMC status
  //
  Status = IpmiSetBootInfoAck ();
  if (!EFI_ERROR (Status)) {
    IpmiClearBootFlags ();
  }

Exit:
  //
  // UiApp was registered with HIDDEN attribute and will be ignored by BDS.
  // Therefore, if first BootOption is UiApp, Manually set Boot OS Indication.
  //
  DataSize = sizeof (UINT64);
  GetEfiGlobalVariable2 (L"BootOrder", (VOID **)&BootOrder, &mBootOrderSize);
  if (BootOrder != NULL) {
    if (CompareBBSType (BootOrder[0], BBS_TYPE_MENU)) {
      Status = gRT->GetVariable (
                      EFI_OS_INDICATIONS_VARIABLE_NAME,
                      &gEfiGlobalVariableGuid,
                      NULL,
                      &DataSize,
                      &OsIndication
                      );
      OsIndication |= EFI_OS_INDICATIONS_BOOT_TO_FW_UI;
      Status = gRT->SetVariable (
                      EFI_OS_INDICATIONS_VARIABLE_NAME,
                      &gEfiGlobalVariableGuid,
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
                      DataSize,
                      &OsIndication
                      );
      ASSERT_EFI_ERROR (Status);
    }
  }

  if (BootOrder != NULL) {
    FreePool (BootOrder);
  }

  gBS->CloseEvent (Event);
}

/**
  The Entry Point for IPMI Boot handler.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval Other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
IpmiBootEntry (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;
  EFI_EVENT  EndOfDxeEvent;

  Status = gBS->CreateEventEx (
                EVT_NOTIFY_SIGNAL,
                TPL_CALLBACK,
                HandleBootTypeCallback,
                NULL,
                &gEfiEndOfDxeEventGroupGuid,
                &EndOfDxeEvent
                );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
