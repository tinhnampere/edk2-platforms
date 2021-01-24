/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/PciLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/DebugLib.h>
#include <Library/ArmLib.h>
#include <Library/PciHostBridgeLib.h>
#include <IndustryStandard/Pci22.h>
#include <Guid/EventGroup.h>
#include <Library/AcpiHelperLib.h>
#include <Library/PciHostBridgeElink.h>
#include "PciHostBridge.h"
#include "PciRootBridgeIo.h"

EFI_HANDLE mDriverImageHandle;

PCI_HOST_BRIDGE_INSTANCE mPciHostBridgeInstanceTemplate = {
  PCI_HOST_BRIDGE_SIGNATURE,  // Signature
  NULL,                       // HostBridgeHandle
  0,                          // RootBridgeNumber
  {NULL, NULL},               // Head
  FALSE,                      // ResourceSubiteed
  TRUE,                       // CanRestarted
  {
    NotifyPhase,
    GetNextRootBridge,
    GetAttributes,
    StartBusEnumeration,
    SetBusNumbers,
    SubmitResources,
    GetProposedResources,
    PreprocessController
  }
};

EFI_PCI_ROOT_BRIDGE_DEVICE_PATH mPciDevicePathTemplate =
{
  {
    {
    ACPI_DEVICE_PATH,
    ACPI_DP,
    {
      (UINT8) (sizeof (ACPI_HID_DEVICE_PATH)),
      (UINT8) ((sizeof (ACPI_HID_DEVICE_PATH)) >> 8)
    }
    },
    EISA_PNP_ID (0x0A08),
    0
  },

  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
    END_DEVICE_PATH_LENGTH,
    0
    }
  }
};

STATIC
EFI_PCI_ROOT_BRIDGE_DEVICE_PATH *
EFIAPI
GenerateRootBridgeDevicePath (
  UINTN HostBridgeIdx,
  UINTN RootBridgeIdx
  )
{

  EFI_PCI_ROOT_BRIDGE_DEVICE_PATH *RootBridgeDevPath = NULL;

  RootBridgeDevPath = AllocateCopyPool (
                        sizeof (EFI_PCI_ROOT_BRIDGE_DEVICE_PATH),
                        (VOID *) &mPciDevicePathTemplate
                        );
  if (RootBridgeDevPath == NULL) {
    return NULL;
  }

  /* We don't expect to have more than 65536 root ports on the same root bridge */
  RootBridgeDevPath->AcpiDevicePath.UID = (UINT32) ((HostBridgeIdx << 16) + RootBridgeIdx);

  return RootBridgeDevPath;
}

/**
  This function will be called when ReadyToBoot event will be signaled.

  @param Event signaled event
  @param Context calling context

  @retval VOID
**/
VOID
EFIAPI
PciHostBridgeReadyToBootEvent (
  EFI_EVENT Event,
  VOID *Context
  )
{
  UINTN    Idx1, Idx2, Count = 0;
  CHAR8    NodePath[MAX_ACPI_NODE_PATH];

  for (Idx1 = 0; Idx1 < PCI_GET_NUMBER_HOSTBRIDGE (); Idx1++) {
    for (Idx2 = 0; Idx2 < PCI_GET_NUMBER_ROOTBRIDGE (Idx1); Idx2++) {
      AsciiSPrint (NodePath, sizeof (NodePath), "\\_SB.PCI%X._STA", Count);
      if (PCI_CHECK_ROOT_BRIDGE_DISABLED (Idx1, Idx2)) {
        AcpiDSDTSetNodeStatusValue (NodePath, 0x0);
      } else {
        AcpiDSDTSetNodeStatusValue (NodePath, 0xf);
      }
      Count++;
    }
  }

  //
  // Close the event, so it will not be signalled again.
  //
  gBS->CloseEvent (Event);
}

