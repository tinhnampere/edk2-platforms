/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "EnrollAmpereSecureKey.h"

//
// String token ID of help message text.
// Shell supports to find help message in the resource section of an application image if
// .MAN file is not found. This global variable is added to make build tool recognizes
// that the help string is consumed by user and then build tool will add the string into
// the resource section. Thus the application can use '-?' option to show help message in
// Shell.
//
GLOBAL_REMOVE_IF_UNREFERENCED EFI_STRING_ID  mStringHelpTokenId = STRING_TOKEN (STR_GET_HELP_EASK);

/**
  Entry point of Eask standalone application.

  @param ImageHandle            The image handle of the process.
  @param SystemTable            The EFI System Table pointer.

  @retval EFI_SUCCESS           Eask command is executed successfully.
  @retval EFI_ABORTED           HII package was failed to initialize.
  @retval others                Other errors when executing tftp command.
**/
EFI_STATUS
EFIAPI
EaskAppInitialize (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS  Status;

  mEaskHiiHandle = InitializeHiiPackage (ImageHandle);
  if (mEaskHiiHandle == NULL) {
    return EFI_ABORTED;
  }

  Status = (EFI_STATUS)RunEask (ImageHandle, SystemTable);
  HiiRemovePackages (mEaskHiiHandle);
  return Status;
}
