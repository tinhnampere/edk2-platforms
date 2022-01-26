/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "EnrollAmpereSecureKey.h"

/**
  * Structure contain command line option of application
**/
typedef struct {
  VOID     *DBUCert;         ///< Pointer to DBU key buffer
  UINTN    DBUCertSize;      ///< Size of DBU key buffer
  VOID     *DBBCert;         ///< Pointer to DBB key buffer
  UINTN    DBBCertSize;      ///< Size of DBB key buffer
} EASK_COMMAND_LINE_OPTIONS;

EFI_HII_HANDLE  mEaskHiiHandle;

STATIC UINTN               mArgCount;
STATIC CHAR16              **mArgVector;
STATIC EFI_SHELL_PROTOCOL  *mShellProtocol = NULL;

/**
  Get shell protocol.
  @param[out] ShellProtocol - Pointer to return shell protocol
**/
STATIC
VOID
GetShellProtocol (
  OUT EFI_SHELL_PROTOCOL **ShellProtocol
  )
{
  EFI_STATUS  Status;

  if (mShellProtocol == NULL) {
    Status = gBS->LocateProtocol (
                    &gEfiShellProtocolGuid,
                    NULL,
                    (VOID **)&mShellProtocol
                    );
    if (EFI_ERROR (Status)) {
      mShellProtocol = NULL;
    }
  }

  *ShellProtocol = mShellProtocol;
}

/**
  Read a file.

  @param[in]  FileName        The file to be read.
  @param[out] BufferSize      The file buffer size
  @param[out] Buffer          The file buffer

  @retval EFI_SUCCESS    Read file successfully
  @retval EFI_NOT_FOUND  Shell protocol or file not found
  @retval others         Read file failed
**/
STATIC
EFI_STATUS
ReadFileToBuffer (
  IN  CHAR16 *FileName,
  OUT UINTN  *BufferSize,
  OUT VOID   **Buffer
  )
{
  EFI_STATUS          Status;
  EFI_SHELL_PROTOCOL  *ShellProtocol;
  SHELL_FILE_HANDLE   Handle;
  UINT64              FileSize;
  UINTN               TempBufferSize;
  VOID                *TempBuffer;

  GetShellProtocol (&ShellProtocol);
  if (ShellProtocol == NULL) {
    return EFI_NOT_FOUND;
  }

  //
  // Open file by FileName.
  //
  Status = ShellProtocol->OpenFileByName (
                            FileName,
                            &Handle,
                            EFI_FILE_MODE_READ
                            );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Get the file size.
  //
  Status = ShellProtocol->GetFileSize (Handle, &FileSize);
  if (EFI_ERROR (Status)) {
    ShellProtocol->CloseFile (Handle);
    return Status;
  }

  TempBufferSize = (UINTN)FileSize;
  TempBuffer     = AllocateZeroPool (TempBufferSize);
  if (TempBuffer == NULL) {
    ShellProtocol->CloseFile (Handle);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Read the file data to the buffer
  //
  Status = ShellProtocol->ReadFile (
                            Handle,
                            &TempBufferSize,
                            TempBuffer
                            );
  if (EFI_ERROR (Status)) {
    ShellProtocol->CloseFile (Handle);
    FreePool (TempBuffer);
    return Status;
  }

  ShellProtocol->CloseFile (Handle);

  *BufferSize = TempBufferSize;
  *Buffer     = TempBuffer;

  return EFI_SUCCESS;
}

/**
  Check secure variable exist

  @param[in]  Name        Variable name
**/
EFI_STATUS
EaskCheckVariableExists (
  IN CHAR16 *Name
  )
{
  EFI_STATUS  Status;
  UINTN       DataSize = 0;
  UINT32      Attributes;

  Print (L"Getting %s Cert Variable\n", Name);
  Status = SecVarGetSecureVariable (
             Name,
             &gAmpereCertVendorGuid,
             &Attributes,
             NULL,
             &DataSize
             );

  if (Status != EFI_BUFFER_TOO_SMALL) {
    Print (L"Not found %s Cert\n", Name);
    return EFI_SUCCESS;
  } else if (DataSize != 0) {
    Print (L"Found Current %s Cert. Size = %d bytes\n", Name, DataSize);
    return EFI_SUCCESS;
  } else {
    Print (
      L"Internal error! %s cert exists but has size 0 - %r\n",
      Name,
      Status
      );
    return EFI_DEVICE_ERROR;
  }
}

/**
  Parse command line option

  @param[in] Argc            pointer to number of strings in Argv array
  @param[in] Argv            pointer to array of strings; one for each parameter
  @param[out]  Options       pointer to command line options output
**/
STATIC
EFI_STATUS
ParseCommandLineArgs (
  IN  UINTN                     Argc,
  IN  CHAR16                    **Argv,
  OUT EASK_COMMAND_LINE_OPTIONS *Options
  )
{
  UINTN       Index;
  EFI_STATUS  Status;

  if ((Options == NULL) || (Argv == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (Argc < 2) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_EASK_GEN_TOO_FEW),
      mEaskHiiHandle,
      ENROLL_AMPERE_SECURE_KEY_APP_NAME
      );
    return EFI_UNSUPPORTED;
  }

  for (Index = 1; Index < Argc; Index++) {
    if (StrCmp (Argv[Index], L"-K") == 0) {
      Index++;

      if (Index >= Argc) {
        ShellPrintHiiEx (
          -1,
          -1,
          NULL,
          STRING_TOKEN (STR_EASK_GEN_TOO_FEW),
          mEaskHiiHandle,
          ENROLL_AMPERE_SECURE_KEY_APP_NAME
          );
        return EFI_INVALID_PARAMETER;
      }

      Print (L"Reading DBU Cert: %s.\n", Argv[Index]);
      Status = ReadFileToBuffer (
                 Argv[Index],
                 &(Options->DBUCertSize),
                 &(Options->DBUCert)
                 );
      if (EFI_ERROR (Status)) {
        Print (L"Failed to read DBU Cert\n");
        return Status;
      }
    } else if (StrCmp (Argv[Index], L"-P") == 0) {
      Index++;

      if (Index >= Argc) {
        ShellPrintHiiEx (
          -1,
          -1,
          NULL,
          STRING_TOKEN (STR_EASK_GEN_TOO_FEW),
          mEaskHiiHandle,
          ENROLL_AMPERE_SECURE_KEY_APP_NAME
          );
        return EFI_INVALID_PARAMETER;
      }

      Print (L"Reading DBB Cert: %s.\n", Argv[Index]);
      Status = ReadFileToBuffer (
                 Argv[Index],
                 &(Options->DBBCertSize),
                 &(Options->DBBCert)
                 );
      if (EFI_ERROR (Status)) {
        Print (L"Failed to read DBB Cert\n");
        return Status;
      }
    } else if (StrCmp (Argv[Index], L"-k") == 0) {
      Index++;
      Print (L"Check DBU Cert.\n");
      Status = EaskCheckVariableExists (AMPERE_FWU_CERT_NAME);
      return Status;
    } else if (StrCmp (Argv[Index], L"-p") == 0) {
      Index++;
      Print (L"Check DBB Cert.\n");
      Status = EaskCheckVariableExists (AMPERE_FWA_CERT_NAME);
      return Status;
    } else {
      Index++;
    }
  }

  return EFI_SUCCESS;
}