/**
   Construct the Pci Root Bridge Io protocol

   @param Protocol         Point to protocol instance
   @param HostBridgeHandle Handle of host bridge
   @param Attri            Attribute of host bridge
   @param ResAppeture      ResourceAppeture for host bridge

   @retval EFI_SUCCESS Success to initialize the Pci Root Bridge.

**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeConstructor (
  IN PCI_ROOT_BRIDGE_INSTANCE           *RootBridgeInstance,
  IN EFI_HANDLE                         HostBridgeHandle,
  IN UINT64                             Attri,
  IN UINT32                             Seg
  )
{
  MRES_TYPE                         Index;

  //
  // The host to pci bridge, the host memory and io addresses are
  // integrated as PCIe controller subsystem resource. We move forward to mark
  // resource as ResAllocated.
  //
  for (Index = rtBus; Index < rtMaxRes; Index++) {
    RootBridgeInstance->ResAllocNode[Index].Type      = Index;
    RootBridgeInstance->ResAllocNode[Index].Base      = 0;
    RootBridgeInstance->ResAllocNode[Index].Length    = 0;
    RootBridgeInstance->ResAllocNode[Index].Status    = ResNone;
  }

  RootBridgeInstance->RootBridgeAttrib                 = Attri;
  RootBridgeInstance->RootBridge.Supports              = EFI_PCI_ATTRIBUTE_DUAL_ADDRESS_CYCLE;
  // Support Extended (4096-byte) Configuration Space
  RootBridgeInstance->RootBridge.NoExtendedConfigSpace = FALSE;
  RootBridgeInstance->RootBridge.Attributes            = RootBridgeInstance->RootBridge.Supports;

  RootBridgeInstance->RbIo.ParentHandle   = HostBridgeHandle;

  RootBridgeInstance->RbIo.PollMem        = RootBridgeIoPollMem;
  RootBridgeInstance->RbIo.PollIo         = RootBridgeIoPollIo;

  RootBridgeInstance->RbIo.Mem.Read       = RootBridgeIoMemRead;
  RootBridgeInstance->RbIo.Mem.Write      = RootBridgeIoMemWrite;

  RootBridgeInstance->RbIo.Io.Read        = RootBridgeIoIoRead;
  RootBridgeInstance->RbIo.Io.Write       = RootBridgeIoIoWrite;

  RootBridgeInstance->RbIo.CopyMem        = RootBridgeIoCopyMem;

  RootBridgeInstance->RbIo.Pci.Read       = RootBridgeIoPciRead;
  RootBridgeInstance->RbIo.Pci.Write      = RootBridgeIoPciWrite;

  RootBridgeInstance->RbIo.Map            = RootBridgeIoMap;
  RootBridgeInstance->RbIo.Unmap          = RootBridgeIoUnmap;

  RootBridgeInstance->RbIo.AllocateBuffer = RootBridgeIoAllocateBuffer;
  RootBridgeInstance->RbIo.FreeBuffer     = RootBridgeIoFreeBuffer;

  RootBridgeInstance->RbIo.Flush          = RootBridgeIoFlush;

  RootBridgeInstance->RbIo.GetAttributes  = RootBridgeIoGetAttributes;
  RootBridgeInstance->RbIo.SetAttributes  = RootBridgeIoSetAttributes;

  RootBridgeInstance->RbIo.Configuration  = RootBridgeIoConfiguration;

  RootBridgeInstance->RbIo.SegmentNumber  = Seg;

  return EFI_SUCCESS;
}

/**
   Entry point of this driver

   @param ImageHandle     Handle of driver image
   @param SystemTable     Point to EFI_SYSTEM_TABLE

   @retval EFI_OUT_OF_RESOURCES  Can not allocate memory resource
   @retval EFI_DEVICE_ERROR      Can not install the protocol instance
   @retval EFI_SUCCESS           Success to initialize the Pci host bridge.
**/
EFI_STATUS
EFIAPI
InitializePciHostBridge (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  STATIC EFI_GUID                 guidReadyToBoot = EFI_EVENT_GROUP_READY_TO_BOOT;
  EFI_EVENT                       EvtReadyToBoot;
  EFI_STATUS                      Status;
  UINTN                           Idx1;
  UINTN                           Idx2;
  UINTN                           Count = 0;
  PCI_HOST_BRIDGE_INSTANCE        *HostBridgeInstance = NULL;
  PCI_ROOT_BRIDGE_INSTANCE        *RootBridgeInstance = NULL;
  UINTN                           NumberRootPortInstalled = FALSE;
  LIST_ENTRY                      *List;
  UINTN                           SegmentNumber;

  if (( PCI_CHECK_ROOT_BRIDGE_DISABLED == NULL)
        || (PCI_CORE_SETUP == NULL)
        || (PCI_CORE_END == NULL)
        || (PCI_CORE_SETUP_HOST_BRIDGE == NULL)
        || (PCI_CORE_SETUP_ROOT_BRIDGE == NULL)
        || (PCI_CORE_IO_PCI_RW == NULL)
        || (PCI_GET_NUMBER_HOSTBRIDGE == NULL)
        || (PCI_GET_NUMBER_ROOTBRIDGE == NULL)
        || (PCI_GET_ROOTBRIDGE_ATTR == NULL ))
  {
    PCIE_ERR ("PciHostBridge: Invalid Parameters\n");
    return EFI_INVALID_PARAMETER;
  }

  PCIE_DEBUG ("%a: START\n", __FUNCTION__);

  mDriverImageHandle = ImageHandle;

  // Inform Pcie Core BSP Driver to start setup phase
  Status = PCI_CORE_SETUP (ImageHandle, SystemTable);
  if (EFI_ERROR (Status)) {
    PCIE_ERR ("  PCIe Core Setup failed!\n");
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Create Host Bridge Device Handle
  //
  for (Idx1 = 0; Idx1 < PCI_GET_NUMBER_HOSTBRIDGE (); Idx1++) {
    HostBridgeInstance = AllocateCopyPool (
                           sizeof (PCI_HOST_BRIDGE_INSTANCE),
                           (VOID *) &mPciHostBridgeInstanceTemplate
                           );
    if (HostBridgeInstance == NULL) {
      PCIE_ERR ("  HB%d allocation failed!\n", Idx1);
      return EFI_OUT_OF_RESOURCES;
    }

    Status = PCI_CORE_SETUP_HOST_BRIDGE (Idx1);
    if (EFI_ERROR (Status)) {
      FreePool (HostBridgeInstance);
      PCIE_ERR ("  HB%d setup failed!\n", Idx1);
      return EFI_OUT_OF_RESOURCES;
    }

    HostBridgeInstance->RootBridgeNumber = PCI_GET_NUMBER_ROOTBRIDGE (Idx1);

    InitializeListHead (&HostBridgeInstance->Head);

    Status = gBS->InstallMultipleProtocolInterfaces (
                    &HostBridgeInstance->HostBridgeHandle,
                    &gEfiPciHostBridgeResourceAllocationProtocolGuid,
                    &HostBridgeInstance->ResAlloc,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      FreePool (HostBridgeInstance);
      PCIE_ERR ("  HB%d instance installation failed\n", Idx1);
      return EFI_DEVICE_ERROR;
    }

    NumberRootPortInstalled = 0;

    //
    // Create Root Bridge Device Handle in this Host Bridge
    //
    for (Idx2 = 0; Idx2 < HostBridgeInstance->RootBridgeNumber; Idx2++, Count++) {
      RootBridgeInstance = AllocateZeroPool (sizeof (PCI_ROOT_BRIDGE_INSTANCE));
      if (RootBridgeInstance == NULL) {
        gBS->UninstallMultipleProtocolInterfaces (
               HostBridgeInstance->HostBridgeHandle,
               &gEfiPciHostBridgeResourceAllocationProtocolGuid,
               &HostBridgeInstance->ResAlloc,
               NULL
               );
        PCIE_ERR ("    HB%d-RB%d allocation failed!\n", Idx1, Idx2);
        return EFI_OUT_OF_RESOURCES;
      }

      // Initialize Hardware
      Status = PCI_CORE_SETUP_ROOT_BRIDGE (Idx1, Idx2, (VOID *) &RootBridgeInstance->RootBridge);
      if (EFI_ERROR (Status)) {
        FreePool (RootBridgeInstance);
        PCIE_ERR ("    HB%d-RB%d setup failed!\n", Idx1, Idx2);
        continue;
      }

      NumberRootPortInstalled++;

      RootBridgeInstance->ConfigBuffer = AllocateZeroPool (
                                           rtMaxRes * sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR)
                                           + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR)
                                           );
      if (RootBridgeInstance->ConfigBuffer == NULL) {
        gBS->UninstallMultipleProtocolInterfaces (
               HostBridgeInstance->HostBridgeHandle,
               &gEfiPciHostBridgeResourceAllocationProtocolGuid,
               &HostBridgeInstance->ResAlloc,
               NULL
               );
        FreePool (RootBridgeInstance);
        PCIE_ERR ("    HB%d-RB%d Descriptor allocation failed!\n", Idx1, Idx2);
        return EFI_OUT_OF_RESOURCES;
      }

      RootBridgeInstance->Signature = PCI_ROOT_BRIDGE_SIGNATURE;
      RootBridgeInstance->RootBridge.DevicePath =
        (EFI_DEVICE_PATH_PROTOCOL *) GenerateRootBridgeDevicePath (Idx1, Idx2);

      SegmentNumber = Count;
      if (PCI_GET_ROOTBRIDGE_SEGMENTNUMBER) {
        SegmentNumber = PCI_GET_ROOTBRIDGE_SEGMENTNUMBER (Idx1, Idx2);
      }

      RootBridgeConstructor (
        RootBridgeInstance,
        HostBridgeInstance->HostBridgeHandle,
        PCI_GET_ROOTBRIDGE_ATTR (Idx1),
        SegmentNumber
        );

      Status = gBS->InstallMultipleProtocolInterfaces (
                      &RootBridgeInstance->RootBridgeHandle,
                      &gEfiDevicePathProtocolGuid,
                      RootBridgeInstance->RootBridge.DevicePath,
                      &gEfiPciRootBridgeIoProtocolGuid,
                      &RootBridgeInstance->RbIo,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        // Uninstall all root ports of this bridge
        List = HostBridgeInstance->Head.ForwardLink;
        while (List != &HostBridgeInstance->Head) {
          RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);
          gBS->UninstallMultipleProtocolInterfaces (
                 RootBridgeInstance->RootBridgeHandle,
                 &gEfiDevicePathProtocolGuid,
                 RootBridgeInstance->RootBridge.DevicePath,
                 &gEfiPciRootBridgeIoProtocolGuid,
                 &RootBridgeInstance->RbIo,
                 NULL
                 );
          FreePool (RootBridgeInstance->ConfigBuffer);
          FreePool (RootBridgeInstance);
          List = List->ForwardLink;
        }

        gBS->UninstallMultipleProtocolInterfaces (
               HostBridgeInstance->HostBridgeHandle,
               &gEfiPciHostBridgeResourceAllocationProtocolGuid,
               &HostBridgeInstance->ResAlloc,
               NULL
               );
        FreePool (HostBridgeInstance);
        PCIE_ERR ("    HB%d-RB%d instance installation failed\n", Idx1, Idx2);
        return EFI_DEVICE_ERROR;
      }

      InsertTailList (&HostBridgeInstance->Head, &RootBridgeInstance->Link);
    }

    if (NumberRootPortInstalled == 0) {
      PCIE_WARN ("  No Root Port! Uninstalling HB%d\n", Idx1);
      gBS->UninstallMultipleProtocolInterfaces (
             HostBridgeInstance->HostBridgeHandle,
             &gEfiPciHostBridgeResourceAllocationProtocolGuid,
             &HostBridgeInstance->ResAlloc,
             NULL
             );
      FreePool (HostBridgeInstance);
    }
  }

  // Inform BSP Pcie Driver to end setup phase
  PCI_CORE_END ();

  /* Event for ACPI Menu configuration */
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,                 // Type,
                  TPL_NOTIFY,                        // NotifyTpl
                  PciHostBridgeReadyToBootEvent,     // NotifyFunction
                  NULL,                              // NotifyContext
                  &guidReadyToBoot,                  // EventGroup
                  &EvtReadyToBoot                    // Event
                  );

  PCIE_DEBUG ("%a: END\n", __FUNCTION__);

  return Status;
}

