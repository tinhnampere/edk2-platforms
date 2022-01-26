/** @file
  Header file for 'eask' command functions.

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ENROLL_AMPERE_SECURE_KEY_H_
#define ENROLL_AMPERE_SECURE_KEY_H_

#include <Uefi.h>

#include <Protocol/HiiPackageList.h>
#include <Protocol/ServiceBinding.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/ShellLib.h>
#include <Library/SecVarLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>

#define ENROLL_AMPERE_SECURE_KEY_APP_NAME  L"eask"

extern EFI_HII_HANDLE  mEaskHiiHandle;

/**
  Function for 'eask' command.

  @param[in]  ImageHandle     The image handle.
  @param[in]  SystemTable     The system table.

  @retval SHELL_SUCCESS            Command completed successfully.
  @retval SHELL_INVALID_PARAMETER  Command usage error.
  @retval SHELL_ABORTED            The user aborts the operation.
  @retval value                    Unknown error.
**/
SHELL_STATUS
RunEask (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  );

/**
  Retrieve HII package list from ImageHandle and publish to HII database.

  @param ImageHandle            The image handle of the process.

  @return HII handle.
**/
EFI_HII_HANDLE
InitializeHiiPackage (
  EFI_HANDLE ImageHandle
  );

#endif // ENROLL_AMPERE_SECURE_KEY_H_
