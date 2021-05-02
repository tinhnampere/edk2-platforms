/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/EventGroup.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/FailSafeLib.h>
#include <Library/NVParamLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include "FailSafe.h"
#include "Watchdog.h"

STATIC UINTN                            gWatchdogOSTimeout;
STATIC BOOLEAN                          gFailSafeOff;
STATIC EFI_WATCHDOG_TIMER_ARCH_PROTOCOL *gWatchdogTimer;

EFI_STATUS
EFIAPI
FailSafeTestBootFailure (
  VOID
  );

STATIC VOID
FailSafeTurnOff (
  VOID
  )
{
  EFI_STATUS Status;

  if (IsFailSafeOff ()) {
    return;
  }

  Status = FailSafeBootSuccessfully ();
  ASSERT_EFI_ERROR (Status);

  gFailSafeOff = TRUE;

  /* Disable Watchdog timer */
  gWatchdogTimer->SetTimerPeriod (gWatchdogTimer, 0);
}

BOOLEAN
EFIAPI
IsFailSafeOff (
  VOID
  )
{
  return gFailSafeOff;
}

/**
  The function to disable Watchdog timer when enter Setup screen
 **/
VOID
WdtTimerEnterSetupScreenCallback (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  /* Make sure FailSafe is turned off */
  FailSafeTurnOff ();
}

/**
  The function to refresh Watchdog timer in the event before booting
 **/
VOID
WdtTimerBeforeBootCallback (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  /*
   * At this point, the system is considered boot successfully to BIOS
   */
  FailSafeTurnOff ();

  /*
   * It is BIOS's responsibility to setup Watchdog when load an EFI application
   * after this step
   */
}

/**
  The function to refresh Watchdog timer in the event before exiting boot services
 **/
VOID
WdtTimerExitBootServiceCallback (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{

  /* Enable Watchdog timer for OS booting */
  if (gWatchdogOSTimeout != 0) {
    gWatchdogTimer->SetTimerPeriod (
                      gWatchdogTimer,
                      gWatchdogOSTimeout * TIME_UNITS_PER_SECOND
                      );
  } else {
    /* Disable Watchdog timer */
    gWatchdogTimer->SetTimerPeriod (gWatchdogTimer, 0);
  }
}

/**
  This function is a hook called when user loads the manufacturing
  or optimal defaults.

  @param Defaults : (NVRAM_VARIABLE *)optimal or manufacturing
  @Data           : Messagebox

  @retval VOID
**/
VOID
LoadNVRAMDefaultConfig (
  IN VOID  *Defaults,
  IN UINTN Data
  )
{
  NVParamClrAll ();
}

/**
  Main entry for this driver.

  @param ImageHandle     Image handle this driver.
  @param SystemTable     Pointer to SystemTable.

  @retval EFI_SUCESS     This function always complete successfully.

**/
EFI_STATUS
EFIAPI
FailSafeDxeEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_EVENT  ExitBootServicesEvent;
  EFI_STATUS Status;

  gFailSafeOff = FALSE;

  FailSafeTestBootFailure ();

  /* We need to setup non secure Watchdog to ensure that the system will
   * boot to OS successfully.
   *
   * The BIOS doesn't handle Watchdog interrupt so we expect WS1 asserted EL3
   * when Watchdog timeout triggered
   */

  Status = WatchdogTimerInstallProtocol (&gWatchdogTimer);
  ASSERT_EFI_ERROR (Status);

  // We should register a callback function before entering to Setup screen
  // rather than always call it at DXE phase.
  FailSafeTurnOff ();

  /* Register event before exit boot services */
  Status = gBS->CreateEvent (
                  EVT_SIGNAL_EXIT_BOOT_SERVICES,
                  TPL_NOTIFY,
                  WdtTimerExitBootServiceCallback,
                  NULL,
                  &ExitBootServicesEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}
