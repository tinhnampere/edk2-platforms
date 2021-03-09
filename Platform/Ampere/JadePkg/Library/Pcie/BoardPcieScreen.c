/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Guid/PlatformInfoHobGuid.h>
#include <Library/AmpereCpuLib.h>
#include <Library/HobLib.h>
#include <Library/NVParamLib.h>
#include <Library/PcieBoardLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <NVParamDef.h>
#include <Pcie.h>
#include <PlatformInfoHob.h>

#include "BoardPcieScreen.h"

#ifndef BIT
#define BIT(nr) (1 << (nr))
#endif

#define MAX_STRING_SIZE     32

CHAR16   VariableName[]     = PCIE_VARSTORE_NAME;
EFI_GUID gPcieFormSetGuid   = PCIE_FORM_SET_GUID;

EFI_HANDLE               DriverHandle = NULL;
PCIE_SCREEN_PRIVATE_DATA *mPrivateData = NULL;

AC01_RC RCList[MAX_AC01_PCIE_ROOT_COMPLEX];

HII_VENDOR_DEVICE_PATH mHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    PCIE_FORM_SET_GUID
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

/**
  This function allows a caller to extract the current configuration for one
  or more named elements from the target driver.
  @param  This                   Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param  Request                A null-terminated Unicode string in
                                 <ConfigRequest> format.
  @param  Progress               On return, points to a character in the Request
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
ExtractConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN CONST EFI_STRING                     Request,
  OUT      EFI_STRING                     *Progress,
  OUT      EFI_STRING                     *Results
  )
{
  EFI_STATUS                      Status;
  UINTN                           BufferSize;
  PCIE_SCREEN_PRIVATE_DATA        *PrivateData;
  EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;
  EFI_STRING                      ConfigRequest;
  EFI_STRING                      ConfigRequestHdr;
  UINTN                           Size;
  CHAR16                          *StrPointer;
  BOOLEAN                         AllocatedRequest;
  PCIE_VARSTORE_DATA              *VarStoreConfig;

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

  PrivateData = PCIE_SCREEN_PRIVATE_FROM_THIS (This);
  HiiConfigRouting = PrivateData->HiiConfigRouting;
  VarStoreConfig = &PrivateData->VarStoreConfig;
  ASSERT (VarStoreConfig != NULL);

  //
  // Get Buffer Storage data from EFI variable.
  // Try to get the current setting from variable.
  //
  BufferSize = sizeof (PCIE_VARSTORE_DATA);
  Status = gRT->GetVariable (
                  VariableName,
                  &gPcieFormSetGuid,
                  NULL,
                  &BufferSize,
                  VarStoreConfig
                  );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  if (Request == NULL) {
    //
    // Request is set to NULL, construct full request string.
    //

    //
    // Allocate and fill a buffer large enough to hold the <ConfigHdr> template
    // followed by "&OFFSET=0&WIDTH=WWWWWWWWWWWWWWWW" followed by a
    // Null-terminator
    //
    ConfigRequestHdr = HiiConstructConfigHdr (
                         &gPcieFormSetGuid,
                         VariableName,
                         PrivateData->DriverHandle
                         );
    if (ConfigRequestHdr == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    Size = (StrLen (ConfigRequestHdr) + 32 + 1) * sizeof (CHAR16);
    ConfigRequest = AllocateZeroPool (Size);
    ASSERT (ConfigRequest != NULL);
    AllocatedRequest = TRUE;
    UnicodeSPrint (
      ConfigRequest,
      Size,
      L"%s&OFFSET=0&WIDTH=%016LX",
      ConfigRequestHdr,
      (UINT64)BufferSize
      );
    FreePool (ConfigRequestHdr);
    ConfigRequestHdr = NULL;
  } else {
    //
    // Check routing data in <ConfigHdr>.
    // Note: if only one Storage is used, then this checking could be skipped.
    //
    if (!HiiIsConfigHdrMatch (Request, &gPcieFormSetGuid, NULL)) {
      return EFI_NOT_FOUND;
    }

    //
    // Set Request to the unified request string.
    //
    ConfigRequest = Request;

    //
    // Check whether Request includes Request Element.
    //
    if (StrStr (Request, L"OFFSET") == NULL) {
      //
      // Check Request Element does exist in Request String
      //
      StrPointer = StrStr (Request, L"PATH");
      if (StrPointer == NULL) {
        return EFI_INVALID_PARAMETER;
      }
      if (StrStr (StrPointer, L"&") == NULL) {
        Size = (StrLen (Request) + 32 + 1) * sizeof (CHAR16);
        ConfigRequest    = AllocateZeroPool (Size);
        ASSERT (ConfigRequest != NULL);
        AllocatedRequest = TRUE;
        UnicodeSPrint (
          ConfigRequest,
          Size,
          L"%s&OFFSET=0&WIDTH=%016LX",
          Request,
          (UINT64)BufferSize
          );
      }
    }
  }

  //
  // Check if requesting Name/Value storage
  //
  if (StrStr (ConfigRequest, L"OFFSET") == NULL) {
    //
    // Don't have any Name/Value storage names
    //
    Status = EFI_SUCCESS;
  } else {
    //
    // Convert buffer data to <ConfigResp> by helper function BlockToConfig()
    //
    Status = HiiConfigRouting->BlockToConfig (
                                 HiiConfigRouting,
                                 ConfigRequest,
                                 (UINT8 *)VarStoreConfig,
                                 BufferSize,
                                 Results,
                                 Progress
                                 );
  }

  //
  // Free the allocated config request string.
  //
  if (AllocatedRequest) {
    FreePool (ConfigRequest);
  }

  if (ConfigRequestHdr != NULL) {
    FreePool (ConfigRequestHdr);
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
RouteConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN CONST EFI_STRING                     Configuration,
  OUT      EFI_STRING                     *Progress
  )
{
  EFI_STATUS                      Status;
  UINTN                           BufferSize;
  PCIE_SCREEN_PRIVATE_DATA        *PrivateData;
  EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;
  PCIE_VARSTORE_DATA              *VarStoreConfig;

  if (Configuration == NULL || Progress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData = PCIE_SCREEN_PRIVATE_FROM_THIS (This);
  HiiConfigRouting = PrivateData->HiiConfigRouting;
  *Progress = Configuration;
  VarStoreConfig = &PrivateData->VarStoreConfig;
  ASSERT (VarStoreConfig != NULL);

  //
  // Check routing data in <ConfigHdr>.
  // Note: if only one Storage is used, then this checking could be skipped.
  //
  if (!HiiIsConfigHdrMatch (Configuration, &gPcieFormSetGuid, NULL)) {
    return EFI_NOT_FOUND;
  }

  //
  // Get Buffer Storage data from EFI variable
  //
  BufferSize = sizeof (PCIE_VARSTORE_DATA);
  Status = gRT->GetVariable (
                  VariableName,
                  &gPcieFormSetGuid,
                  NULL,
                  &BufferSize,
                  VarStoreConfig
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Check if configuring Name/Value storage
  //
  if (StrStr (Configuration, L"OFFSET") == NULL) {
    //
    // Don't have any Name/Value storage names
    //
    return EFI_SUCCESS;
  }

  //
  // Convert <ConfigResp> to buffer data by helper function ConfigToBlock()
  //
  BufferSize = sizeof (PCIE_VARSTORE_DATA);
  Status = HiiConfigRouting->ConfigToBlock (
                               HiiConfigRouting,
                               Configuration,
                               (UINT8 *)VarStoreConfig,
                               &BufferSize,
                               Progress
                               );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Store Buffer Storage back to variable
  //
  Status = gRT->SetVariable (
                  VariableName,
                  &gPcieFormSetGuid,
                  EFI_VARIABLE_NON_VOLATILE |
                  EFI_VARIABLE_BOOTSERVICE_ACCESS |
                  EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof (PCIE_VARSTORE_DATA),
                  VarStoreConfig
                  );

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
  @retval EFI_SUCCESS            The callback successfully handled the action.
  @retval EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the
                                 variable and its data.
  @retval EFI_DEVICE_ERROR       The variable could not be saved.
  @retval EFI_UNSUPPORTED        The specified Action is not supported by the
                                 callback.
**/
EFI_STATUS
EFIAPI
DriverCallback (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN       EFI_BROWSER_ACTION             Action,
  IN       EFI_QUESTION_ID                QuestionId,
  IN       UINT8                          Type,
  IN       EFI_IFR_TYPE_VALUE             *Value,
  OUT      EFI_BROWSER_ACTION_REQUEST     *ActionRequest
  )
{
  PCIE_VARSTORE_DATA       *VarStoreConfig;
  PCIE_SCREEN_PRIVATE_DATA *PrivateData;
  EFI_STATUS               Status;

  if (((Value == NULL) &&
       (Action != EFI_BROWSER_ACTION_FORM_OPEN) &&
       (Action != EFI_BROWSER_ACTION_FORM_CLOSE)) ||
      (ActionRequest == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData = PCIE_SCREEN_PRIVATE_FROM_THIS (This);
  VarStoreConfig = &PrivateData->VarStoreConfig;
  ASSERT (VarStoreConfig != NULL);

  Status = EFI_SUCCESS;

  switch (Action) {
  case EFI_BROWSER_ACTION_FORM_OPEN:
    break;

  case EFI_BROWSER_ACTION_FORM_CLOSE:
    break;

  case EFI_BROWSER_ACTION_DEFAULT_STANDARD:
  case EFI_BROWSER_ACTION_DEFAULT_MANUFACTURING:
    if (QuestionId == 0x9000) {
      /* SMMU PMU */
      Value->u32 = 0; /* default disable */
      break;
    }

    switch ((QuestionId - 0x8002) % MAX_EDITABLE_ELEMENTS) {
    case 0:
      Value->u8 = PcieRCActiveDefaultSetting ((QuestionId - 0x8002) / MAX_EDITABLE_ELEMENTS, PrivateData);
      break;

    case 1:
      Value->u8 = PcieRCDevMapLoDefaultSetting ((QuestionId - 0x8002) / MAX_EDITABLE_ELEMENTS, PrivateData);
      break;

    case 2:
      Value->u8 = PcieRCDevMapHiDefaultSetting ((QuestionId - 0x8002) / MAX_EDITABLE_ELEMENTS, PrivateData);
      break;
    }
    break;

  case EFI_BROWSER_ACTION_RETRIEVE:
  case EFI_BROWSER_ACTION_CHANGING:
  case EFI_BROWSER_ACTION_SUBMITTED:
    break;

  default:
    Status = EFI_UNSUPPORTED;
    break;
  }

  return Status;
}

EFI_STATUS
PcieScreenUnload (
  IN EFI_HANDLE ImageHandle
  )
{
  ASSERT (mPrivateData != NULL);

  if (DriverHandle != NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           DriverHandle,
           &gEfiDevicePathProtocolGuid,
           &mHiiVendorDevicePath,
           &gEfiHiiConfigAccessProtocolGuid,
           &mPrivateData->ConfigAccess,
           NULL
           );
    DriverHandle = NULL;
  }

  if (mPrivateData->HiiHandle != NULL) {
    HiiRemovePackages (mPrivateData->HiiHandle);
  }

  FreePool (mPrivateData);
  mPrivateData = NULL;

  return EFI_SUCCESS;
}

/**
  This function return default settings for Dev Map LO.
  @param  RC                     Root Complex ID.
  @param  PrivateData            Private data.

  @retval Default dev settings.
**/
UINT8
PcieRCDevMapLoDefaultSetting (
  IN UINTN                    RCIndex,
  IN PCIE_SCREEN_PRIVATE_DATA *PrivateData
  )
{
  AC01_RC *RC = &RCList[RCIndex];

  return RC->DefaultDevMapLo;
}

/**
  This function return default settings for Dev Map HI.
  @param  RC                     Root Complex ID.
  @param  PrivateData            Private data.

  @retval Default dev settings.
**/
UINT8
PcieRCDevMapHiDefaultSetting (
  IN UINTN                    RCIndex,
  IN PCIE_SCREEN_PRIVATE_DATA *PrivateData
  )
{
  AC01_RC *RC = &RCList[RCIndex];

  return RC->DefaultDevMapHi;
}

BOOLEAN
PcieRCActiveDefaultSetting (
  IN UINTN                    RCIndex,
  IN PCIE_SCREEN_PRIVATE_DATA *PrivateData
  )
{
  PlatformInfoHob_V2 *PlatformHob;
  VOID               *Hob;
  UINT32             Efuse;

  // FIXME: Disable Root Complex 6 (USB and VGA) as default
  if (RCIndex == 6) {
    return FALSE;
  }

  Hob = GetFirstGuidHob (&gPlatformHobV2Guid);
  if (Hob != NULL) {
    PlatformHob = (PlatformInfoHob_V2 *)GET_GUID_HOB_DATA (Hob);
    Efuse = PlatformHob->RcDisableMask[0] | (PlatformHob->RcDisableMask[1] << RCS_PER_SOCKET);
    return (!(Efuse & BIT (RCIndex))) ? TRUE : FALSE;
  }

  return FALSE;
}

/**
  This function sets up the first elements of the form.
  @param  RC                     Root Complex ID.
  @param  PrivateData            Private data.
  @retval EFI_SUCCESS            The form is set up successfully.
**/
EFI_STATUS
PcieRCScreenSetup (
  IN UINTN                    RCIndex,
  IN PCIE_SCREEN_PRIVATE_DATA *PrivateData
  )
{
  VOID               *StartOpCodeHandle;
  EFI_IFR_GUID_LABEL *StartLabel;
  VOID               *EndOpCodeHandle;
  EFI_IFR_GUID_LABEL *EndLabel;
  VOID               *OptionsOpCodeHandle;
  VOID               *OptionsHiOpCodeHandle;
  CHAR16             Str[MAX_STRING_SIZE];
  UINT16             DisabledStatusVarOffset;
  UINT16             BifurLoVarOffset;
  UINT16             BifurHiVarOffset;
  UINT8              QuestionFlags, QuestionFlagsSubItem;
  AC01_RC            *RC;

  RC = &RCList[RCIndex];

  // Initialize the container for dynamic opcodes
  StartOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (StartOpCodeHandle != NULL);
  EndOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (EndOpCodeHandle != NULL);

  // Create Hii Extend Label OpCode as the start opcode
  StartLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                       StartOpCodeHandle,
                                       &gEfiIfrTianoGuid,
                                       NULL,
                                       sizeof (EFI_IFR_GUID_LABEL)
                                       );
  StartLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  StartLabel->Number       = LABEL_RC0_UPDATE + 2 * RCIndex;

  // Create Hii Extend Label OpCode as the end opcode
  EndLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                     EndOpCodeHandle,
                                     &gEfiIfrTianoGuid,
                                     NULL,
                                     sizeof (EFI_IFR_GUID_LABEL)
                                     );
  EndLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  EndLabel->Number       = LABEL_RC0_END + 2 * RCIndex;

  // Create textbox to tell socket
  HiiCreateTextOpCode (
    StartOpCodeHandle,
    STRING_TOKEN (STR_PCIE_SOCKET),
    STRING_TOKEN (STR_PCIE_SOCKET_HELP),
    HiiSetString (
      PrivateData->HiiHandle,
      0,
      (RC->Socket) ? L"1" : L"0",
      NULL
      )
    );

  // Create textbox to tell Root Complex type
  HiiCreateTextOpCode (
    StartOpCodeHandle,
    STRING_TOKEN (STR_PCIE_RC_TYPE),
    STRING_TOKEN (STR_PCIE_RC_TYPE_HELP),
    HiiSetString (
      PrivateData->HiiHandle,
      0,
      (RC->Type == RCA) ? L"Root Complex Type-A" : L"Root Complex Type-B",
      NULL
      )
    );

  UnicodeSPrint (Str, sizeof (Str), L"Root Complex #%2d", RCIndex);

  DisabledStatusVarOffset = (UINT16)PCIE_RC0_STATUS_OFFSET +
                            sizeof (BOOLEAN) * RCIndex;
  BifurLoVarOffset = (UINT16)PCIE_RC0_BIFUR_LO_OFFSET +
                     sizeof (UINT8) * RCIndex;
  BifurHiVarOffset = (UINT16)PCIE_RC0_BIFUR_HI_OFFSET +
                     sizeof (UINT8) * RCIndex;

  QuestionFlags = EFI_IFR_FLAG_RESET_REQUIRED | EFI_IFR_FLAG_CALLBACK;
  if (IsEmptyRC (RC)
      || (GetNumberActiveSockets () == 1 && RC->Socket == 1))
  {
    //
    // Do not allow changing if none of Root Port underneath enabled
    // or slave Root Complex on 1P system.
    //
    QuestionFlags |= EFI_IFR_FLAG_READ_ONLY;
  }
  // Create the RC disabled checkbox
  HiiCreateCheckBoxOpCode (
    StartOpCodeHandle,                        // Container for dynamic created opcodes
    0x8002 + MAX_EDITABLE_ELEMENTS * RCIndex, // QuestionId (or "key")
    PCIE_VARSTORE_ID,                         // VarStoreId
    DisabledStatusVarOffset,                  // VarOffset in Buffer Storage
    HiiSetString (
      PrivateData->HiiHandle,
      0,
      Str,
      NULL
      ),                                       // Prompt
    STRING_TOKEN (STR_PCIE_RC_STATUS_HELP),    // Help
    QuestionFlags,                             // QuestionFlags
    0,                                         // CheckBoxFlags
    NULL                                       // DefaultsOpCodeHandle
    );

  if (RC->Type == RCA) {
    // Create Option OpCode to display bifurcation for RCA
    OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
    ASSERT (OptionsOpCodeHandle != NULL);

    HiiCreateOneOfOptionOpCode (
      OptionsOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE0),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      0 // Devmap=0
      );


    if (RC->DefaultDevMapLo != 0) {
      QuestionFlags |= EFI_IFR_FLAG_READ_ONLY;
    }

    HiiCreateOneOfOptionOpCode (
      OptionsOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE1),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      1 // Devmap=1
      );

    HiiCreateOneOfOptionOpCode (
      OptionsOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE2),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      2 // Devmap=2
      );

    HiiCreateOneOfOptionOpCode (
      OptionsOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE3),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      3 // Devmap=3
      );

    HiiCreateOneOfOpCode (
      StartOpCodeHandle,                        // Container for dynamic created opcodes
      0x8003 + MAX_EDITABLE_ELEMENTS * RCIndex, // Question ID (or call it "key")
      PCIE_VARSTORE_ID,                         // VarStore ID
      BifurLoVarOffset,                         // Offset in Buffer Storage
      STRING_TOKEN (STR_PCIE_RCA_BIFUR),        // Question prompt text
      STRING_TOKEN (STR_PCIE_RCA_BIFUR_HELP),   // Question help text
      QuestionFlags,                            // Question flag
      EFI_IFR_NUMERIC_SIZE_1,                   // Data type of Question Value
      OptionsOpCodeHandle,                      // Option Opcode list
      NULL                                      // Default Opcode is NULl
      );
  } else {
    // Create Option OpCode to display bifurcation for RCB-Lo
    OptionsOpCodeHandle = HiiAllocateOpCodeHandle ();
    ASSERT (OptionsOpCodeHandle != NULL);

    HiiCreateOneOfOptionOpCode (
      OptionsOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE4),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      0 // Devmap=0
      );

    QuestionFlagsSubItem = QuestionFlags;
    if (RC->DefaultDevMapLo != 0) {
      QuestionFlagsSubItem |= EFI_IFR_FLAG_READ_ONLY;
    }

    HiiCreateOneOfOptionOpCode (
      OptionsOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE5),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      1 // Devmap=1
      );

    HiiCreateOneOfOptionOpCode (
      OptionsOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE6),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      2 // Devmap=2
      );

    HiiCreateOneOfOptionOpCode (
      OptionsOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE7),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      3 // Devmap=3
      );

    HiiCreateOneOfOpCode (
      StartOpCodeHandle,                         // Container for dynamic created opcodes
      0x8003 + MAX_EDITABLE_ELEMENTS * RCIndex,  // Question ID (or call it "key")
      PCIE_VARSTORE_ID,                          // VarStore ID
      BifurLoVarOffset,                          // Offset in Buffer Storage
      STRING_TOKEN (STR_PCIE_RCB_LO_BIFUR),      // Question prompt text
      STRING_TOKEN (STR_PCIE_RCB_LO_BIFUR_HELP), // Question help text
      QuestionFlagsSubItem,                      // Question flag
      EFI_IFR_NUMERIC_SIZE_1,                    // Data type of Question Value
      OptionsOpCodeHandle,                       // Option Opcode list
      NULL                                       // Default Opcode is NULl
      );

    // Create Option OpCode to display bifurcation for RCB-Hi
    OptionsHiOpCodeHandle = HiiAllocateOpCodeHandle ();
    ASSERT (OptionsHiOpCodeHandle != NULL);

    QuestionFlagsSubItem = QuestionFlags;
    if (RC->DefaultDevMapHi != 0) {
      QuestionFlagsSubItem |= EFI_IFR_FLAG_READ_ONLY;
    }

    HiiCreateOneOfOptionOpCode (
      OptionsHiOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE4),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      0 // Devmap=0
      );

    HiiCreateOneOfOptionOpCode (
      OptionsHiOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE5),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      1 // Devmap=1
      );

    HiiCreateOneOfOptionOpCode (
      OptionsHiOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE6),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      2 // Devmap=2
      );

    HiiCreateOneOfOptionOpCode (
      OptionsHiOpCodeHandle,
      STRING_TOKEN (STR_PCIE_BIFUR_SELECT_VALUE7),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      3 // Devmap=3
      );

    HiiCreateOneOfOpCode (
      StartOpCodeHandle,                         // Container for dynamic created opcodes
      0x8004 + MAX_EDITABLE_ELEMENTS * RCIndex,  // Question ID (or call it "key")
      PCIE_VARSTORE_ID,                          // VarStore ID
      BifurHiVarOffset,                          // Offset in Buffer Storage
      STRING_TOKEN (STR_PCIE_RCB_HI_BIFUR),      // Question prompt text
      STRING_TOKEN (STR_PCIE_RCB_HI_BIFUR_HELP), // Question help text
      QuestionFlagsSubItem,                      // Question flag
      EFI_IFR_NUMERIC_SIZE_1,                    // Data type of Question Value
      OptionsHiOpCodeHandle,                     // Option Opcode list
      NULL                                       // Default Opcode is NULl
      );
  }

  HiiUpdateForm (
    PrivateData->HiiHandle,     // HII handle
    &gPcieFormSetGuid,          // Formset GUID
    PCIE_RC0_FORM_ID + RCIndex, // Form ID
    StartOpCodeHandle,          // Label for where to insert opcodes
    EndOpCodeHandle             // Insert data
    );

  HiiFreeOpCodeHandle (StartOpCodeHandle);
  HiiFreeOpCodeHandle (EndOpCodeHandle);

  return EFI_SUCCESS;
}