/**

  This function parse application ARG.

  @return Status
**/
STATIC EFI_STATUS
GetArg (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS  Status;

  if (!ImageHandle || !SystemTable) {
    return EFI_INVALID_PARAMETER;
  }

  if (!gEfiShellParametersProtocol) {
    Status = gBS->OpenProtocol (
                    ImageHandle,
                    &gEfiShellParametersProtocolGuid,
                    (VOID **)&gEfiShellParametersProtocol,
                    ImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  mArgCount  = gEfiShellParametersProtocol->Argc;
  mArgVector = gEfiShellParametersProtocol->Argv;

  return EFI_SUCCESS;
}

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
  )
{
  EFI_STATUS                 Status;
  EASK_COMMAND_LINE_OPTIONS  Options = { 0 };
  UINT32                     Attributes;

  Print (L"Ampere Secure Keys Enroll Utility.\n");

  Status = GetArg (ImageHandle, SystemTable);
  if (EFI_ERROR (Status)) {
    Print (L"Please use UEFI SHELL to run this application!\n");
    return Status;
  }

  Status = ParseCommandLineArgs (mArgCount, mArgVector, &Options);
  if (EFI_ERROR (Status)) {
    Print (L"\nFailed to parse command line args.!\n");
    return EFI_SUCCESS;
  }

  if (Options.DBUCertSize) {
    Print (L"Attempting to Write DBU certificate.\n");

    Attributes = EFI_VARIABLE_NON_VOLATILE
                 | EFI_VARIABLE_RUNTIME_ACCESS
                 | EFI_VARIABLE_BOOTSERVICE_ACCESS
                 | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;

    Status = SecVarSetSecureVariable (
               AMPERE_FWU_CERT_NAME,
               &gAmpereCertVendorGuid,
               Attributes,
               Options.DBUCert,
               Options.DBUCertSize
               );

    if (EFI_ERROR (Status)) {
      Print (
        L"Failed to write new data to DBU Cert variable - %r\n",
        Status
        );

      goto Exit;
    }

    Status = EaskCheckVariableExists (AMPERE_FWU_CERT_NAME);
    if (EFI_ERROR (Status)) {
      Print (L"DBU Cert was deleted successfully\n");
    }

    goto Exit;
  }

  if (Options.DBBCertSize) {
    Print (L"Attempting to Write DBB certificate.\n");

    Attributes = EFI_VARIABLE_NON_VOLATILE
                 | EFI_VARIABLE_RUNTIME_ACCESS
                 | EFI_VARIABLE_BOOTSERVICE_ACCESS
                 | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;

    Status = SecVarSetSecureVariable (
               AMPERE_FWA_CERT_NAME,
               &gAmpereCertVendorGuid,
               Attributes,
               Options.DBBCert,
               Options.DBBCertSize
               );
    if (EFI_ERROR (Status)) {
      Print (
        L"Failed to write new data to DBB Cert variable - %r\n",
        Status
        );
      goto Exit;
    }

    Status = EaskCheckVariableExists (AMPERE_FWA_CERT_NAME);
    if (EFI_ERROR (Status)) {
      Print (L"DBB Cert was deleted successfully\n");
    }

    goto Exit;
  }

Exit:
  if (Options.DBUCert != NULL) {
    FreePool (Options.DBUCert);
  }

  if (Options.DBBCert != NULL) {
    FreePool (Options.DBBCert);
  }

  return EFI_SUCCESS;
}

/**
  Retrieve HII package list from ImageHandle and publish to HII database.

  @param ImageHandle            The image handle of the process.

  @return HII handle.
**/
EFI_HII_HANDLE
InitializeHiiPackage (
  EFI_HANDLE ImageHandle
  )
{
  EFI_STATUS                   Status;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;
  EFI_HII_HANDLE               HiiHandle;

  //
  // Retrieve HII package list from ImageHandle
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **)&PackageList,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  //
  // Publish HII package list to HII Database.
  //
  Status = gHiiDatabase->NewPackageList (
                           gHiiDatabase,
                           PackageList,
                           NULL,
                           &HiiHandle
                           );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return HiiHandle;
}
