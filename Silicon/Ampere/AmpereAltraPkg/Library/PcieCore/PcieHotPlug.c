/** @file

   Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

   SPDX-License-Identifier: BSD-2-Clause-Patent

 **/

#include <Uefi.h>

#include <Library/ArmSpciLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PcieHotPlug.h>

EFI_GUID PcieHotPlugGuid = HOTPLUG_GUID;
UINT32   HandleId;

VOID
PcieHotPlugStart (
  VOID
  )
{
  ARM_SPCI_ARGS Args;
  EFI_STATUS    Status;

  /* Open handle */
  Status = SpciServiceHandleOpen (SPCI_CLIENT_ID, &HandleId, PcieHotPlugGuid);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SPM failed to return invalid handle. Returned: %d\n",
      Status
      ));

    return;
  }

  /* Set GPIO reset map */
  ZeroMem (&Args, sizeof (Args));
  Args.HandleId = HandleId;
  Args.X1 = GPIOMAP_CMD;
  Args.X2 = PcdGet8(gPcieHotPlugGpioResetMap);
  Status = SpciServiceRequestBlocking (&Args);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SPM HotPlug gpio reset map failed. Returned: %d\n", Status));
  }

  /* Lock Portmap */
  ZeroMem (&Args, sizeof (Args));
  Args.HandleId = HandleId;
  Args.X1 = PORTMAP_LOCK_CMD;
  Status = SpciServiceRequestBlocking (&Args);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SPM HotPlug port map lock failed. Returned: %d\n", Status));
  }

  /* Start HotPlug */
  ZeroMem (&Args, sizeof (Args));
  Args.HandleId = HandleId;
  Args.X1 = HOTPLUG_START_CMD;
  Status = SpciServiceRequestBlocking (&Args);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SPM HotPlug start failed. Returned: %d\n", Status));
  }

  /* Close handle */
  Status = SpciServiceHandleClose (HandleId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SPM HotPlug close handle failed. Returned: %d\n", Status));
  }
}
