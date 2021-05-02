/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/FlashLib.h>
#include <Library/NVParamLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <NVParamDef.h>

#include "FailSafe.h"
#include "Watchdog.h"

STATIC UINTN                            gWatchdogOSTimeout;
STATIC BOOLEAN                          gFailSafeOff;
STATIC EFI_WATCHDOG_TIMER_ARCH_PROTOCOL *gWatchdogTimer;

STATIC
INTN
CheckCrc16 (
  UINT8 *Pointer,
  INTN  Count
  )
{
  INTN Crc = 0;
  INTN Index;

  while (--Count >= 0) {
    Crc = Crc ^ (INTN)*Pointer++ << 8;
    for (Index = 0; Index < 8; ++Index) {
      if ((Crc & 0x8000) != 0) {
        Crc = Crc << 1 ^ 0x1021;
      } else {
        Crc = Crc << 1;
      }
    }
  }

  return Crc & 0xFFFF;
}

BOOLEAN
FailSafeValidCRC (
  FAIL_SAFE_CONTEXT *FailSafeBuf
  )
{
  UINT8  Valid;
  UINT16 Crc;
  UINT32 Len;

  Len = sizeof (FAIL_SAFE_CONTEXT);
  Crc = FailSafeBuf->CRC16;
  FailSafeBuf->CRC16 = 0;

  Valid = (Crc == CheckCrc16 ((UINT8 *)FailSafeBuf, Len));
  FailSafeBuf->CRC16 = Crc;

  return Valid;
}

BOOLEAN
FailSafeFailureStatus (
  UINT8 Status
  )
{
  if ((Status == FAILSAFE_BOOT_LAST_KNOWN_SETTINGS) ||
      (Status == FAILSAFE_BOOT_DEFAULT_SETTINGS) ||
      (Status == FAILSAFE_BOOT_DDR_DOWNGRADE)) {
    return TRUE;
  }

  return FALSE;
}

EFI_STATUS
EFIAPI
FailSafeBootSuccessfully (
  VOID
  )
{
  EFI_STATUS                    Status;
  FAIL_SAFE_CONTEXT             FailSafeBuf;
  UINT32                        FailSafeSize;
  UINT64                        FailSafeStartOffset;

  Status = FlashGetFailSafeInfo (&FailSafeStartOffset, &FailSafeSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get context region information\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  Status = FlashReadCommand (FailSafeStartOffset, (UINT8 *)&FailSafeBuf, sizeof (FAIL_SAFE_CONTEXT));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // If failsafe context is valid, and:
  //    - The status indicate non-failure, then don't clear it
  //    - The status indicate a failure, then go and clear it
  //
  if (FailSafeValidCRC (&FailSafeBuf)
      && !FailSafeFailureStatus (FailSafeBuf.Status)) {
    return EFI_SUCCESS;
  }

  Status = FlashEraseCommand (FailSafeStartOffset, FailSafeSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
FailSafeTestBootFailure (
  VOID
  )
{
  EFI_STATUS Status;
  UINT32     Value = 0;

  //
  // Simulate UEFI boot failure due to config wrong NVPARAM for
  // testing failsafe feature
  //
  Status = NVParamGet (NV_SI_UEFI_FAILURE_FAILSAFE, NV_PERM_ALL, &Value);
  if (!EFI_ERROR (Status) && (Value == 1)) {
    CpuDeadLoop ();
  }

  return EFI_SUCCESS;
}

VOID
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
  Main entry for this driver.

  @param ImageHandle     Image handle this driver.
  @param SystemTable     Pointer to SystemTable.

  @retval EFI_SUCCESS    This function always complete successfully.

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
