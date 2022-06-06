/** @file
  Ipmi utilities to test interaction with BMC via ipmi command

  Copyright (c) 2022, Ampere Computing. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Protocol/IpmiProtocol.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "IpmiRawCommand.h"
#include "IpmiUtilHelper.h"

#define MAX_IPMI_CMD_DATA_SUPPORT  256

/**
  Wrapper Ipmi protocol
**/
EFI_STATUS
EFIAPI
IpmiUtilSendRawCommand (
  IN     UINT8  NetFunction,
  IN     UINT8  Command,
  IN     UINT8  *RequestData,
  IN     UINT32 RequestDataSize,
  OUT    UINT8  *ResponseData,
  IN OUT UINT32 *ResponseDataSize
  )
{
  EFI_STATUS     Status;
  IPMI_PROTOCOL  *IpmiProtocol;

  Status = gBS->LocateProtocol (
                  &gIpmiProtocolGuid,
                  NULL,
                  (VOID **)&IpmiProtocol
                  );
  if (EFI_ERROR (Status)) {
    //
    // Ipmi Protocol is not installed. So, IPMI device is not present.
    //
    DEBUG ((DEBUG_ERROR, "IpmiUtil: Ipmi device is not present - %r\n", Status));
    return Status;
  }

  Status = IpmiProtocol->IpmiSubmitCommand (
                           IpmiProtocol,
                           NetFunction,
                           Command,
                           RequestData,
                           RequestDataSize,
                           ResponseData,
                           ResponseDataSize
                           );
  if (EFI_ERROR (Status)) {
    Print (L"IpmiUtil: Send command to ipmi device failed - %r\n", Status);
    return Status;
  }

  return EFI_SUCCESS;
}

/**
 Handler of Ipmi command with raw option.

 @param ArgList          List entry of arguments.
 @param HiiPackageHandle Hii Handle which is registered by App or Driver.

 @retval EFI_SUCCESS Command successfully executed
 @retval EFI_INVALID_PARAMETER One or more parameters are invalid
 @retval EFI_ABORTED Command failed
 @retval Other       Error unexpected
**/
EFI_STATUS
EFIAPI
IpmiUtilRawCommandHandler (
  IN LIST_ENTRY CONST *ArgList,
  IN EFI_HII_HANDLE   HiiPackageHandle
  )
{
  BOOLEAN     IsVerbose;
  EFI_STATUS  Status;
  CHAR16      *CmdAsString;
  UINT8       *CmdData = NULL;
  UINT8       ResponseData[MAX_IPMI_CMD_DATA_SUPPORT];
  UINTN       ResponseSize, ResponseStringOutputSize;
  UINTN       Index, DataIndex, StartCmdData;
  CHAR16      ResponseStringOutput[MAX_IPMI_CMD_DATA_SUPPORT * 4];

  ResponseSize = MAX_IPMI_CMD_DATA_SUPPORT;

  IsVerbose = ShellCommandLineGetFlag (ArgList, L"-v");

  CmdAsString = (CHAR16 *)ShellCommandLineGetValue (ArgList, L"-r");

  if (CmdAsString == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0, DataIndex = 0, StartCmdData = 0;
       CmdAsString[Index] != L'\0';
       ++DataIndex, ++Index)
  {
    CmdData = ReallocatePool (
                sizeof (UINT8) + DataIndex,
                sizeof (UINT8) + DataIndex + 1,
                (VOID *)CmdData
                );
    if (CmdData == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    Status = IpmiUtilUniStrToNumber (
               CmdAsString + Index,
               CmdData + DataIndex
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Fail to convert string to number\r\n"));
      Status = EFI_ABORTED;
      goto Cleanup;
    }

    //
    // Find next hex string.
    //
    while (CmdAsString[Index] != L' ') {
      if (CmdAsString[Index] == L'\0') {
        --Index;
        break;
      }

      ++Index;
    }

    //
    // Catching position of command data
    //
    if (DataIndex == 1) {
      if (CmdAsString[Index + 1] == L'\0') {
        StartCmdData = Index + 1;
      } else {
        StartCmdData = Index;
      }
    }
  }

  if (Index < 2) {
    DEBUG ((
      DEBUG_ERROR,
      "IpmiUtil: Dont have enough information to create ipmi request with raw data\n"
      ));
    Status = EFI_ABORTED;
    goto Cleanup;
  }

  if (IsVerbose) {
    Status = ShellPrintHiiEx (
               -1,
               -1,
               NULL,
               STRING_TOKEN (STR_GEN_IPMI_CMD_INFO),
               HiiPackageHandle,
               L"IpmiUtil",
               *CmdData,
               *(CmdData + 1),
               *(CmdAsString + StartCmdData) != L'\0' ?
               CmdAsString + StartCmdData : L"No data"
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Fail to print command information - %r\r\n", Status));
    }
  }

  Status = IpmiUtilSendRawCommand (
             *CmdData,
             *(CmdData + 1),
             (DataIndex - 2) == 0 ? NULL : (CmdData + 2),
             DataIndex - 2,
             ResponseData,
             (UINT32 *)&ResponseSize
             );
  if (EFI_ERROR (Status)) {
    Status = EFI_ABORTED;
    goto Cleanup;
  }

  if (IsVerbose) {
    IpmiUtilErrorCatching (HiiPackageHandle, ResponseData);
  }

  //
  // Do not show completion code
  //
  Status = IpmiUtilNumberToHexJoin (
             ResponseData + 1,
             ResponseSize,
             L' ',
             &ResponseStringOutputSize,
             ResponseStringOutput
             );
  if (EFI_ERROR (Status)) {
    Status = EFI_ABORTED;
    goto Cleanup;
  }

  Status = ShellPrintHiiEx (
             -1,
             -1,
             NULL,
             STRING_TOKEN (STR_GEN_IPMI_RESPONSE),
             HiiPackageHandle,
             ResponseStringOutput
             );
  if (EFI_ERROR (Status)) {
    goto Cleanup;
  }

Cleanup:
  if (CmdData != NULL) {
    FreePool ((VOID *)CmdData);
  }

  return Status;
}
