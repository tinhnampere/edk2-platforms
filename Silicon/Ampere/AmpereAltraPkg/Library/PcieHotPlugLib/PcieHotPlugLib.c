/** @file

   Copyright (c) 2021-2022, Ampere Computing LLC. All rights reserved.<BR>

   SPDX-License-Identifier: BSD-2-Clause-Patent

 **/

#include <Uefi.h>

#include <Library/ArmSpciLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PcieHotPlugLib.h>
#include <Library/PcieHotPlugPortMapLib.h>

#define END_PORTMAP_ENTRY 0xFF

// SPM takes up to 4 arguments as value for SPCI call (Args.X2->Args.X5).
#define MAX_MSG_CMD_ARGS  4

EFI_GUID PcieHotPlugGuid = HOTPLUG_GUID;
UINT32   HandleId;

/**
  Set GPIO pins used for PCIe reset. This command
  limits the number of GPIO[16:21] for reset purpose.
**/
VOID
PcieHotPlugSetGpioMap (
  VOID
  )
{
  ARM_SPCI_ARGS Args;
  EFI_STATUS    Status;

  Args.HandleId = HandleId;
  Args.X1 = GPIOMAP_CMD;
  Args.X2 = (UINTN)PcdGet8 (PcdPcieHotPlugGpioResetMap);

  Status = SpciServiceRequestBlocking (&Args);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SPM HotPlug GPIO reset map failed. Returned: %r\n", Status));
  }
}

/**
  Lock current Portmap table.
**/
VOID
PcieHotPlugSetLockPortMap (
  VOID
  )
{
  ARM_SPCI_ARGS Args;
  EFI_STATUS    Status;

  Args.HandleId = HandleId;
  Args.X1 = PORTMAP_LOCK_CMD;

  Status = SpciServiceRequestBlocking (&Args);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SPM HotPlug port map lock failed. Returned: %r\n", Status));
  }
}

/**
  Start Hot plug service.
**/
VOID
PcieHotPlugSetStart (
  VOID
  )
{
  ARM_SPCI_ARGS Args;
  EFI_STATUS    Status;

  Args.HandleId = HandleId;
  Args.X1 = HOTPLUG_START_CMD;

  Status = SpciServiceRequestBlocking (&Args);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SPM HotPlug start failed. Returned: %r\n", Status));
  }
}

/**
  Clear current configuration of Portmap table.
**/
VOID
PcieHotPlugSetClear (
  VOID
  )
{
  ARM_SPCI_ARGS Args;
  EFI_STATUS    Status;

  Args.HandleId = HandleId;
  Args.X1 = PORTMAP_CLR_CMD;

  Status = SpciServiceRequestBlocking (&Args);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SPM HotPlug clear port map failed. Returned: %r\n", Status));
  }
}

/**
  Set configuration for Portmap table.
**/
VOID
PcieHotPlugSetPortMap (
  VOID
  )
{
  ARM_SPCI_ARGS Args;
  EFI_STATUS    Status;

  PCIE_HOTPLUG_PORTMAP_TABLE  *PortMapTable;
  PCIE_HOTPLUG_PORTMAP_ENTRY  PortMapEntry;
  UINT8                       Index;
  UINT8                       PortMapEntryIndex;
  BOOLEAN                     IsEndPortMapEntry;
  UINTN                       ConfigValue;
  UINTN                       *ConfigLegacy;

  // Retrieves PCD of Portmap table.
  PortMapTable = (PCIE_HOTPLUG_PORTMAP_TABLE *)PcdGetPtr (PcdPcieHotPlugPortMapTable);

  //
  // Check whether specific platform configuration is used?
  // Otherwise, keep default configuration (Mt. Jade 2U).
  //
  if (!PortMapTable->UseDefaultConfig) {
    IsEndPortMapEntry = FALSE;
    PortMapEntryIndex = 0;

    // Clear old Port Map table first.
    PcieHotPlugSetClear ();

    while (!IsEndPortMapEntry) {
      ZeroMem (&Args, sizeof (Args));
      Args.HandleId = HandleId;
      Args.X1 = PORTMAP_SET_CMD;

      // Pointer will get configuration value for Args.X2->Args.X5
      ConfigLegacy = &Args.X2;

      for (Index = 0; Index < MAX_MSG_CMD_ARGS; Index++) {
        PortMapEntry = *((PCIE_HOTPLUG_PORTMAP_ENTRY *)PortMapTable->PortMap[PortMapEntryIndex]);
        ConfigValue = PCIE_HOT_PLUG_GET_CONFIG_VALUE (PortMapEntry);
        *ConfigLegacy = ConfigValue;

        if (PortMapTable->PortMap[PortMapEntryIndex][0] == END_PORTMAP_ENTRY) {
          IsEndPortMapEntry = TRUE;
          break;
        }

        PortMapEntryIndex++;
        ConfigLegacy++;
      }

      Status = SpciServiceRequestBlocking (&Args);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "SPM HotPlug set port map failed. Returned: %r\n", Status));
      }
    }
  }
}

/**
  This function will start Hotplug service after following steps:
  - Open handle to make a SPCI call.
  - Set GPIO pins for PCIe reset.
  - Set configuration for Portmap table.
  - Lock current Portmap table.
  - Start Hot plug service.
  - Close handle.
**/
VOID
PcieHotPlugStart (
  VOID
  )
{
  EFI_STATUS Status;

  /* Open handle */
  Status = SpciServiceHandleOpen (SPCI_CLIENT_ID, &HandleId, PcieHotPlugGuid);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SPM failed to return invalid handle. Returned: %r\n",
      Status
      ));

    return;
  }

  /* Set GPIO pins for PCIe reset. */
  PcieHotPlugSetGpioMap ();

  /* Set Portmap table */
  PcieHotPlugSetPortMap ();

  /* Lock current Portmap table*/
  PcieHotPlugSetLockPortMap ();

  /* Start Hot plug service*/
  PcieHotPlugSetStart ();

  /* Close handle */
  Status = SpciServiceHandleClose (HandleId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SPM HotPlug close handle failed. Returned: %r\n", Status));
  }
}
