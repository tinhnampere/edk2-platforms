/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVParamLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <NVParamDef.h>

#include "WatchdogConfigDxe.h"

CHAR16 WatchDogConfigVarstoreDataName[] = L"WatchdogConfigNVData";

EFI_HANDLE                   mDriverHandle = NULL;
WATCHDOG_CONFIG_PRIVATE_DATA *mPrivateData = NULL;

HII_VENDOR_DEVICE_PATH mWatchdogConfigHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    WATCHDOG_CONFIG_FORMSET_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(END_DEVICE_PATH_LENGTH),
      (UINT8)((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

EFI_STATUS
WatchdogConfigNvParamGet (
  OUT WATCHDOG_CONFIG_VARSTORE_DATA *VarStoreConfig
  )
{
  EFI_STATUS Status;
  UINT32     Value;
  BOOLEAN    SetDefault;

  SetDefault = FALSE;
  Status = NVParamGet (
             NV_SI_SEC_WDT_BIOS_EXP_MINS,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status)) {
    VarStoreConfig->SecureWatchdogTimerTimeout = SWDT_DEFAULT_VALUE;
    if (Status == EFI_NOT_FOUND) {
      SetDefault = TRUE;
    } else {
      ASSERT (FALSE);
    }
  } else {
    VarStoreConfig->SecureWatchdogTimerTimeout = Value;
  }

  if (SetDefault) {
    WatchdogConfigNvParamSet (VarStoreConfig);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
WatchdogConfigNvParamSet (
  IN WATCHDOG_CONFIG_VARSTORE_DATA *VarStoreConfig
  )
{
  EFI_STATUS Status;
  UINT32     Value;

  Status = NVParamGet (
             NV_SI_SEC_WDT_BIOS_EXP_MINS,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status)
      || Value != VarStoreConfig->SecureWatchdogTimerTimeout)
  {
    Status = NVParamSet (
               NV_SI_SEC_WDT_BIOS_EXP_MINS,
               NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
               NV_PERM_BIOS | NV_PERM_MANU,
               VarStoreConfig->SecureWatchdogTimerTimeout
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**
  This function allows a caller to extract the current configuration for one
  or more named elements from the target driver.

  @param  This                   Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param  Request                A null-terminated Unicode string in
                                 <ConfigRequest> format.
  @param  Progress               On   return, points to a character in the Request
                                 string. Points to the string's null terminator if
                                 request was successful. Points to the most recent
                                 '&' before the first failing name/value pair (or
                                 the beginning of the string if the failure is in
                                 the first name/value pair) if the request was not
                                 successful.
  @param  Results                A null-terminated Unicode string in
                                 <ConfigAltResp> format which has all values filled
                                 in for the names in the Request string. String to
                                 be allocated by the called function.

  @retval EFI_SUCCESS            The Results is filled with the requested values.
  @retval EFI_OUT_OF_RESOURCES   Not enough memory to store the results.
  @retval EFI_INVALID_PARAMETER  Request is illegal syntax, or unknown name.
  @retval EFI_NOT_FOUND          Routing data doesn't match any storage in this
                                 driver.

**/
EFI_STATUS
EFIAPI
WatchdogConfigExtractConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN CONST EFI_STRING                     Request,
  OUT      EFI_STRING                     *Progress,
  OUT      EFI_STRING                     *Results
  )
{
  EFI_STATUS                      Status;
  UINTN                           BufferSize;
  WATCHDOG_CONFIG_PRIVATE_DATA    *PrivateData;
  EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;
  EFI_STRING                      ConfigRequest;
  EFI_STRING                      ConfigRequestHdr;
  UINTN                           Size;
  BOOLEAN                         AllocatedRequest;

  if (Progress == NULL || Results == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Initialize the local variables.
  //
  ConfigRequestHdr  = NULL;
  ConfigRequest     = NULL;
  Size              = 0;
  *Progress         = Request;
  AllocatedRequest  = FALSE;

  if ((Request != NULL) && !HiiIsConfigHdrMatch (Request, &gWatchdogConfigFormSetGuid, WatchDogConfigVarstoreDataName)) {
    return EFI_NOT_FOUND;
  }

  PrivateData = WATCHDOG_CONFIG_PRIVATE_FROM_THIS (This);
  HiiConfigRouting = PrivateData->HiiConfigRouting;

  //
  // Get current setting from NVParam.
  //
  Status = WatchdogConfigNvParamGet (&PrivateData->Configuration);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Convert buffer data to <ConfigResp> by helper function BlockToConfig()
  //
  BufferSize = sizeof (WATCHDOG_CONFIG_VARSTORE_DATA);
  ConfigRequest = Request;
  if ((Request == NULL) || (StrStr (Request, L"OFFSET") == NULL)) {
    //
    // Request has no request element, construct full request string.
    // Allocate and fill a buffer large enough to hold the <ConfigHdr> template
    // followed by "&OFFSET=0&WIDTH=WWWWWWWWWWWWWWWW" followed by a Null-terminator
    //
    ConfigRequestHdr = HiiConstructConfigHdr (&gWatchdogConfigFormSetGuid, WatchDogConfigVarstoreDataName, PrivateData->DriverHandle);
    Size = (StrLen (ConfigRequestHdr) + 32 + 1) * sizeof (CHAR16);
    ConfigRequest = AllocateZeroPool (Size);
    ASSERT (ConfigRequest != NULL);
    if (ConfigRequest == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    AllocatedRequest = TRUE;
    UnicodeSPrint (ConfigRequest, Size, L"%s&OFFSET=0&WIDTH=%016LX", ConfigRequestHdr, (UINT64)BufferSize);
    FreePool (ConfigRequestHdr);
  }

  //
  // Convert buffer data to <ConfigResp> by helper function BlockToConfig()
  //
  Status = HiiConfigRouting->BlockToConfig (
                               HiiConfigRouting,
                               ConfigRequest,
                               (UINT8 *)&PrivateData->Configuration,
                               BufferSize,
                               Results,
                               Progress
                               );

  //
  // Free the allocated config request string.
  //
  if (AllocatedRequest) {
    FreePool (ConfigRequest);
    ConfigRequest = NULL;
  }

  //
  // Set Progress string to the original request string.
  //
  if (Request == NULL) {
    *Progress = NULL;
  } else if (StrStr (Request, L"OFFSET") == NULL) {
    *Progress = Request + StrLen (Request);
  }

  return Status;
}

/**
  This function processes the results of changes in configuration.

  @param  This                   Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param  Configuration          A null-terminated Unicode string in <ConfigResp>
                                 format.
  @param  Progress               A pointer to a string filled in with the offset of
                                 the most recent '&' before the first failing
                                 name/value pair (or the beginning of the string if
                                 the failure is in the first name/value pair) or
                                 the terminating NULL if all was successful.

  @retval EFI_SUCCESS            The Results is processed successfully.
  @retval EFI_INVALID_PARAMETER  Configuration is NULL.
  @retval EFI_NOT_FOUND          Routing data doesn't match any storage in this
                                 driver.

**/
EFI_STATUS
EFIAPI
WatchdogConfigRouteConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN CONST EFI_STRING                     Configuration,
  OUT      EFI_STRING                     *Progress
  )
{
  EFI_STATUS                      Status;
  UINTN                           BufferSize;
  WATCHDOG_CONFIG_PRIVATE_DATA    *PrivateData;
  EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;

  if (Configuration == NULL || Progress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData = WATCHDOG_CONFIG_PRIVATE_FROM_THIS (This);
  HiiConfigRouting = PrivateData->HiiConfigRouting;
  *Progress = Configuration;

  //
  // Check routing data in <ConfigHdr>.
  // Note: if only one Storage is used, then this checking could be skipped.
  //
  if (!HiiIsConfigHdrMatch (Configuration, &gWatchdogConfigFormSetGuid, WatchDogConfigVarstoreDataName)) {
    return EFI_NOT_FOUND;
  }

  //
  // Get configuration data from NVParam
  //
  Status = WatchdogConfigNvParamGet (&PrivateData->Configuration);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Convert <ConfigResp> to buffer data by helper function ConfigToBlock()
  //
  BufferSize = sizeof (WATCHDOG_CONFIG_VARSTORE_DATA);
  Status = HiiConfigRouting->ConfigToBlock (
                               HiiConfigRouting,
                               Configuration,
                               (UINT8 *)&PrivateData->Configuration,
                               &BufferSize,
                               Progress
                               );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Store configuration data back to NVParam
  //
  Status = WatchdogConfigNvParamSet (&PrivateData->Configuration);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

/**
  This function processes the results of changes in configuration.

  @param  This                   Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param  Action                 Specifies the type of action taken by the browser.
  @param  QuestionId             A unique value which is sent to the original
                                 exporting driver so that it can identify the type
                                 of data to expect.
  @param  Type                   The type of value for the question.
  @param  Value                  A pointer to the data being sent to the original
                                 exporting driver.
  @param  ActionRequest          On return, points to the action requested by the
                                 callback function.

  @retval  EFI_SUCCESS           The callback successfully handled the action.
  @retval  EFI_INVALID_PARAMETER The setup browser call this function with invalid parameters.

**/
EFI_STATUS
EFIAPI
WatchdogConfigCallback (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN       EFI_BROWSER_ACTION             Action,
  IN       EFI_QUESTION_ID                QuestionId,
  IN       UINT8                          Type,
  IN       EFI_IFR_TYPE_VALUE             *Value,
  OUT      EFI_BROWSER_ACTION_REQUEST     *ActionRequest
  )
{
  if (Action != EFI_BROWSER_ACTION_CHANGING) {
    //
    // Do nothing for other UEFI Action. Only do call back when data is changed.
    //
    return EFI_UNSUPPORTED;
  }
  if (((Value == NULL)
       && (Action != EFI_BROWSER_ACTION_FORM_OPEN)
       && (Action != EFI_BROWSER_ACTION_FORM_CLOSE))
      || (ActionRequest == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
WatchdogConfigInitialize (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS                      Status;
  EFI_HII_HANDLE                  HiiHandle;
  EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;

  //
  // Initialize driver private data
  //
  mPrivateData = AllocateZeroPool (sizeof (WATCHDOG_CONFIG_PRIVATE_DATA));
  if (mPrivateData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  mPrivateData->Signature = WATCHDOG_CONFIG_PRIVATE_SIGNATURE;

  mPrivateData->ConfigAccess.ExtractConfig = WatchdogConfigExtractConfig;
  mPrivateData->ConfigAccess.RouteConfig = WatchdogConfigRouteConfig;
  mPrivateData->ConfigAccess.Callback = WatchdogConfigCallback;

  //
  // Locate ConfigRouting protocol
  //
  Status = gBS->LocateProtocol (&gEfiHiiConfigRoutingProtocolGuid, NULL, (VOID **)&HiiConfigRouting);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  mPrivateData->HiiConfigRouting = HiiConfigRouting;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mDriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mWatchdogConfigHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &mPrivateData->ConfigAccess,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  mPrivateData->DriverHandle = mDriverHandle;

  //
  // Publish our HII data
  //
  HiiHandle = HiiAddPackages (
                &gWatchdogConfigFormSetGuid,
                mDriverHandle,
                WatchdogConfigDxeStrings,
                WatchdogConfigVfrBin,
                NULL
                );
  if (HiiHandle == NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           mDriverHandle,
           &gEfiDevicePathProtocolGuid,
           &mWatchdogConfigHiiVendorDevicePath,
           &gEfiHiiConfigAccessProtocolGuid,
           &mPrivateData->ConfigAccess,
           NULL
           );
    return EFI_OUT_OF_RESOURCES;
  }

  mPrivateData->HiiHandle = HiiHandle;

  return EFI_SUCCESS;
}
