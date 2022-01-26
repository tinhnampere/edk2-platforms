/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "EnrollAmpereSecureKey.h"
#include <Protocol/ShellDynamicCommand.h>

/**
  This is the shell command handler function pointer callback type.  This
  function handles the command when it is invoked in the shell.

  @param[in] This                   The instance of the EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL.
  @param[in] SystemTable            The pointer to the system table.
  @param[in] ShellParameters        The parameters associated with the command.
  @param[in] Shell                  The instance of the shell protocol used in the context
                                    of processing this command.

  @return EFI_SUCCESS               the operation was successful
  @return other                     the operation failed.
**/
SHELL_STATUS
EFIAPI
EaskCommandHandler (
  IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL *This,
  IN EFI_SYSTEM_TABLE                   *SystemTable,
  IN EFI_SHELL_PARAMETERS_PROTOCOL      *ShellParameters,
  IN EFI_SHELL_PROTOCOL                 *Shell
  )
{
  gEfiShellParametersProtocol = ShellParameters;
  gEfiShellProtocol = Shell;
  return RunEask (gImageHandle, SystemTable);
}

/**
  This is the command help handler function pointer callback type.  This
  function is responsible for displaying help information for the associated
  command.

  @param[in] This                   The instance of the EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL.
  @param[in] Language               The pointer to the language string to use.

  @return string                    Pool allocated help string, must be freed by caller
**/
CHAR16 *
EFIAPI
EaskCommandGetHelp (
  IN EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL *This,
  IN CONST CHAR8                        *Language
  )
{
  return HiiGetString (mEaskHiiHandle, STRING_TOKEN (STR_GET_HELP_EASK), Language);
}

EFI_SHELL_DYNAMIC_COMMAND_PROTOCOL  mEaskDynamicCommand = {
  ENROLL_AMPERE_SECURE_KEY_APP_NAME,
  EaskCommandHandler,
  EaskCommandGetHelp
};

/**
  Entry point of Eask Dynamic Command.

  Produce the DynamicCommand protocol to handle "eask" command.

  @param ImageHandle            The image handle of the process.
  @param SystemTable            The EFI System Table pointer.

  @retval EFI_SUCCESS           Eask command is executed successfully.
  @retval EFI_ABORTED           HII package was failed to initialize.
  @retval others                Other errors when executing eask command.
**/
EFI_STATUS
EFIAPI
EaskCommandInitialize (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS  Status;

  mEaskHiiHandle = InitializeHiiPackage (ImageHandle);
  if (mEaskHiiHandle == NULL) {
    return EFI_ABORTED;
  }

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEfiShellDynamicCommandProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mEaskDynamicCommand
                  );
  ASSERT_EFI_ERROR (Status);
  return Status;
}

/**
  Eask driver unload handler.

  @param ImageHandle            The image handle of the process.

  @retval EFI_SUCCESS           The image is unloaded.
  @retval Others                Failed to unload the image.
**/
EFI_STATUS
EFIAPI
EaskUnload (
  IN EFI_HANDLE ImageHandle
  )
{
  EFI_STATUS  Status;

  Status = gBS->UninstallProtocolInterface (
                  ImageHandle,
                  &gEfiShellDynamicCommandProtocolGuid,
                  &mEaskDynamicCommand
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  HiiRemovePackages (mEaskHiiHandle);
  return EFI_SUCCESS;
}
