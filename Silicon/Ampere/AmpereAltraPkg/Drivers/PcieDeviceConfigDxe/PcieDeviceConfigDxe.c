/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/MdeModuleHii.h>
#include <Guid/PcieDeviceConfigHii.h>
#include <IndustryStandard/Pci22.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/HiiConfigAccess.h>
#include <Protocol/PciIo.h>

#include "PcieDeviceConfigDxe.h"
#include "PcieHelper.h"

STATIC VOID      *mPciProtocolNotifyRegistration;
STATIC EFI_EVENT mPciProtocolNotifyEvent;

CHAR16        *mVariableName = VARSTORE_NAME;
VARSTORE_DATA *mVarStoreConfig;
PRIVATE_DATA  *mPrivateData;
PCIE_NODE     *mDeviceBuf[MAX_DEVICE] = {NULL};

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
    PCIE_DEVICE_CONFIG_FORMSET_GUID
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

VOID
FlushDeviceData (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_STATUS    Status;
  PCIE_NODE     *Node;
  UINT64        BufferSize;
  UINT8         Index;
  VARSTORE_DATA VarStoreConfig;

  // Get Variables
  BufferSize = sizeof (VARSTORE_DATA);
  Status = gRT->GetVariable (
                  mVariableName,
                  &gPcieDeviceConfigFormSetGuid,
                  NULL,
                  &BufferSize,
                  &VarStoreConfig
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to get variable status %r",
      __FUNCTION__,
      Status
      ));
    return;
  }

  // Iterate through the list, then write corresponding MPS MRR
  for (Index = 0; Index < MAX_DEVICE; Index++) {
    if (mDeviceBuf[Index] == NULL) {
      continue;
    }

    // Write MPS value
    WriteMps (mDeviceBuf[Index], VarStoreConfig.MPS[Index]);

    FOR_EACH (Node, mDeviceBuf[Index], Parent) {
      WriteMps (Node, VarStoreConfig.MPS[Index]);
    }

    FOR_EACH (Node, mDeviceBuf[Index], Brother) {
      WriteMps (Node, VarStoreConfig.MPS[Index]);
    }

    // Write MRR value
    // FIXME: No need to update MRR of parent node
    WriteMrr (mDeviceBuf[Index], VarStoreConfig.MRR[Index]);

    FOR_EACH (Node, mDeviceBuf[Index], Brother) {
      WriteMrr (Node, VarStoreConfig.MRR[Index]);
    }
  }

  gBS->CloseEvent (Event);
}