/**
   These are the notifications from the PCI bus driver that it is about to enter a certain
   phase of the PCI enumeration process.

   This member function can be used to notify the host bridge driver to perform specific actions,
   including any chipset-specific initialization, so that the chipset is ready to enter the next phase.
   Eight notification points are defined at this time. See belows:
   EfiPciHostBridgeBeginEnumeration       Resets the host bridge PCI apertures and internal data
                                          structures. The PCI enumerator should issue this notification
                                          before starting a fresh enumeration process. Enumeration cannot
                                          be restarted after sending any other notification such as
                                          EfiPciHostBridgeBeginBusAllocation.
   EfiPciHostBridgeBeginBusAllocation     The bus allocation phase is about to begin. No specific action is
                                          required here. This notification can be used to perform any
                                          chipset-specific programming.
   EfiPciHostBridgeEndBusAllocation       The bus allocation and bus programming phase is complete. No
                                          specific action is required here. This notification can be used to
                                          perform any chipset-specific programming.
   EfiPciHostBridgeBeginResourceAllocation
                                          The resource allocation phase is about to begin. No specific
                                          action is required here. This notification can be used to perform
                                          any chipset-specific programming.
   EfiPciHostBridgeAllocateResources      Allocates resources per previously submitted requests for all the PCI
                                          root bridges. These resource settings are returned on the next call to
                                          GetProposedResources(). Before calling NotifyPhase() with a Phase of
                                          EfiPciHostBridgeAllocateResource, the PCI bus enumerator is responsible
                                          for gathering I/O and memory requests for
                                          all the PCI root bridges and submitting these requests using
                                          SubmitResources(). This function pads the resource amount
                                          to suit the root bridge hardware, takes care of dependencies between
                                          the PCI root bridges, and calls the Global Coherency Domain (GCD)
                                          with the allocation request. In the case of padding, the allocated range
                                          could be bigger than what was requested.
   EfiPciHostBridgeSetResources           Programs the host bridge hardware to decode previously allocated
                                          resources (proposed resources) for all the PCI root bridges. After the
                                          hardware is programmed, reassigning resources will not be supported.
                                          The bus settings are not affected.
   EfiPciHostBridgeFreeResources          Deallocates resources that were previously allocated for all the PCI
                                          root bridges and resets the I/O and memory apertures to their initial
                                          state. The bus settings are not affected. If the request to allocate
                                          resources fails, the PCI enumerator can use this notification to
                                          deallocate previous resources, adjust the requests, and retry
                                          allocation.
   EfiPciHostBridgeEndResourceAllocation  The resource allocation phase is completed. No specific action is
                                          required here. This notification can be used to perform any chipsetspecific
                                          programming.

   @param[in] This                The instance pointer of EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
   @param[in] Phase               The phase during enumeration

   @retval EFI_NOT_READY          This phase cannot be entered at this time. For example, this error
                                  is valid for a Phase of EfiPciHostBridgeAllocateResources if
                                  SubmitResources() has not been called for one or more
                                  PCI root bridges before this call
   @retval EFI_DEVICE_ERROR       Programming failed due to a hardware error. This error is valid
                                  for a Phase of EfiPciHostBridgeSetResources.
   @retval EFI_INVALID_PARAMETER  Invalid phase parameter
   @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a lack of resources.
                                  This error is valid for a Phase of EfiPciHostBridgeAllocateResources if the
                                  previously submitted resource requests cannot be fulfilled or
                                  were only partially fulfilled.
   @retval EFI_SUCCESS            The notification was accepted without any errors.

**/
EFI_STATUS
EFIAPI
NotifyPhase (
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL *This,
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE    Phase
  )
{
  PCI_HOST_BRIDGE_INSTANCE              *HostBridgeInstance;
  PCI_ROOT_BRIDGE_INSTANCE              *RootBridgeInstance;
  MRES_TYPE                             Index;
  PCI_RES_NODE                          *ResNode;
  LIST_ENTRY                            *List;
  EFI_PHYSICAL_ADDRESS                  AddrBase;
  EFI_PHYSICAL_ADDRESS                  AddrLimit;
  UINT64                                AddrLen;
  UINTN                                 BitsOfAlignment;
  EFI_STATUS                            Status;
  EFI_STATUS                            ReturnStatus;
  EFI_PCI_ROOT_BRIDGE_DEVICE_PATH       *DevPath;
  UINTN                                 HostBridgeIdx, RootBridgeIdx;

  HostBridgeInstance = PCI_HOST_BRIDGE_FROM_THIS (This);
  ReturnStatus = EFI_SUCCESS;

  switch (Phase) {

  case EfiPciHostBridgeBeginEnumeration:
    PCIE_DEBUG ("PciHostBridge: NotifyPhase (BeginEnumeration)\n");

    if (!HostBridgeInstance->CanRestarted) {
      return EFI_NOT_READY;
    }

    //
    // Reset each Root Bridge
    //
    List = HostBridgeInstance->Head.ForwardLink;
    while (List != &HostBridgeInstance->Head) {
      RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);

      for (Index = rtBus; Index < rtMaxRes; Index++) {
        ResNode = &(RootBridgeInstance->ResAllocNode[Index]);

        ResNode->Type      = Index;
        ResNode->Base      = 0;
        ResNode->Length    = 0;
        ResNode->Status    = ResNone;
      }

      List = List->ForwardLink;
    }

    HostBridgeInstance->ResourceSubmited = FALSE;
    HostBridgeInstance->CanRestarted     = TRUE;
    break;

  case EfiPciHostBridgeEndEnumeration:
    //
    // The Host Bridge Enumeration is completed. No specific action is required here.
    // This notification can be used to perform any chipset specific programming.
    //
    PCIE_DEBUG ("PciHostBridge: NotifyPhase (EndEnumeration)\n");
    break;

  case EfiPciHostBridgeBeginBusAllocation:
    // No specific action is required here, can perform any chipset specific programing
    PCIE_DEBUG ("PciHostBridge: NotifyPhase (BeginBusAllocation)\n");
    HostBridgeInstance->CanRestarted = FALSE;
    break;

  case EfiPciHostBridgeEndBusAllocation:
    // No specific action is required here, can perform any chipset specific programing
    PCIE_DEBUG ("PciHostBridge: NotifyPhase (EndBusAllocation)\n");
    break;

  case EfiPciHostBridgeBeginResourceAllocation:
    // No specific action is required here, can perform any chipset specific programing
    PCIE_DEBUG ("PciHostBridge: NotifyPhase (BeginResourceAllocation)\n");
    break;

  case EfiPciHostBridgeAllocateResources:

    // Make sure the resource for all root bridges has been submitted.
    if (!HostBridgeInstance->ResourceSubmited) {
      return EFI_NOT_READY;
    }

    PCIE_DEBUG ("PciHostBridge: NotifyPhase (AllocateResources)\n");

    //
    // Take care of the resource dependencies between the root bridges
    //
    List = HostBridgeInstance->Head.ForwardLink;
    while (List != &HostBridgeInstance->Head) {

      RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);

      for (Index = rtIo16; Index < rtMaxRes; Index++) {
        ResNode = &(RootBridgeInstance->ResAllocNode[Index]);

        if (ResNode->Status == ResNone) {
          continue;
        }

        switch (Index) {
        case rtIo16:
        case rtIo32:
          AddrBase = RootBridgeInstance->RootBridge.Io.Base;
          AddrLimit = RootBridgeInstance->RootBridge.Io.Limit;
          break;

        case rtMmio32:
          AddrBase = RootBridgeInstance->RootBridge.Mem.Base;
          AddrLimit = RootBridgeInstance->RootBridge.Mem.Limit;
          break;

        case rtMmio32p:
          AddrBase = RootBridgeInstance->RootBridge.PMem.Base;
          AddrLimit = RootBridgeInstance->RootBridge.PMem.Limit;
          break;

        case rtMmio64:
          AddrBase = RootBridgeInstance->RootBridge.MemAbove4G.Base;
          AddrLimit = RootBridgeInstance->RootBridge.MemAbove4G.Limit;
          break;

        case rtMmio64p:
          AddrBase = RootBridgeInstance->RootBridge.PMemAbove4G.Base;
          AddrLimit = RootBridgeInstance->RootBridge.PMemAbove4G.Limit;
          break;

        default:
          ASSERT (FALSE);
          break;
        }; //end switch (Index)

        AddrLen = ResNode->Length;

        if ((AddrBase + AddrLen - 1) > AddrLimit) {
          ReturnStatus = EFI_OUT_OF_RESOURCES;
          ResNode->Length = 0;
        } else {
          // Get the number of '1' in Alignment.
          BitsOfAlignment = (UINTN) (HighBitSet64 (ResNode->Alignment) + 1);

          Status = gDS->AllocateMemorySpace (
                          EfiGcdAllocateAddress,
                          EfiGcdMemoryTypeMemoryMappedIo,
                          BitsOfAlignment,
                          AddrLen,
                          &AddrBase,
                          mDriverImageHandle,
                          NULL
                          );

          if (!EFI_ERROR (Status)) {
            ResNode->Base   = (UINTN) AddrBase;
            ResNode->Status = ResAllocated;
          } else {
            ReturnStatus = EFI_OUT_OF_RESOURCES;
            ResNode->Length = 0;
          }
        }
      } // end for
      List = List->ForwardLink;
    } // end while

    break;

  case EfiPciHostBridgeSetResources:
    PCIE_DEBUG ("PciHostBridge: NotifyPhase (SetResources)\n");
    break;

  case EfiPciHostBridgeFreeResources:
    PCIE_DEBUG ("PciHostBridge: NotifyPhase (FreeResources)\n");

    List = HostBridgeInstance->Head.ForwardLink;

    while (List != &HostBridgeInstance->Head) {
      RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);

      for (Index = rtIo16; Index < rtMaxRes; Index++) {
        ResNode = &(RootBridgeInstance->ResAllocNode[Index]);

        if (ResNode->Status == ResAllocated) {
          AddrLen = ResNode->Length;
          AddrBase = ResNode->Base;

          switch (Index) {
          case rtIo16:
          case rtIo32:
          case rtMmio32:
          case rtMmio32p:
          case rtMmio64:
          case rtMmio64p:
            Status = gDS->FreeMemorySpace (AddrBase, AddrLen);
            if (EFI_ERROR (Status)) {
              ReturnStatus = Status;
            }
            break;

          default:
            ASSERT (FALSE);
            break;
          }; //end switch (Index)

          ResNode->Type      = Index;
          ResNode->Base      = 0;
          ResNode->Length    = 0;
          ResNode->Status    = ResNone;
        }
      }

      List = List->ForwardLink;
    }

    HostBridgeInstance->ResourceSubmited = FALSE;
    HostBridgeInstance->CanRestarted     = TRUE;
    break;

  case EfiPciHostBridgeEndResourceAllocation:
    //
    // The resource allocation phase is completed.  No specific action is required
    // here. This notification can be used to perform any chipset specific programming.
    //
    PCIE_DEBUG ("PciHostBridge: NotifyPhase (EndResourceAllocation)\n");
    HostBridgeInstance->CanRestarted = FALSE;
    break;

  default:
    return EFI_INVALID_PARAMETER;
  }

  // Notify BSP Driver the phase we are being
  List = HostBridgeInstance->Head.ForwardLink;
  while (List != &HostBridgeInstance->Head) {
    RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);

    // Retrieve the HostBridgeIdx and RootBridgeIdx from UID
    // UID = (UINT32)((HostBridgeIdx << 16) + RootBridgeIdx);
    DevPath = (EFI_PCI_ROOT_BRIDGE_DEVICE_PATH*) RootBridgeInstance->RootBridge.DevicePath;
    HostBridgeIdx = DevPath->AcpiDevicePath.UID / (1<<16);
    RootBridgeIdx = DevPath->AcpiDevicePath.UID % (1<<16);

    // Notify BSP Driver
    if (PCI_CORE_HOST_BRIDGE_NOTIFY_PHASE != NULL) {
      PCI_CORE_HOST_BRIDGE_NOTIFY_PHASE (HostBridgeIdx, RootBridgeIdx, Phase);
    }

    List = List->ForwardLink;
  }

  return ReturnStatus;
}