/**
  This function sets up the first elements of the form.
  @param  PrivateData            Private data.
  @retval EFI_SUCCESS            The form is set up successfully.
**/
EFI_STATUS
PcieMainScreenSetup (
  IN PCIE_SCREEN_PRIVATE_DATA *PrivateData
  )
{
  VOID                 *StartOpCodeHandle;
  EFI_IFR_GUID_LABEL   *StartLabel;
  VOID                 *EndOpCodeHandle;
  EFI_IFR_GUID_LABEL   *EndLabel;
  CHAR16               Str[MAX_STRING_SIZE];
  UINTN                RC;
  PCIE_SETUP_GOTO_DATA *GotoItem = NULL;
  EFI_QUESTION_ID      GotoId;

  // Initialize the container for dynamic opcodes
  StartOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (StartOpCodeHandle != NULL);
  EndOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (EndOpCodeHandle != NULL);

  // Create Hii Extend Label OpCode as the start opcode
  StartLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                       StartOpCodeHandle,
                                       &gEfiIfrTianoGuid,
                                       NULL,
                                       sizeof (EFI_IFR_GUID_LABEL)
                                       );
  StartLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  StartLabel->Number       = LABEL_UPDATE;

  // Create Hii Extend Label OpCode as the end opcode
  EndLabel = (EFI_IFR_GUID_LABEL *)HiiCreateGuidOpCode (
                                     EndOpCodeHandle,
                                     &gEfiIfrTianoGuid,
                                     NULL,
                                     sizeof (EFI_IFR_GUID_LABEL)
                                     );
  EndLabel->ExtendOpCode = EFI_IFR_EXTEND_OP_LABEL;
  EndLabel->Number       = LABEL_END;

  HiiCreateCheckBoxOpCode (
    StartOpCodeHandle,                       // Container for dynamic created opcodes
    0x9000,                                  // Question ID
    PCIE_VARSTORE_ID,                        // VarStore ID
    (UINT16)PCIE_SMMU_PMU_OFFSET,            // Offset in Buffer Storage
    STRING_TOKEN (STR_PCIE_SMMU_PMU_PROMPT), // Question prompt text
    STRING_TOKEN (STR_PCIE_SMMU_PMU_HELP),   // Question help text
    EFI_IFR_FLAG_CALLBACK | EFI_IFR_FLAG_RESET_REQUIRED,
    0,
    NULL
    );

  //
  // Create the a seperated line
  //
  HiiCreateTextOpCode (
    StartOpCodeHandle,
    STRING_TOKEN (STR_PCIE_FORM_SEPERATE_LINE),
    STRING_TOKEN (STR_PCIE_FORM_SEPERATE_LINE),
    STRING_TOKEN (STR_PCIE_FORM_SEPERATE_LINE)
    );

  // Create Goto form for each RC
  for (RC = 0; RC < MAX_AC01_PCIE_ROOT_COMPLEX; RC++) {

    GotoItem = AllocateZeroPool (sizeof (PCIE_SETUP_GOTO_DATA));
    if (GotoItem == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    GotoItem->PciDevIdx = RC;

    GotoId = PCIE_GOTO_ID_BASE + (UINT16)RC;

    // Update HII string
    UnicodeSPrint (Str, sizeof (Str), L"Root Complex #%2d", RC);
    GotoItem->GotoStringId = HiiSetString (
                               PrivateData->HiiHandle,
                               0,
                               Str,
                               NULL
                               );
    GotoItem->GotoHelpStringId = STRING_TOKEN (STR_PCIE_GOTO_HELP);
    GotoItem->ShowItem = TRUE;

    // Add goto control
    HiiCreateGotoOpCode (
      StartOpCodeHandle,
      PCIE_RC0_FORM_ID + RC,
      GotoItem->GotoStringId,
      GotoItem->GotoHelpStringId,
      EFI_IFR_FLAG_CALLBACK,
      GotoId
      );
  }

  HiiUpdateForm (
    PrivateData->HiiHandle,  // HII handle
    &gPcieFormSetGuid,       // Formset GUID
    PCIE_FORM_ID,            // Form ID
    StartOpCodeHandle,       // Label for where to insert opcodes
    EndOpCodeHandle          // Insert data
    );

  HiiFreeOpCodeHandle (StartOpCodeHandle);
  HiiFreeOpCodeHandle (EndOpCodeHandle);

  return EFI_SUCCESS;
}