EFI_STATUS
UpdateDeviceForm (
  UINT8 Index
  )
{
  CHAR16 Str[MAX_STRING_SIZE];
  UINT8  MaxMps;

  VOID               *StartOpCodeHandle;
  EFI_IFR_GUID_LABEL *StartLabel;
  VOID               *EndOpCodeHandle;
  EFI_IFR_GUID_LABEL *EndLabel;
  VOID               *MpsOpCodeHandle;
  VOID               *MrrOpCodeHandle;
  PCIE_NODE          *Node;

  if (mDeviceBuf[Index] == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  MaxMps = mDeviceBuf[Index]->MaxMps;
  FOR_EACH (Node, mDeviceBuf[Index], Parent) {
    if (Node->MaxMps < MaxMps) {
      MaxMps = Node->MaxMps;
    }
  }

  UnicodeSPrint (
    Str,
    sizeof (Str),
    L"PCIe Device 0x%04x:0x%04x",
    mDeviceBuf[Index]->Vid,
    mDeviceBuf[Index]->Did
    );

  HiiSetString (
    mPrivateData->HiiHandle,
    STRING_TOKEN (STR_DEVICE_FORM),
    Str,
    NULL
    );

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
  StartLabel->Number       = DEVICE_LABEL_UPDATE;

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
  EndLabel->Number       = DEVICE_LABEL_END;

  // Create Option OpCode for MPS selection
  MpsOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (MpsOpCodeHandle != NULL);

  switch (MaxMps) {
  case 5:
    HiiCreateOneOfOptionOpCode (
      MpsOpCodeHandle,
      STRING_TOKEN (STR_4096),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      5
      );

  case 4:
    HiiCreateOneOfOptionOpCode (
      MpsOpCodeHandle,
      STRING_TOKEN (STR_2048),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      4
      );

  case 3:
    HiiCreateOneOfOptionOpCode (
      MpsOpCodeHandle,
      STRING_TOKEN (STR_1024),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      3
      );

  case 2:
    HiiCreateOneOfOptionOpCode (
      MpsOpCodeHandle,
      STRING_TOKEN (STR_512),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      2
      );

  case 1:
    HiiCreateOneOfOptionOpCode (
      MpsOpCodeHandle,
      STRING_TOKEN (STR_256),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      1
      );

  case 0:
    HiiCreateOneOfOptionOpCode (
      MpsOpCodeHandle,
      STRING_TOKEN (STR_128),
      0,
      EFI_IFR_NUMERIC_SIZE_1,
      0
      );
  }

  // Create MPS OneOf
  HiiCreateOneOfOpCode (
    StartOpCodeHandle,                // Container for dynamic created opcodes
    (MPS_ONE_OF_KEY + Index),         // Question ID (or call it "key")
    VARSTORE_ID,                      // VarStore ID
    Index,                            // Offset in Buffer Storage
    STRING_TOKEN (STR_PCIE_MPS),      // Question prompt text
    STRING_TOKEN (STR_PCIE_MPS_HELP), // Question help text
    EFI_IFR_FLAG_CALLBACK,            // Question flag
    EFI_IFR_NUMERIC_SIZE_1,           // Data type of Question Value
    MpsOpCodeHandle,                  // Option Opcode list
    NULL                              // Default Opcode is NULl
    );

  // Create Option OpCode for MRR selection
  MrrOpCodeHandle = HiiAllocateOpCodeHandle ();
  ASSERT (MrrOpCodeHandle != NULL);

  HiiCreateOneOfOptionOpCode (
    MrrOpCodeHandle,
    STRING_TOKEN (STR_4096),
    0,
    EFI_IFR_NUMERIC_SIZE_1,
    5
    );

  HiiCreateOneOfOptionOpCode (
    MrrOpCodeHandle,
    STRING_TOKEN (STR_2048),
    0,
    EFI_IFR_NUMERIC_SIZE_1,
    4
    );

  HiiCreateOneOfOptionOpCode (
    MrrOpCodeHandle,
    STRING_TOKEN (STR_1024),
    0,
    EFI_IFR_NUMERIC_SIZE_1,
    3
    );

  HiiCreateOneOfOptionOpCode (
    MrrOpCodeHandle,
    STRING_TOKEN (STR_512),
    0,
    EFI_IFR_NUMERIC_SIZE_1,
    2
    );

  HiiCreateOneOfOptionOpCode (
    MrrOpCodeHandle,
    STRING_TOKEN (STR_256),
    0,
    EFI_IFR_NUMERIC_SIZE_1,
    1
    );

  HiiCreateOneOfOptionOpCode (
    MrrOpCodeHandle,
    STRING_TOKEN (STR_128),
    0,
    EFI_IFR_NUMERIC_SIZE_1,
    0
    );

  // Create MRR OneOf
  HiiCreateOneOfOpCode (
    StartOpCodeHandle,                // Container for dynamic created opcodes
    (MRR_ONE_OF_KEY + Index),         // Question ID (or call it "key")
    VARSTORE_ID,                      // VarStore ID
    MAX_DEVICE + Index,               // Offset in Buffer Storage
    STRING_TOKEN (STR_PCIE_MRR),      // Question prompt text
    STRING_TOKEN (STR_PCIE_MRR_HELP), // Question help text
    EFI_IFR_FLAG_CALLBACK,            // Question flag
    EFI_IFR_NUMERIC_SIZE_1,           // Data type of Question Value
    MrrOpCodeHandle,                  // Option Opcode list
    NULL                              // Default Opcode is NULl
    );

  HiiUpdateForm (
    mPrivateData->HiiHandle,       // HII handle
    &gPcieDeviceConfigFormSetGuid, // Formset GUID
    DEVICE_FORM_ID,                // Form ID
    StartOpCodeHandle,             // Label for where to insert opcodes
    EndOpCodeHandle                // Insert data
    );

  HiiFreeOpCodeHandle (StartOpCodeHandle);
  HiiFreeOpCodeHandle (EndOpCodeHandle);

  return EFI_SUCCESS;
}

EFI_STATUS
StoreNodeInfo (
  PCIE_NODE *Node,
  UINT8     Index
  )
{
  EFI_STATUS           Status;
  STATIC VARSTORE_DATA NewVarStoreConfig;
  UINT8                Idx;
  UINT64               SlotInfo;
  UINTN                BufferSize;

  if (Node == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  SlotInfo = PCIE_ADD (Node->Vid, Node->Did, Node->Seg, Node->Bus, Node->Dev);

  NewVarStoreConfig.MPS[Index] = DEFAULT_MPS;
  NewVarStoreConfig.MRR[Index] = DEFAULT_MRR;
  NewVarStoreConfig.SlotInfo[Index] = SlotInfo;

  // Retrieve setting from previous variable
  for (Idx = 0; Idx < MAX_DEVICE; Idx++) {
    if (mVarStoreConfig->SlotInfo[Idx] == SlotInfo) {
      NewVarStoreConfig.MPS[Index] = mVarStoreConfig->MPS[Idx];
      NewVarStoreConfig.MRR[Index] = mVarStoreConfig->MRR[Idx];
      break;
    }
  }

  BufferSize = sizeof (NewVarStoreConfig);
  Status = gRT->SetVariable (
                  mVariableName,
                  &gPcieDeviceConfigFormSetGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  BufferSize,
                  &NewVarStoreConfig
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

VOID
OnPciIoProtocolNotify (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  EFI_PCI_IO_PROTOCOL *PciIo;
  EFI_STATUS          Status;
  EFI_HANDLE          HandleBuffer;
  PCI_TYPE00          Pci;

  UINTN BufferSize;
  UINTN PciBusNumber;
  UINTN PciDeviceNumber;
  UINTN PciFunctionNumber;
  UINTN PciSegment;

  UINT8  NextCapPtr;
  UINT16 TmpValue;

  PCIE_NODE        *Node;
  STATIC PCIE_NODE *LastNode;
  STATIC UINT8     Index;
  STATIC UINT8     LastBus;

  while (TRUE) {
    BufferSize = sizeof (EFI_HANDLE);
    Status = gBS->LocateHandle (
                    ByRegisterNotify,
                    NULL,
                    mPciProtocolNotifyRegistration,
                    &BufferSize,
                    &HandleBuffer
                    );
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = gBS->HandleProtocol (
                    HandleBuffer,
                    &gEfiPciIoProtocolGuid,
                    (VOID **)&PciIo
                    );

    // Get device bus location
    Status = PciIo->GetLocation (
                      PciIo,
                      &PciSegment,
                      &PciBusNumber,
                      &PciDeviceNumber,
                      &PciFunctionNumber
                      );
    if (EFI_ERROR (Status) ||
        ((PciBusNumber == 0) && (PciDeviceNumber == 0)))
    {
      // Filter out Host Bridge
      continue;
    }

    DEBUG ((
      DEBUG_INFO,
      ">> Dev 0x%04x:0x%02x:0x%02x:0x%02x\n",
      PciSegment,
      PciBusNumber,
      PciDeviceNumber,
      PciFunctionNumber
      ));

    // find PCIe Capabilities offset
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint8,
                          0x34,
                          1,
                          &NextCapPtr
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Pci.Read error line 0x%d\n", __LINE__));
      continue;
    }

    while (TRUE) {
      Status = PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint16,
                            NextCapPtr,
                            1,
                            &TmpValue
                            );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Pci.Read error line 0x%d\n", __LINE__));
        break;
      }
      if ((TmpValue & 0xFF) == PCIE_CAPABILITIES_ID) {
        break;
      }
      NextCapPtr = (TmpValue >> 8) & 0xFF;
      ASSERT (NextCapPtr < 512);
    }

    // Get Device's max MPS support
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint16,
                          NextCapPtr + PCIE_CAPABILITIES_REG,
                          1,
                          &TmpValue
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Pci.Read error line 0x%d\n", __LINE__));
      continue;
    }

    // Read device's VID:PID
    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint32,
                          0,
                          sizeof (Pci) / sizeof (UINT32),
                          &Pci
                          );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Pci.Read error line 0x%d\n", __LINE__));
      continue;
    }
    DEBUG ((
      DEBUG_INFO,
      "VendorId 0x%04x - DeviceId 0x%04x\n",
      Pci.Hdr.VendorId,
      Pci.Hdr.DeviceId
      ));

    Node = AllocateZeroPool (sizeof (*Node));
    Node->MaxMps = TmpValue & PCIE_MAX_PAYLOAD_MASK;
    Node->PcieCapOffset = NextCapPtr;
    Node->PciIo = PciIo;
    Node->Seg = PciSegment;
    Node->Bus = PciBusNumber;
    Node->Dev = PciDeviceNumber;
    Node->Fun = PciFunctionNumber;
    Node->Vid = Pci.Hdr.VendorId;
    Node->Did = Pci.Hdr.DeviceId;

    // Presume child devices were registered follow root port
    if (PciBusNumber != 0) {
      if (LastBus == 0) {
        Node->Parent = LastNode;
        mDeviceBuf[Index] = Node;
        StoreNodeInfo (Node, Index);

        Index++;
      } else if (PciBusNumber == LastBus) {
        LastNode->Brother = Node;
      } else {
        // Ignore devices don't stay under root port
        continue;
      }
    }

    LastBus = PciBusNumber;
    LastNode = Node;
  }
}