/**
   Return the device handle of the next PCI root bridge that is associated with this Host Bridge.

   This function is called multiple times to retrieve the device handles of all the PCI root bridges that
   are associated with this PCI host bridge. Each PCI host bridge is associated with one or more PCI
   root bridges. On each call, the handle that was returned by the previous call is passed into the
   interface, and on output the interface returns the device handle of the next PCI root bridge. The
   caller can use the handle to obtain the instance of the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
   for that root bridge. When there are no more PCI root bridges to report, the interface returns
   EFI_NOT_FOUND. A PCI enumerator must enumerate the PCI root bridges in the order that they
   are returned by this function.
   For D945 implementation, there is only one root bridge in PCI host bridge.

   @param[in]       This              The instance pointer of EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
   @param[in, out]  RootBridgeHandle  Returns the device handle of the next PCI root bridge.

   @retval EFI_SUCCESS            If parameter RootBridgeHandle = NULL, then return the first Rootbridge handle of the
                                  specific Host bridge and return EFI_SUCCESS.
   @retval EFI_NOT_FOUND          Can not find the any more root bridge in specific host bridge.
   @retval EFI_INVALID_PARAMETER  RootBridgeHandle is not an EFI_HANDLE that was
                                  returned on a previous call to GetNextRootBridge().
**/
EFI_STATUS
EFIAPI
GetNextRootBridge (
  IN       EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL *This,
  IN OUT   EFI_HANDLE                                       *RootBridgeHandle
  )
{
  BOOLEAN                               NoRootBridge;
  LIST_ENTRY                            *List;
  PCI_HOST_BRIDGE_INSTANCE              *HostBridgeInstance;
  PCI_ROOT_BRIDGE_INSTANCE              *RootBridgeInstance;

  NoRootBridge = TRUE;
  HostBridgeInstance = PCI_HOST_BRIDGE_FROM_THIS (This);
  List = HostBridgeInstance->Head.ForwardLink;

  while (List != &HostBridgeInstance->Head) {
    NoRootBridge = FALSE;
    RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);

    if (*RootBridgeHandle == NULL) {
      //
      // Return the first Root Bridge Handle of the Host Bridge
      //
      *RootBridgeHandle = RootBridgeInstance->RootBridgeHandle;

      return EFI_SUCCESS;
    } else {
      if (*RootBridgeHandle == RootBridgeInstance->RootBridgeHandle) {
        //
        // Get next if have
        //
        List = List->ForwardLink;

        if (List != &HostBridgeInstance->Head) {
          RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);
          *RootBridgeHandle = RootBridgeInstance->RootBridgeHandle;

          return EFI_SUCCESS;
        }

        return EFI_NOT_FOUND;
      }
    }

    List = List->ForwardLink;
  } //end while

  return NoRootBridge ? EFI_NOT_FOUND : EFI_INVALID_PARAMETER;
}

