/** @file
  PCI Host Bridge Library instance for Ampere Altra-based platforms.

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Guid/EventGroup.h>
#include <IndustryStandard/Acpi.h>
#include <Library/Ac01PcieLib.h>
#include <Library/AcpiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>
#include <Protocol/PciRootBridgeIo.h>

#include <Ac01PcieCommon.h>

GLOBAL_REMOVE_IF_UNREFERENCED
STATIC CHAR16 CONST * CONST mPciHostBridgeLibAcpiAddressSpaceTypeStr[] = {
  L"Mem", L"I/O", L"Bus"
};

#pragma pack(1)
typedef struct {
  ACPI_HID_DEVICE_PATH     AcpiDevicePath;
  EFI_DEVICE_PATH_PROTOCOL EndDevicePath;
} EFI_PCI_ROOT_BRIDGE_DEVICE_PATH;
#pragma pack ()

STATIC EFI_PCI_ROOT_BRIDGE_DEVICE_PATH mEfiPciRootBridgeDevicePath = {
  {
    {
      ACPI_DEVICE_PATH,
      ACPI_DP,
      {
        (UINT8)(sizeof (ACPI_HID_DEVICE_PATH)),
        (UINT8)((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
      }
    },
    EISA_PNP_ID (0x0A08), // PCIe
    0
  }, {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      END_DEVICE_PATH_LENGTH,
      0
    }
  }
};

STATIC PCI_ROOT_BRIDGE mRootBridgeTemplate = {
  0,                                              // Segment
  0,                                              // Supports
  0,                                              // Attributes
  TRUE,                                           // DmaAbove4G
  FALSE,                                          // NoExtendedConfigSpace
  FALSE,                                          // ResourceAssigned
  EFI_PCI_HOST_BRIDGE_MEM64_DECODE,
  {
    // Bus
    0,
    0
  }, {
    // Io
    0,
    0,
    0
  }, {
    // Mem
    MAX_UINT64,
    0,
    0
  }, {
    // MemAbove4G
    MAX_UINT64,
    0,
    0
  }, {
    // PMem
    MAX_UINT64,
    0,
    0
  }, {
    // PMemAbove4G
    MAX_UINT64,
    0,
    0
  },
  (EFI_DEVICE_PATH_PROTOCOL *)&mEfiPciRootBridgeDevicePath
};

UINT8           mRootBridgeCount = 0;
PCI_ROOT_BRIDGE *mRootBridges = NULL;

EFI_STATUS
UpdateStatusMethodObject (
  EFI_ACPI_SDT_PROTOCOL   *AcpiSdtProtocol,
  EFI_ACPI_HANDLE         TableHandle,
  CHAR8                   *AsciiObjectPath,
  CHAR8                   ReturnValue
  )
{
  EFI_STATUS            Status;
  EFI_ACPI_HANDLE       ObjectHandle;
  EFI_ACPI_DATA_TYPE    DataType;
  CHAR8                 *Buffer;
  UINTN                 DataSize;

  Status = AcpiSdtProtocol->FindPath (TableHandle, AsciiObjectPath, &ObjectHandle);
  if (EFI_ERROR (Status) || ObjectHandle == NULL) {
    return EFI_SUCCESS;
  }
  ASSERT (ObjectHandle != NULL);

  Status = AcpiSdtProtocol->GetOption (ObjectHandle, 2, &DataType, (VOID *)&Buffer, &DataSize);
  if (!EFI_ERROR (Status) && Buffer[2] == AML_BYTE_PREFIX) {
    //
    // Only patch when the initial value is byte object.
    //
    Buffer[3] = ReturnValue;
  }

  AcpiSdtProtocol->Close (ObjectHandle);
  return Status;
}

/**
  This function will be called when ReadyToBoot event is signaled.

  @param Event   signaled event
  @param Context calling context

  @retval VOID
**/
VOID
EFIAPI
PciHostBridgeReadyToBootEvent (
  EFI_EVENT Event,
  VOID      *Context
  )
{
  EFI_STATUS                    Status;
  EFI_ACPI_SDT_PROTOCOL         *AcpiSdtProtocol;
  EFI_ACPI_DESCRIPTION_HEADER   *Table;
  UINTN                         TableKey;
  UINTN                         TableIndex;
  EFI_ACPI_HANDLE               TableHandle;
  CHAR8                         NodePath[256];
  UINTN                         Count;
  UINTN                         Idx1;
  UINTN                         Idx2;

  Count = 0;

  Status = gBS->LocateProtocol (
                  &gEfiAcpiSdtProtocolGuid,
                  NULL,
                  (VOID **)&AcpiSdtProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to locate ACPI table protocol\n"));
    return;
  }

  TableIndex = 0;
  Status = AcpiLocateTableBySignature (
             AcpiSdtProtocol,
             EFI_ACPI_6_3_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
             &TableIndex,
             &Table,
             &TableKey
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a %d Status = %r \n", __FUNCTION__, __LINE__, Status));
    ASSERT_EFI_ERROR (Status);
    return;
  }

  Status = AcpiSdtProtocol->OpenSdt (TableKey, &TableHandle);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    AcpiSdtProtocol->Close (TableHandle);
    return;
  }

  for (Idx1 = 0; Idx1 < Ac01PcieGetTotalHBs (); Idx1++) {
    for (Idx2 = 0; Idx2 < Ac01PcieGetTotalRBsPerHB (Idx1); Idx2++) {
      AsciiSPrint (NodePath, sizeof (NodePath), "\\_SB.PCI%X._STA", Count);
      if (Ac01PcieCheckRootBridgeDisabled (Idx1, Idx2)) {
        UpdateStatusMethodObject (AcpiSdtProtocol, TableHandle, NodePath, 0x0);
      } else {
        UpdateStatusMethodObject (AcpiSdtProtocol, TableHandle, NodePath, 0xf);
      }
      Count++;
    }
  }

  AcpiSdtProtocol->Close (TableHandle);
  AcpiUpdateChecksum ((UINT8 *)Table, Table->Length);

  //
  // Close the event, so it will not be signalled again.
  //
  gBS->CloseEvent (Event);
}