VOID
UpdateMainForm (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  UINTN              Index;
  EFI_STRING_ID      StrId;
  CHAR16             Str[MAX_STRING_SIZE];
  VOID               *StartOpCodeHandle;
  EFI_IFR_GUID_LABEL *StartLabel;
  VOID               *EndOpCodeHandle;
  EFI_IFR_GUID_LABEL *EndLabel;

  DEBUG ((DEBUG_INFO, "%a Entry ...\n", __FUNCTION__));

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
  StartLabel->Number       = MAIN_LABEL_UPDATE;

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
  EndLabel->Number       = MAIN_LABEL_END;

  for (Index = 0; Index < MAX_DEVICE; Index++) {
    if (mDeviceBuf[Index] == NULL) {
      break;
    }
    DEBUG ((DEBUG_INFO, ">> Add item %d\n", Index));

    // TODO: convert and store in SlotID ex:SystemSlot(seg, bus, dev)
    UnicodeSPrint (
      Str,
      sizeof (Str),
      L"PCIe Device 0x%04x:0x%04x - 0x%04x:0x%02x:0x%02x",
      mDeviceBuf[Index]->Vid,
      mDeviceBuf[Index]->Did,
      mDeviceBuf[Index]->Seg,
      mDeviceBuf[Index]->Bus,
      mDeviceBuf[Index]->Dev
      );

    StrId = HiiSetString (mPrivateData->HiiHandle, 0, Str, NULL);

    //
    // Create a Goto OpCode to device configuration
    //
    HiiCreateGotoOpCode (
      StartOpCodeHandle,                   // Container for dynamic created opcodes
      DEVICE_FORM_ID,                      // Target Form ID
      StrId,                               // Prompt text
      STRING_TOKEN (STR_DEVICE_GOTO_HELP), // Help text
      EFI_IFR_FLAG_CALLBACK,               // Question flag
      (DEVICE_KEY + Index)                 // Question ID
      );
  }

  HiiUpdateForm (
    mPrivateData->HiiHandle,        // HII handle
    &gPcieDeviceConfigFormSetGuid,  // Formset GUID
    MAIN_FORM_ID,                   // Form ID
    StartOpCodeHandle,              // Label for where to insert opcodes
    EndOpCodeHandle                 // Insert data
    );

  HiiFreeOpCodeHandle (StartOpCodeHandle);
  HiiFreeOpCodeHandle (EndOpCodeHandle);

  gBS->CloseEvent (Event);
}

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
  PRIVATE_DATA                    *PrivateData;
  EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;
  EFI_STRING                      ConfigRequest;
  EFI_STRING                      ConfigRequestHdr;
  UINTN                           Size;
  CHAR16                          *StrPointer;
  BOOLEAN                         AllocatedRequest;
  VARSTORE_DATA                   *VarStoreConfig;

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

  PrivateData = PRIVATE_DATA_FROM_THIS (This);
  HiiConfigRouting = PrivateData->HiiConfigRouting;
  VarStoreConfig = &PrivateData->VarStoreConfig;
  ASSERT (VarStoreConfig != NULL);

  //
  // Get Buffer Storage data from EFI variable.
  // Try to get the current setting from variable.
  //
  BufferSize = sizeof (VARSTORE_DATA);
  Status = gRT->GetVariable (
                  mVariableName,
                  &gPcieDeviceConfigFormSetGuid,
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
                         &gPcieDeviceConfigFormSetGuid,
                         mVariableName,
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
    if (!HiiIsConfigHdrMatch (Request, &gPcieDeviceConfigFormSetGuid, NULL)) {
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
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL *This,
  IN  CONST EFI_STRING                     Configuration,
  OUT EFI_STRING                           *Progress
  )
{
  EFI_STATUS                      Status;
  UINTN                           BufferSize;
  PRIVATE_DATA                    *PrivateData;
  EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;
  VARSTORE_DATA                   *VarStoreConfig;

  if (Configuration == NULL || Progress == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  PrivateData = PRIVATE_DATA_FROM_THIS (This);
  HiiConfigRouting = PrivateData->HiiConfigRouting;
  *Progress = Configuration;
  VarStoreConfig = &PrivateData->VarStoreConfig;
  ASSERT (VarStoreConfig != NULL);

  //
  // Check routing data in <ConfigHdr>.
  // Note: if only one Storage is used, then this checking could be skipped.
  //
  if (!HiiIsConfigHdrMatch (
         Configuration,
         &gPcieDeviceConfigFormSetGuid,
         NULL
         ))
  {
    return EFI_NOT_FOUND;
  }
  //
  // Get Buffer Storage data from EFI variable
  //
  BufferSize = sizeof (VARSTORE_DATA);
  Status = gRT->GetVariable (
                  mVariableName,
                  &gPcieDeviceConfigFormSetGuid,
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
  BufferSize = sizeof (VARSTORE_DATA);
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
                  mVariableName,
                  &gPcieDeviceConfigFormSetGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  sizeof (VARSTORE_DATA),
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
  @retval EFI_INVALID_PARAMETER  Configuration is NULL.
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
  EFI_STATUS Status;

  if (((Value == NULL) &&
       (Action != EFI_BROWSER_ACTION_FORM_OPEN) &&
       (Action != EFI_BROWSER_ACTION_FORM_CLOSE)) ||
      (ActionRequest == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  switch (Action) {
  case EFI_BROWSER_ACTION_CHANGING:
    if ((QuestionId >= DEVICE_KEY)
        & (QuestionId <= (DEVICE_KEY + MAX_DEVICE)))
    {
      Status = UpdateDeviceForm (QuestionId - DEVICE_KEY);
      if (EFI_ERROR (Status)) {
        return Status;
      }
    }
    break;

  case EFI_BROWSER_ACTION_DEFAULT_STANDARD:
  case EFI_BROWSER_ACTION_DEFAULT_MANUFACTURING:
    if ((QuestionId >= MPS_ONE_OF_KEY)
        & (QuestionId <= (MPS_ONE_OF_KEY + MAX_DEVICE)))
    {
      Value->u8 = DEFAULT_MPS;
    }

    if ((QuestionId >= MRR_ONE_OF_KEY)
        & (QuestionId <= (MRR_ONE_OF_KEY + MAX_DEVICE)))
    {
      Value->u8 = DEFAULT_MRR;
    }
    break;

  case EFI_BROWSER_ACTION_SUBMITTED:
    break;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PcieDeviceConfigEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_HANDLE                      DriverHandle;
  EFI_HII_CONFIG_ROUTING_PROTOCOL *HiiConfigRouting;
  EFI_HII_HANDLE                  HiiHandle;
  EFI_STATUS                      Status;
  EFI_EVENT                       PlatformUiEntryEvent;
  EFI_EVENT                       FlushDeviceEvent;
  UINTN                           BufferSize;

  DriverHandle = NULL;
  mPrivateData = AllocateZeroPool (sizeof (*mPrivateData));
  if (mPrivateData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  mPrivateData->Signature = PRIVATE_DATA_SIGNATURE;

  mPrivateData->ConfigAccess.ExtractConfig = ExtractConfig;
  mPrivateData->ConfigAccess.RouteConfig = RouteConfig;
  mPrivateData->ConfigAccess.Callback = DriverCallback;

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

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gEfiDevicePathProtocolGuid,
                  &mHiiVendorDevicePath,
                  &gEfiHiiConfigAccessProtocolGuid,
                  &mPrivateData->ConfigAccess,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mPrivateData->DriverHandle = DriverHandle;

  //
  // Publish our HII data
  //
  HiiHandle = HiiAddPackages (
                &gPcieDeviceConfigFormSetGuid,
                DriverHandle,
                PcieDeviceConfigDxeStrings,
                VfrBin,
                NULL
                );
  if (HiiHandle == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  mPrivateData->HiiHandle = HiiHandle;

  // Event to fixup screen
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  UpdateMainForm,
                  NULL,
                  &gPlatformManagerEntryEventGuid,
                  &PlatformUiEntryEvent
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Event to collect PciIo
  mPciProtocolNotifyEvent = EfiCreateProtocolNotifyEvent (
                              &gEfiPciIoProtocolGuid,
                              TPL_CALLBACK,
                              OnPciIoProtocolNotify,
                              NULL,
                              &mPciProtocolNotifyRegistration
                              );
  ASSERT (mPciProtocolNotifyEvent != NULL);

  // Event to flush device data
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  FlushDeviceData,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &FlushDeviceEvent
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Verify varstore
  mVarStoreConfig = &mPrivateData->VarStoreConfig;
  BufferSize = sizeof (VARSTORE_DATA);
  Status = gRT->GetVariable (
                  mVariableName,
                  &gPcieDeviceConfigFormSetGuid,
                  NULL,
                  &BufferSize,
                  mVarStoreConfig
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get varstore config status %r\n", Status));
  }

  return EFI_SUCCESS;
}