/**
   Returns the allocation attributes of a PCI root bridge.

   The function returns the allocation attributes of a specific PCI root bridge. The attributes can vary
   from one PCI root bridge to another. These attributes are different from the decode-related
   attributes that are returned by the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.GetAttributes() member function. The
   RootBridgeHandle parameter is used to specify the instance of the PCI root bridge. The device
   handles of all the root bridges that are associated with this host bridge must be obtained by calling
   GetNextRootBridge(). The attributes are static in the sense that they do not change during or
   after the enumeration process. The hardware may provide mechanisms to change the attributes on
   the fly, but such changes must be completed before EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL is
   installed. The permitted values of EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_ATTRIBUTES are defined in
   "Related Definitions" below. The caller uses these attributes to combine multiple resource requests.
   For example, if the flag EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM is set, the PCI bus enumerator needs to
   include requests for the prefetchable memory in the nonprefetchable memory pool and not request any
   prefetchable memory.
      Attribute                                 Description
   ------------------------------------         ----------------------------------------------------------------------
   EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM         If this bit is set, then the PCI root bridge does not support separate
                                                windows for nonprefetchable and prefetchable memory. A PCI bus
                                                driver needs to include requests for prefetchable memory in the
                                                nonprefetchable memory pool.

   EFI_PCI_HOST_BRIDGE_MEM64_DECODE             If this bit is set, then the PCI root bridge supports 64-bit memory
                                                windows. If this bit is not set, the PCI bus driver needs to include
                                                requests for a 64-bit memory address in the corresponding 32-bit
                                                memory pool.

   @param[in]   This               The instance pointer of EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL
   @param[in]   RootBridgeHandle   The device handle of the PCI root bridge in which the caller is interested. Type
                                   EFI_HANDLE is defined in InstallProtocolInterface() in the UEFI 2.0 Specification.
   @param[out]  Attributes         The pointer to attribte of root bridge, it is output parameter

   @retval EFI_INVALID_PARAMETER   Attribute pointer is NULL
   @retval EFI_INVALID_PARAMETER   RootBridgehandle is invalid.
   @retval EFI_SUCCESS             Success to get attribute of interested root bridge.

**/
EFI_STATUS
EFIAPI
GetAttributes (
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL *This,
  IN  EFI_HANDLE                                       RootBridgeHandle,
  OUT UINT64                                           *Attributes
  )
{
  LIST_ENTRY                            *List;
  PCI_HOST_BRIDGE_INSTANCE              *HostBridgeInstance;
  PCI_ROOT_BRIDGE_INSTANCE              *RootBridgeInstance;

  if (Attributes == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  HostBridgeInstance = PCI_HOST_BRIDGE_FROM_THIS (This);
  List = HostBridgeInstance->Head.ForwardLink;

  while (List != &HostBridgeInstance->Head) {
    RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);
    if (RootBridgeHandle == RootBridgeInstance->RootBridgeHandle) {
      *Attributes = RootBridgeInstance->RootBridgeAttrib;

      return EFI_SUCCESS;
    }

    List = List->ForwardLink;
  }

  //
  // RootBridgeHandle is not an EFI_HANDLE
  // that was returned on a previous call to GetNextRootBridge()
  //
  return EFI_INVALID_PARAMETER;
}