EFI_STATUS
HostBridgeConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                      Status;
  UINT8                           Index;
  EFI_PCI_ROOT_BRIDGE_DEVICE_PATH *DevicePath;
  EFI_EVENT                       EvtReadyToBoot;

  mRootBridges = AllocatePool (Ac01PcieGetTotalHBs () * sizeof (PCI_ROOT_BRIDGE));
  if (mRootBridges == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = Ac01PcieSetup ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < Ac01PcieGetTotalHBs (); Index++) {
    CopyMem (&mRootBridges[mRootBridgeCount], &mRootBridgeTemplate, sizeof (PCI_ROOT_BRIDGE));

    Status = Ac01PcieSetupRootBridge (Index, 0, (VOID *)&mRootBridges[mRootBridgeCount]);
    if (EFI_ERROR (Status)) {
      continue;
    }
    mRootBridges[mRootBridgeCount].Segment = Ac01PcieGetRootBridgeSegmentNumber (Index, 0);

    DevicePath = AllocateCopyPool (
                   sizeof (EFI_PCI_ROOT_BRIDGE_DEVICE_PATH),
                   (VOID *)&mEfiPciRootBridgeDevicePath
                   );
    if (DevicePath == NULL) {
      continue;
    }

    //
    // Embedded RC Index to the DevicePath
    // This will be used later by the platform NotifyPhase()
    //
    DevicePath->AcpiDevicePath.UID = Index;

    mRootBridges[mRootBridgeCount].DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;
    mRootBridgeCount++;
  }

  Ac01PcieEnd ();

  //
  // Register event to fixup ACPI table
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,                 // Type,
                  TPL_NOTIFY,                        // NotifyTpl
                  PciHostBridgeReadyToBootEvent,     // NotifyFunction
                  NULL,                              // NotifyContext
                  &gEfiEventReadyToBootGuid,         // EventGroup
                  &EvtReadyToBoot                    // Event
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  Return all the root bridge instances in an array.

  @param Count  Return the count of root bridge instances.

  @return All the root bridge instances in an array.
          The array should be passed into PciHostBridgeFreeRootBridges()
          when it's not used.
**/
PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
  UINTN *Count
  )
{
  *Count = mRootBridgeCount;
  return mRootBridges;
}

/**
  Free the root bridge instances array returned from PciHostBridgeGetRootBridges().

  @param Bridges The root bridge instances array.
  @param Count   The count of the array.
**/
VOID
EFIAPI
PciHostBridgeFreeRootBridges (
  PCI_ROOT_BRIDGE *Bridges,
  UINTN           Count
  )
{
  //
  // Unsupported
  //
}

/**
  Inform the platform that the resource conflict happens.

  @param HostBridgeHandle Handle of the Host Bridge.
  @param Configuration    Pointer to PCI I/O and PCI memory resource
                          descriptors. The Configuration contains the resources
                          for all the root bridges. The resource for each root
                          bridge is terminated with END descriptor and an
                          additional END is appended indicating the end of the
                          entire resources. The resource descriptor field
                          values follow the description in
                          EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
                          .SubmitResources().
**/
VOID
EFIAPI
PciHostBridgeResourceConflict (
  EFI_HANDLE                        HostBridgeHandle,
  VOID                              *Configuration
  )
{
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Descriptor;
  UINTN                             RootBridgeIndex;
  DEBUG ((DEBUG_ERROR, "PciHostBridge: Resource conflict happens!\n"));

  RootBridgeIndex = 0;
  Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Configuration;
  while (Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR) {
    DEBUG ((DEBUG_ERROR, "RootBridge[%d]:\n", RootBridgeIndex++));
    for (; Descriptor->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR; Descriptor++) {
      ASSERT (Descriptor->ResType <
              (sizeof (mPciHostBridgeLibAcpiAddressSpaceTypeStr) /
               sizeof (mPciHostBridgeLibAcpiAddressSpaceTypeStr[0])
               )
              );
      DEBUG ((DEBUG_ERROR, " %s: Length/Alignment = 0x%lx / 0x%lx\n",
              mPciHostBridgeLibAcpiAddressSpaceTypeStr[Descriptor->ResType],
              Descriptor->AddrLen, Descriptor->AddrRangeMax
              ));
      if (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {
        DEBUG ((DEBUG_ERROR, "     Granularity/SpecificFlag = %ld / %02x%s\n",
                Descriptor->AddrSpaceGranularity, Descriptor->SpecificFlag,
                ((Descriptor->SpecificFlag &
                  EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE
                  ) != 0) ? L" (Prefetchable)" : L""
                ));
      }
    }
    //
    // Skip the END descriptor for root bridge
    //
    ASSERT (Descriptor->Desc == ACPI_END_TAG_DESCRIPTOR);
    Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)(
                   (EFI_ACPI_END_TAG_DESCRIPTOR *)Descriptor + 1
                   );
  }
}
