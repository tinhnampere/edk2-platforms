/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Uefi/UefiSpec.h>
#include <Guid/EventGroup.h>
#include <Guid/MemoryAttributesTable.h>
#include <Protocol/Cpu.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_CPU_ARCH_PROTOCOL             *mCpu;

/**
  Notify function for event group EVT_SIGNAL_EXIT_BOOT_SERVICES.

  @param[in]  Event   The Event that is being processed.
  @param[in]  Context The Event Context.

**/
VOID
EFIAPI
OnExitBootServices(
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  EFI_STATUS                    Status;
  EFI_MEMORY_ATTRIBUTES_TABLE   *MemoryAttributesTable;
  EFI_MEMORY_DESCRIPTOR         *Desc;
  UINTN                         Index;

  DEBUG ((DEBUG_INFO, "%a:%d +\n", __FUNCTION__, __LINE__));

  //
  // Close the event, so it will not be signalled again.
  //
  gBS->CloseEvent (Event);

  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **) &mCpu);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a:%d -\n", __FUNCTION__, __LINE__));
    return;
  }

  Status = EfiGetSystemConfigurationTable (&gEfiMemoryAttributesTableGuid, (VOID **) &MemoryAttributesTable);
  if (EFI_ERROR (Status) || MemoryAttributesTable == NULL) {
    DEBUG ((DEBUG_INFO, "%a:%d -\n", __FUNCTION__, __LINE__));
    return;
  }

  Desc = (EFI_MEMORY_DESCRIPTOR *) ((UINT64) MemoryAttributesTable + sizeof (EFI_MEMORY_ATTRIBUTES_TABLE));
  for (Index = 0; Index < MemoryAttributesTable->NumberOfEntries; Index++) {
    if (Desc->Type != EfiRuntimeServicesCode && Desc->Type != EfiRuntimeServicesData) {
      Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT64) Desc + MemoryAttributesTable->DescriptorSize);
      continue;
    }

    if (!(Desc->Attribute & (EFI_MEMORY_RO | EFI_MEMORY_XP))) {
      Desc->Attribute |= EFI_MEMORY_XP;
      mCpu->SetMemoryAttributes (mCpu, Desc->PhysicalStart, EFI_PAGES_TO_SIZE (Desc->NumberOfPages), Desc->Attribute);
      DEBUG ((DEBUG_INFO, "%a: Set memory attribute, Desc->PhysicalStart=0x%X, size=%d, Attributes=0x%X\n",
                          __FUNCTION__, Desc->PhysicalStart, EFI_PAGES_TO_SIZE (Desc->NumberOfPages), Desc->Attribute));
    }
    Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT64) Desc + MemoryAttributesTable->DescriptorSize);
  }

  DEBUG ((DEBUG_INFO, "%a:%d -\n", __FUNCTION__, __LINE__));
}

EFI_STATUS
EFIAPI
FixupMemoryMapInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_EVENT             ExitBootServicesEvent;
  EFI_STATUS            Status;

  Status = gBS->CreateEvent (
                  EVT_SIGNAL_EXIT_BOOT_SERVICES,
                  TPL_NOTIFY,
                  OnExitBootServices,
                  NULL,
                  &ExitBootServicesEvent);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