/**
   Sets up the specified PCI root bridge for the bus enumeration process.

   This member function sets up the root bridge for bus enumeration and returns the PCI bus range
   over which the search should be performed in ACPI 2.0 resource descriptor format.

   @param[in]   This              The EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_ PROTOCOL instance.
   @param[in]   RootBridgeHandle  The PCI Root Bridge to be set up.
   @param[out]  Configuration     Pointer to the pointer to the PCI bus resource descriptor.

   @retval EFI_INVALID_PARAMETER Invalid Root bridge's handle
   @retval EFI_OUT_OF_RESOURCES  Fail to allocate ACPI resource descriptor tag.
   @retval EFI_SUCCESS           Sucess to allocate ACPI resource descriptor.

**/
EFI_STATUS
EFIAPI
StartBusEnumeration (
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL *This,
  IN  EFI_HANDLE                                       RootBridgeHandle,
  OUT VOID                                             **Configuration
  )
{
  LIST_ENTRY                            *List;
  PCI_HOST_BRIDGE_INSTANCE              *HostBridgeInstance;
  PCI_ROOT_BRIDGE_INSTANCE              *RootBridgeInstance;
  VOID                                  *Buffer;
  UINT8                                 *Temp;
  UINT64                                BusStart;
  UINT64                                BusEnd;

  HostBridgeInstance = PCI_HOST_BRIDGE_FROM_THIS (This);
  List = HostBridgeInstance->Head.ForwardLink;

  while (List != &HostBridgeInstance->Head) {
    RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);
    if (RootBridgeHandle == RootBridgeInstance->RootBridgeHandle) {
      //
      // Set up the Root Bridge for Bus Enumeration
      //
      BusStart = RootBridgeInstance->RootBridge.Bus.Base;
      BusEnd   = RootBridgeInstance->RootBridge.Bus.Limit;

      Buffer = AllocatePool (sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR));
      if (Buffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      Temp = (UINT8 *) Buffer;

      ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Temp)->Desc = ACPI_ADDRESS_SPACE_DESCRIPTOR;
      ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Temp)->Len  = sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) - 3;
      ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Temp)->ResType = ACPI_ADDRESS_SPACE_TYPE_BUS;
      ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Temp)->GenFlag = 0;
      ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Temp)->SpecificFlag = 0;
      ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Temp)->AddrSpaceGranularity = 0;
      ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Temp)->AddrRangeMin = BusStart;
      ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Temp)->AddrRangeMax = BusEnd;
      ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Temp)->AddrTranslationOffset = 0;
      ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Temp)->AddrLen = BusEnd - BusStart + 1;

      Temp += sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR);
      ((EFI_ACPI_END_TAG_DESCRIPTOR *)Temp)->Desc = ACPI_END_TAG_DESCRIPTOR;
      ((EFI_ACPI_END_TAG_DESCRIPTOR *)Temp)->Checksum = 0x0;

      *Configuration = Buffer;

      return EFI_SUCCESS;
    }

    List = List->ForwardLink;
  }

  return EFI_INVALID_PARAMETER;
}

/**
   Programs the PCI root bridge hardware so that it decodes the specified PCI bus range.

   This member function programs the specified PCI root bridge to decode the bus range that is
   specified by the input parameter Configuration.
   The bus range information is specified in terms of the ACPI 2.0 resource descriptor format.

   @param[in] This              The EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_ PROTOCOL instance
   @param[in] RootBridgeHandle  The PCI Root Bridge whose bus range is to be programmed
   @param[in] Configuration     The pointer to the PCI bus resource descriptor

   @retval EFI_INVALID_PARAMETER  RootBridgeHandle is not a valid root bridge handle.
   @retval EFI_INVALID_PARAMETER  Configuration is NULL.
   @retval EFI_INVALID_PARAMETER  Configuration does not point to a valid ACPI 2.0 resource descriptor.
   @retval EFI_INVALID_PARAMETER  Configuration does not include a valid ACPI 2.0 bus resource descriptor.
   @retval EFI_INVALID_PARAMETER  Configuration includes valid ACPI 2.0 resource descriptors other than
                                  bus descriptors.
   @retval EFI_INVALID_PARAMETER  Configuration contains one or more invalid ACPI resource descriptors.
   @retval EFI_INVALID_PARAMETER  "Address Range Minimum" is invalid for this root bridge.
   @retval EFI_INVALID_PARAMETER  "Address Range Length" is invalid for this root bridge.
   @retval EFI_DEVICE_ERROR       Programming failed due to a hardware error.
   @retval EFI_SUCCESS            The bus range for the PCI root bridge was programmed.

**/
EFI_STATUS
EFIAPI
SetBusNumbers (
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL *This,
  IN EFI_HANDLE                                       RootBridgeHandle,
  IN VOID                                             *Configuration
  )
{
  LIST_ENTRY                            *List;
  PCI_HOST_BRIDGE_INSTANCE              *HostBridgeInstance;
  PCI_ROOT_BRIDGE_INSTANCE              *RootBridgeInstance;
  PCI_RES_NODE                          *ResNode;
  UINT8                                 *Ptr;
  UINTN                                 BusStart;
  UINTN                                 BusEnd;
  UINTN                                 BusLen;

  if (Configuration == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Ptr = Configuration;

  //
  // Check the Configuration is valid
  //
  if (*Ptr != ACPI_ADDRESS_SPACE_DESCRIPTOR) {
    return EFI_INVALID_PARAMETER;
  }

  if (((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) Ptr)->ResType != ACPI_ADDRESS_SPACE_TYPE_BUS) {
    return EFI_INVALID_PARAMETER;
  }

  Ptr += sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR);
  if (*Ptr != ACPI_END_TAG_DESCRIPTOR) {
    return EFI_INVALID_PARAMETER;
  }

  HostBridgeInstance = PCI_HOST_BRIDGE_FROM_THIS (This);
  List = HostBridgeInstance->Head.ForwardLink;

  Ptr = Configuration;

  while (List != &HostBridgeInstance->Head) {
    RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);
    if (RootBridgeHandle == RootBridgeInstance->RootBridgeHandle) {
      BusStart = (UINTN) ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) Ptr)->AddrRangeMin;
      BusLen = (UINTN) ((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) Ptr)->AddrLen;
      BusEnd = BusStart + BusLen - 1;

      if (BusStart > BusEnd) {
        return EFI_INVALID_PARAMETER;
      }

      if ((BusStart < RootBridgeInstance->RootBridge.Bus.Base) ||
          (BusEnd > RootBridgeInstance->RootBridge.Bus.Limit)) {
        return EFI_INVALID_PARAMETER;
      }

      //
      // Update the Bus Range
      //
      ResNode = &(RootBridgeInstance->ResAllocNode[rtBus]);
      ResNode->Base   = BusStart;
      ResNode->Length = BusLen;
      ResNode->Status = ResAllocated;

      return EFI_SUCCESS;
    }

    List = List->ForwardLink;
  }

  return EFI_INVALID_PARAMETER;
}