EFI_STATUS
PcieBoardScreenInitialize (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN AC01_RC          *NewRCList
  )
{
  EFI_STATUS                          Status;
  EFI_HII_HANDLE                      HiiHandle;
  EFI_HII_DATABASE_PROTOCOL           *HiiDatabase;
  EFI_HII_STRING_PROTOCOL             *HiiString;
  EFI_HII_CONFIG_ROUTING_PROTOCOL     *HiiConfigRouting;
  EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL *HiiKeywordHandler;
  UINTN                               BufferSize;
  BOOLEAN                             IsUpdated;
  PCIE_VARSTORE_DATA                  *VarStoreConfig;
  UINT8                               RCIndex;
  AC01_RC                             *RC;

  //
  // Initialize driver private data
  //
  mPrivateData = AllocateZeroPool (sizeof (PCIE_SCREEN_PRIVATE_DATA));
  if (mPrivateData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  mPrivateData->Signature = PCIE_SCREEN_PRIVATE_DATA_SIGNATURE;

  mPrivateData->ConfigAccess.ExtractConfig = ExtractConfig;
  mPrivateData->ConfigAccess.RouteConfig = RouteConfig;
  mPrivateData->ConfigAccess.Callback = DriverCallback;

  //
  // Locate Hii Database protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiHiiDatabaseProtocolGuid,
                  NULL,
                  (VOID **)&HiiDatabase
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  mPrivateData->HiiDatabase = HiiDatabase;

  //
  // Locate HiiString protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiHiiStringProtocolGuid,
                  NULL,
                  (VOID **)&HiiString
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  mPrivateData->HiiString = HiiString;

  //
  // Locate ConfigRouting protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiHiiConfigRoutingProtocolGuid,
                  NULL,
                  (VOID **)&HiiConfigRouting
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  mPrivateData->HiiConfigRouting = HiiConfigRouting;

  //
  // Locate keyword handler protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiConfigKeywordHandlerProtocolGuid,
                  NULL,
                  (VOID **)&HiiKeywordHandler
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  mPrivateData->HiiKeywordHandler = HiiKeywordHandler;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &mPrivateData->ConfigAccess,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  mPrivateData->DriverHandle = DriverHandle;

  //
  // Publish our HII data
  //
  HiiHandle = HiiAddPackages (
                &gPcieFormSetGuid,
                DriverHandle,
                PcieBoardLibStrings,
                VfrBin,
                NULL
                );
  if (HiiHandle == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  mPrivateData->HiiHandle = HiiHandle;

  // Make a shadow copy all Root Complexes' properties
  CopyMem ((VOID *)RCList, (VOID *)NewRCList, sizeof (RCList));

  //
  // Initialize efi varstore configuration data
  //
  VarStoreConfig = &mPrivateData->VarStoreConfig;
  ZeroMem (VarStoreConfig, sizeof (PCIE_VARSTORE_DATA));

  // Get Buffer Storage data from EFI variable
  BufferSize = sizeof (PCIE_VARSTORE_DATA);
  Status = gRT->GetVariable (
                  VariableName,
                  &gPcieFormSetGuid,
                  NULL,
                  &BufferSize,
                  VarStoreConfig
                  );

  IsUpdated = FALSE;

  if (EFI_ERROR (Status)) {
    VarStoreConfig->SmmuPmu = 0; /* Disable by default */
    IsUpdated = TRUE;
  }
  // Update board settings to menu
  for (RCIndex = 0; RCIndex < MAX_AC01_PCIE_ROOT_COMPLEX; RCIndex++) {
    RC = &RCList[RCIndex];

    if (EFI_ERROR (Status)) {
      VarStoreConfig->RCBifurLo[RCIndex] = RC->DevMapLo;
      VarStoreConfig->RCBifurHi[RCIndex] = RC->DevMapHi;
      VarStoreConfig->RCStatus[RCIndex] = RC->Active;
      IsUpdated = TRUE;
    }
    // FIXME: Disable Root Complex 6 (USB and VGA) as default
    if (EFI_ERROR (Status) && RCIndex == 6) {
      VarStoreConfig->RCStatus[RCIndex] = 0;
    }
  }

  if (IsUpdated) {
    // Update Buffer Storage
    Status = gRT->SetVariable (
                    VariableName,
                    &gPcieFormSetGuid,
                    EFI_VARIABLE_NON_VOLATILE |
                    EFI_VARIABLE_BOOTSERVICE_ACCESS |
                    EFI_VARIABLE_RUNTIME_ACCESS,
                    sizeof (PCIE_VARSTORE_DATA),
                    VarStoreConfig
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }
  Status = PcieMainScreenSetup (mPrivateData);
  ASSERT_EFI_ERROR (Status);

  for (RCIndex = 0; RCIndex < MAX_AC01_PCIE_ROOT_COMPLEX; RCIndex++) {
    Status = PcieRCScreenSetup (RCIndex, mPrivateData);
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}