/**
   Submits the I/O and memory resource requirements for the specified PCI root bridge.

   This function is used to submit all the I/O and memory resources that are required by the specified
   PCI root bridge. The input parameter Configuration is used to specify the following:
   - The various types of resources that are required
   - The associated lengths in terms of ACPI 2.0 resource descriptor format

   @param[in] This              Pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL instance.
   @param[in] RootBridgeHandle  The PCI root bridge whose I/O and memory resource requirements are being submitted.
   @param[in] Configuration     The pointer to the PCI I/O and PCI memory resource descriptor.

   @retval EFI_SUCCESS            The I/O and memory resource requests for a PCI root bridge were accepted.
   @retval EFI_INVALID_PARAMETER  RootBridgeHandle is not a valid root bridge handle.
   @retval EFI_INVALID_PARAMETER  Configuration is NULL.
   @retval EFI_INVALID_PARAMETER  Configuration does not point to a valid ACPI 2.0 resource descriptor.
   @retval EFI_INVALID_PARAMETER  Configuration includes requests for one or more resource types that are
                                  not supported by this PCI root bridge. This error will happen if the caller
                                  did not combine resources according to Attributes that were returned by
                                  GetAllocAttributes().
   @retval EFI_INVALID_PARAMETER  Address Range Maximum" is invalid.
   @retval EFI_INVALID_PARAMETER  "Address Range Length" is invalid for this PCI root bridge.
   @retval EFI_INVALID_PARAMETER  "Address Space Granularity" is invalid for this PCI root bridge.

**/
EFI_STATUS
EFIAPI
SubmitResources (
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL *This,
  IN EFI_HANDLE                                       RootBridgeHandle,
  IN VOID                                             *Configuration
  )
{
  LIST_ENTRY                            *List;
  PCI_HOST_BRIDGE_INSTANCE              *HostBridgeInstance;
  PCI_ROOT_BRIDGE_INSTANCE              *RootBridgeInstance = NULL;
  PCI_RES_NODE                          *ResNode;
  UINT8                                 *Temp;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR     *Ptr;
  UINTN                                 Index = 0;
  UINTN                                 AddrSpaceCnt = 0;
  UINTN                                 i;

  //
  // Check the input parameter: Configuration
  //
  if (Configuration == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  HostBridgeInstance = PCI_HOST_BRIDGE_FROM_THIS (This);
  List = HostBridgeInstance->Head.ForwardLink;

  //
  // Input resource descriptor must end properly
  //
  Temp = (UINT8 *) Configuration;
  AddrSpaceCnt = 0;
  while (*Temp == ACPI_ADDRESS_SPACE_DESCRIPTOR) {
    Temp += sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR);
    AddrSpaceCnt ++;
  }
  if (*Temp != ACPI_END_TAG_DESCRIPTOR) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Get the corresponding Root Bridge Instance
  //
  while (List != &HostBridgeInstance->Head) {
    RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);

    if (RootBridgeHandle == RootBridgeInstance->RootBridgeHandle) {
      break;
    }

    List = List->ForwardLink;
  }

  if (RootBridgeHandle != RootBridgeInstance->RootBridgeHandle) {
    return EFI_INVALID_PARAMETER;
  }

  PCIE_DEBUG ("%a: \n", __FUNCTION__);

  Temp = (UINT8 *) Configuration;
  for (i = 0; i < AddrSpaceCnt; i ++) {
    Ptr = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) Temp ;

    PCIE_DEBUG ("Ptr->ResType:%d\n", Ptr->ResType);
    PCIE_DEBUG ("Ptr->Addrlen:0x%llx\n", Ptr->AddrLen);
    PCIE_DEBUG ("Ptr->AddrRangeMax:0x%llx\n", Ptr->AddrRangeMax);
    PCIE_DEBUG ("Ptr->AddrRangeMin:0x%llx\n", Ptr->AddrRangeMin);
    PCIE_DEBUG ("Ptr->SpecificFlag:0x%llx\n", Ptr->SpecificFlag);
    PCIE_DEBUG ("Ptr->AddrSpaceGranularity:%d\n", Ptr->AddrSpaceGranularity);
    PCIE_DEBUG ("RootBridgeInstance->RootBridgeAttrib:0x%llx\n", RootBridgeInstance->RootBridgeAttrib);

    switch (Ptr->ResType) {
    case ACPI_ADDRESS_SPACE_TYPE_MEM:

      if (Ptr->AddrSpaceGranularity != 32 && Ptr->AddrSpaceGranularity != 64) {
        return EFI_INVALID_PARAMETER;
      }

      if (Ptr->AddrSpaceGranularity == 32) {
        if (Ptr->AddrLen >= SIZE_4GB) {
          return EFI_INVALID_PARAMETER;
        }

        Index = (Ptr->SpecificFlag & EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE) ?
                   rtMmio32p : rtMmio32;
      }

      if (Ptr->AddrSpaceGranularity == 64) {
        Index = (Ptr->SpecificFlag & EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE) ?
                   rtMmio64p : rtMmio64;
      }

      break;

    case ACPI_ADDRESS_SPACE_TYPE_IO:
      //
      // Check address range alignment
      //
      if (Ptr->AddrRangeMax != (GetPowerOfTwo64 (Ptr->AddrRangeMax + 1) - 1)) {
        return EFI_INVALID_PARAMETER;
      }

      Index = rtIo32;
      break;

    case ACPI_ADDRESS_SPACE_TYPE_BUS:
    default:
      ASSERT (FALSE);
      break;
    };

    ResNode = &(RootBridgeInstance->ResAllocNode[Index]);
    ResNode->Length  = Ptr->AddrLen;
    ResNode->Alignment = Ptr->AddrRangeMax;
    ResNode->Status  = ResSubmitted;

    Temp += sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) ;
  }

  HostBridgeInstance->ResourceSubmited = TRUE;
  return EFI_SUCCESS;
}

/**
   Returns the proposed resource settings for the specified PCI root bridge.

   This member function returns the proposed resource settings for the specified PCI root bridge. The
   proposed resource settings are prepared when NotifyPhase() is called with a Phase of
   EfiPciHostBridgeAllocateResources. The output parameter Configuration
   specifies the following:
   - The various types of resources, excluding bus resources, that are allocated
   - The associated lengths in terms of ACPI 2.0 resource descriptor format

   @param[in]  This              Pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL instance.
   @param[in]  RootBridgeHandle  The PCI root bridge handle. Type EFI_HANDLE is defined in InstallProtocolInterface() in the UEFI 2.0 Specification.
   @param[out] Configuration     The pointer to the pointer to the PCI I/O and memory resource descriptor.

   @retval EFI_SUCCESS            The requested parameters were returned.
   @retval EFI_INVALID_PARAMETER  RootBridgeHandle is not a valid root bridge handle.
   @retval EFI_DEVICE_ERROR       Programming failed due to a hardware error.
   @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
GetProposedResources (
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL *This,
  IN  EFI_HANDLE                                       RootBridgeHandle,
  OUT VOID                                             **Configuration
  )
{
  LIST_ENTRY                            *List = NULL;
  PCI_HOST_BRIDGE_INSTANCE              *HostBridgeInstance = NULL;
  PCI_ROOT_BRIDGE_INSTANCE              *RootBridgeInstance = NULL;
  UINTN                                 Index = 0;
  VOID                                  *Buffer = NULL;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR     *Descriptor = NULL;
  EFI_ACPI_END_TAG_DESCRIPTOR           *End = NULL;
  UINT64                                ResStatus;

  Buffer = NULL;

  PCIE_DEBUG ("%a: \n", __FUNCTION__);

  //
  // Get the Host Bridge Instance from the resource allocation protocol
  //
  HostBridgeInstance = PCI_HOST_BRIDGE_FROM_THIS (This);
  List = HostBridgeInstance->Head.ForwardLink;

  //
  // Get the corresponding Root Bridge Instance
  //
  while (List != &HostBridgeInstance->Head) {
    RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);

    if (RootBridgeHandle == RootBridgeInstance->RootBridgeHandle) {
      break;
    }

    List = List->ForwardLink;
  }

  if (RootBridgeHandle != RootBridgeInstance->RootBridgeHandle) {
    return EFI_INVALID_PARAMETER;
  }

  Buffer = AllocateZeroPool (
             (rtMaxRes - 1) * sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) +
             sizeof (EFI_ACPI_END_TAG_DESCRIPTOR)
             );
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Descriptor = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *) Buffer;
  for (Index = rtIo16; Index < rtMaxRes; Index++) {
    ResStatus = RootBridgeInstance->ResAllocNode[Index].Status;

    Descriptor->Desc                  = ACPI_ADDRESS_SPACE_DESCRIPTOR;
    Descriptor->Len                   = sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) - 3;;
    Descriptor->GenFlag               = 0;
    Descriptor->AddrRangeMin          = RootBridgeInstance->ResAllocNode[Index].Base;
    Descriptor->AddrLen               = RootBridgeInstance->ResAllocNode[Index].Length;
    Descriptor->AddrRangeMax          = Descriptor->AddrRangeMin + Descriptor->AddrLen - 1;
    Descriptor->AddrTranslationOffset = (ResStatus == ResAllocated) ? EFI_RESOURCE_SATISFIED : PCI_RESOURCE_LESS;

    switch (Index) {
    case rtIo16:
    case rtIo32:
      Descriptor->ResType              = ACPI_ADDRESS_SPACE_TYPE_IO;
      Descriptor->SpecificFlag         = 0;
      Descriptor->AddrSpaceGranularity = 32;
      break;

    case rtMmio32:
    case rtMmio32p:
      Descriptor->ResType              = ACPI_ADDRESS_SPACE_TYPE_MEM;
      Descriptor->SpecificFlag         = (Index == rtMmio32) ? 0 :
                                           EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE;
      Descriptor->AddrSpaceGranularity = 32;
      break;

    case rtMmio64:
    case rtMmio64p:
      Descriptor->ResType              = ACPI_ADDRESS_SPACE_TYPE_MEM;
      Descriptor->SpecificFlag         = (Index == rtMmio64) ? 0 :
                                           EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE;
      Descriptor->AddrSpaceGranularity = 64;
      break;
    };

    PCIE_DEBUG ("Descriptor->ResType:%d\n", Descriptor->ResType);
    PCIE_DEBUG ("Descriptor->Addrlen:%llx\n", Descriptor->AddrLen);
    PCIE_DEBUG ("Descriptor->AddrRangeMax:%llx\n", Descriptor->AddrRangeMax);
    PCIE_DEBUG ("Descriptor->AddrRangeMin:%llx\n", Descriptor->AddrRangeMin);
    PCIE_DEBUG ("Descriptor->SpecificFlag:%llx\n", Descriptor->SpecificFlag);
    PCIE_DEBUG ("Descriptor->AddrTranslationOffset:%d\n", Descriptor->AddrTranslationOffset);
    PCIE_DEBUG ("Descriptor->AddrSpaceGranularity:%d\n", Descriptor->AddrSpaceGranularity);

    Descriptor++;
  }

  //
  // Terminate the entries.
  //
  End = (EFI_ACPI_END_TAG_DESCRIPTOR *) Descriptor;
  End->Desc      = ACPI_END_TAG_DESCRIPTOR;
  End->Checksum  = 0x0;

  *Configuration = Buffer;

  return EFI_SUCCESS;
}

/**
   Provides the hooks from the PCI bus driver to every PCI controller (device/function) at various
   stages of the PCI enumeration process that allow the host bridge driver to preinitialize individual
   PCI controllers before enumeration.

   This function is called during the PCI enumeration process. No specific action is expected from this
   member function. It allows the host bridge driver to preinitialize individual PCI controllers before
   enumeration.

   @param This              Pointer to the EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL instance.
   @param RootBridgeHandle  The associated PCI root bridge handle. Type EFI_HANDLE is defined in
                            InstallProtocolInterface() in the UEFI 2.0 Specification.
   @param PciAddress        The address of the PCI device on the PCI bus. This address can be passed to the
                            EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL member functions to access the PCI
                            configuration space of the device. See Table 12-1 in the UEFI 2.0 Specification for
                            the definition of EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS.
   @param Phase             The phase of the PCI device enumeration.

   @retval EFI_SUCCESS              The requested parameters were returned.
   @retval EFI_INVALID_PARAMETER    RootBridgeHandle is not a valid root bridge handle.
   @retval EFI_INVALID_PARAMETER    Phase is not a valid phase that is defined in
                                    EFI_PCI_CONTROLLER_RESOURCE_ALLOCATION_PHASE.
   @retval EFI_DEVICE_ERROR         Programming failed due to a hardware error. The PCI enumerator should
                                    not enumerate this device, including its child devices if it is a PCI-to-PCI
                                    bridge.

**/
EFI_STATUS
EFIAPI
PreprocessController (
  IN  EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PROTOCOL          *This,
  IN  EFI_HANDLE                                                RootBridgeHandle,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS               PciAddress,
  IN  EFI_PCI_CONTROLLER_RESOURCE_ALLOCATION_PHASE              Phase
  )
{
  PCI_HOST_BRIDGE_INSTANCE              *HostBridgeInstance;
  PCI_ROOT_BRIDGE_INSTANCE              *RootBridgeInstance;
  LIST_ENTRY                            *List;

  HostBridgeInstance = PCI_HOST_BRIDGE_FROM_THIS (This);
  List = HostBridgeInstance->Head.ForwardLink;

  //
  // Enumerate the root bridges in this host bridge
  //
  while (List != &HostBridgeInstance->Head) {

    RootBridgeInstance = ROOT_BRIDGE_FROM_LINK (List);
    if (RootBridgeHandle == RootBridgeInstance->RootBridgeHandle) {
      break;
    }
    List = List->ForwardLink;
  }

  if (List == &HostBridgeInstance->Head) {
    return EFI_INVALID_PARAMETER;
  }

  if ((UINT32) Phase > EfiPciBeforeResourceCollection) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}
