/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/ArmGenericTimerCounterLib.h>
#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/HardwareInterrupt2.h>

#include "FailSafe.h"
#include "Watchdog.h"

/* Watchdog timer controller registers */
#define WDT_CTRL_BASE_REG                     FixedPcdGet64 (PcdGenericWatchdogControlBase)
#define WDT_CTRL_WCS_OFF                      0x0
#define WDT_CTRL_WCS_ENABLE_MASK              0x1
#define WDT_CTRL_WOR_OFF                      0x8
#define WDT_CTRL_WCV_OFF                      0x10
#define WS0_INTERRUPT_SOURCE                  FixedPcdGet32 (PcdGenericWatchdogEl2IntrNum)

STATIC UINT64                           mNumTimerTicks;
STATIC EFI_HARDWARE_INTERRUPT2_PROTOCOL *mInterruptProtocol;
BOOLEAN                                 mInterruptWS0Enabled;

STATIC
VOID
WatchdogTimerWriteOffsetRegister (
  UINT32 Value
  )
{
  MmioWrite32 (WDT_CTRL_BASE_REG + WDT_CTRL_WOR_OFF, Value);
}

STATIC
VOID
WatchdogTimerWriteCompareRegister (
  UINT64 Value
  )
{
  MmioWrite64 (WDT_CTRL_BASE_REG + WDT_CTRL_WCV_OFF, Value);
}

STATIC
EFI_STATUS
WatchdogTimerEnable (
  IN BOOLEAN Enable
  )
{
  UINT32 Val =  MmioRead32 ((UINTN)(WDT_CTRL_BASE_REG + WDT_CTRL_WCS_OFF));

  if (Enable) {
    Val |= WDT_CTRL_WCS_ENABLE_MASK;
  } else {
    Val &= ~WDT_CTRL_WCS_ENABLE_MASK;
  }
  MmioWrite32 ((UINTN)(WDT_CTRL_BASE_REG + WDT_CTRL_WCS_OFF), Val);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
WatchdogTimerSetup (
  VOID
  )
{
  EFI_STATUS Status;

  /* Disable Watchdog timer */
  WatchdogTimerEnable (FALSE);

  if (!mInterruptWS0Enabled) {
    Status = mInterruptProtocol->EnableInterruptSource (
                                   mInterruptProtocol,
                                   WS0_INTERRUPT_SOURCE
                                   );
    ASSERT_EFI_ERROR (Status);

    mInterruptWS0Enabled = TRUE;
  }

  if (mNumTimerTicks == 0) {
    return EFI_SUCCESS;
  }

  /* If the number of required ticks is greater than the max the Watchdog's
     offset register (WOR) can hold, we need to manually compute and set
     the compare register (WCV) */
  if (mNumTimerTicks > MAX_UINT32) {
    /* We need to enable the Watchdog *before* writing to the compare register,
       because enabling the Watchdog causes an "explicit refresh", which
       clobbers the compare register (WCV). In order to make sure this doesn't
       trigger an interrupt, set the offset to max. */
    WatchdogTimerWriteOffsetRegister (MAX_UINT32);
    WatchdogTimerEnable (TRUE);
    WatchdogTimerWriteCompareRegister (ArmGenericTimerGetSystemCount () + mNumTimerTicks);
  } else {
    WatchdogTimerWriteOffsetRegister ((UINT32)mNumTimerTicks);
    WatchdogTimerEnable (TRUE);
  }

  return EFI_SUCCESS;
}


/* This function is called when the Watchdog's first signal (WS0) goes high.
   It uses the ResetSystem Runtime Service to reset the board.
*/
VOID
EFIAPI
WatchdogTimerInterruptHandler (
  IN HARDWARE_INTERRUPT_SOURCE Source,
  IN EFI_SYSTEM_CONTEXT        SystemContext
  )
{
  STATIC CONST CHAR16 ResetString[]= L"The generic Watchdog timer ran out.";

  mInterruptProtocol->EndOfInterrupt (mInterruptProtocol, Source);

  if (!IsFailSafeOff ()) {
    /* Not handling interrupt as ATF is monitoring it */
    return;
  }

  WatchdogTimerEnable (FALSE);

  gRT->ResetSystem (
         EfiResetCold,
         EFI_TIMEOUT,
         StrSize (ResetString),
         (VOID *)&ResetString
         );

  /* If we got here then the reset didn't work */
  ASSERT (FALSE);
}

/**
  This function registers the handler NotifyFunction so it is called every time
  the Watchdog timer expires.  It also passes the amount of time since the last
  handler call to the NotifyFunction.
  If NotifyFunction is not NULL and a handler is not already registered,
  then the new handler is registered and EFI_SUCCESS is returned.
  If NotifyFunction is NULL, and a handler is already registered,
  then that handler is unregistered.
  If an attempt is made to register a handler when a handler is already
  registered, then EFI_ALREADY_STARTED is returned.
  If an attempt is made to unregister a handler when a handler is not
  registered, then EFI_INVALID_PARAMETER is returned.

  @param  This             The EFI_TIMER_ARCH_PROTOCOL instance.
  @param  NotifyFunction   The function to call when a timer interrupt fires.
                           This function executes at TPL_HIGH_LEVEL. The DXE
                           Core will register a handler for the timer interrupt,
                           so it can know how much time has passed. This
                           information is used to signal timer based events.
                           NULL will unregister the handler.

  @retval EFI_UNSUPPORTED       The code does not support NotifyFunction.

**/
EFI_STATUS
EFIAPI
WatchdogTimerRegisterHandler (
  IN CONST EFI_WATCHDOG_TIMER_ARCH_PROTOCOL *This,
  IN       EFI_WATCHDOG_TIMER_NOTIFY        NotifyFunction
  )
{
  /* Not support. Watchdog will reset the board */
  return EFI_UNSUPPORTED;
}

/**
  This function sets the amount of time to wait before firing the Watchdog
  timer to TimerPeriod 100ns units.  If TimerPeriod is 0, then the Watchdog
  timer is disabled.

  @param  This             The EFI_WATCHDOG_TIMER_ARCH_PROTOCOL instance.
  @param  TimerPeriod      The amount of time in 100ns units to wait before
                           the Watchdog timer is fired. If TimerPeriod is zero,
                           then the Watchdog timer is disabled.

  @retval EFI_SUCCESS           The Watchdog timer has been programmed to fire
                                in Time  100ns units.
  @retval EFI_DEVICE_ERROR      A Watchdog timer could not be programmed due
                                to a device error.

**/
EFI_STATUS
EFIAPI
WatchdogTimerSetPeriod (
  IN CONST EFI_WATCHDOG_TIMER_ARCH_PROTOCOL *This,
  IN       UINT64                           TimerPeriod   // In 100ns units
  )
{
  mNumTimerTicks  = (ArmGenericTimerGetTimerFreq () * TimerPeriod) / TIME_UNITS_PER_SECOND;

  if (!IsFailSafeOff ()) {
    /* Not support Watchdog timer service until FailSafe is off as ATF is monitoring it */
    return EFI_SUCCESS;
  }

  return WatchdogTimerSetup ();
}

/**
  This function retrieves the period of timer interrupts in 100ns units,
  returns that value in TimerPeriod, and returns EFI_SUCCESS.  If TimerPeriod
  is NULL, then EFI_INVALID_PARAMETER is returned.  If a TimerPeriod of 0 is
  returned, then the timer is currently disabled.

  @param  This             The EFI_TIMER_ARCH_PROTOCOL instance.
  @param  TimerPeriod      A pointer to the timer period to retrieve in
                           100ns units. If 0 is returned, then the timer is
                           currently disabled.


  @retval EFI_SUCCESS           The timer period was returned in TimerPeriod.
  @retval EFI_INVALID_PARAMETER TimerPeriod is NULL.

**/
EFI_STATUS
EFIAPI
WatchdogTimerGetPeriod (
  IN CONST EFI_WATCHDOG_TIMER_ARCH_PROTOCOL *This,
  OUT      UINT64                           *TimerPeriod
  )
{
  if (TimerPeriod == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *TimerPeriod = ((TIME_UNITS_PER_SECOND / ArmGenericTimerGetTimerFreq ()) * mNumTimerTicks);

  return EFI_SUCCESS;
}

/**
  Interface structure for the Watchdog Architectural Protocol.

  @par Protocol Description:
  This protocol provides a service to set the amount of time to wait
  before firing the Watchdog timer, and it also provides a service to
  register a handler that is invoked when the Watchdog timer fires.

  @par When the Watchdog timer fires, control will be passed to a handler
  if one has been registered.  If no handler has been registered,
  or the registered handler returns, then the system will be
  reset by calling the Runtime Service ResetSystem().

  @param RegisterHandler
  Registers a handler that will be called each time the
  Watchdogtimer interrupt fires.  TimerPeriod defines the minimum
  time between timer interrupts, so TimerPeriod will also
  be the minimum time between calls to the registered
  handler.
  NOTE: If the Watchdog resets the system in hardware, then
        this function will not have any chance of executing.

  @param SetTimerPeriod
  Sets the period of the timer interrupt in 100ns units.
  This function is optional, and may return EFI_UNSUPPORTED.
  If this function is supported, then the timer period will
  be rounded up to the nearest supported timer period.

  @param GetTimerPeriod
  Retrieves the period of the timer interrupt in 100ns units.

**/
STATIC EFI_WATCHDOG_TIMER_ARCH_PROTOCOL gWatchdogTimer = {
  (EFI_WATCHDOG_TIMER_REGISTER_HANDLER)WatchdogTimerRegisterHandler,
  (EFI_WATCHDOG_TIMER_SET_TIMER_PERIOD)WatchdogTimerSetPeriod,
  (EFI_WATCHDOG_TIMER_GET_TIMER_PERIOD)WatchdogTimerGetPeriod
};

EFI_STATUS
EFIAPI
WatchdogTimerInstallProtocol (
  EFI_WATCHDOG_TIMER_ARCH_PROTOCOL **WatchdogTimerProtocol
  )
{
  EFI_STATUS Status;
  EFI_HANDLE Handle;
  EFI_TPL    CurrentTpl;

  /* Make sure the Watchdog Timer Architectural Protocol has not been installed
     in the system yet.
     This will avoid conflicts with the universal Watchdog */
  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gEfiWatchdogTimerArchProtocolGuid);

  ASSERT (ArmGenericTimerGetTimerFreq () != 0);

  /* Install interrupt handler */
  Status = gBS->LocateProtocol (
                  &gHardwareInterrupt2ProtocolGuid,
                  NULL,
                  (VOID **)&mInterruptProtocol
                  );
  ASSERT_EFI_ERROR (Status);

  /*
   * We don't want to be interrupted while registering Watchdog interrupt source as the interrupt
   * may be trigger in the middle because the interrupt line already enabled in the EL3.
   */
  CurrentTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);

  Status = mInterruptProtocol->RegisterInterruptSource (
                                 mInterruptProtocol,
                                 WS0_INTERRUPT_SOURCE,
                                 WatchdogTimerInterruptHandler
                                 );
  ASSERT_EFI_ERROR (Status);

  /* Don't enable interrupt until FailSafe off */
  mInterruptWS0Enabled = FALSE;
  Status = mInterruptProtocol->DisableInterruptSource (
                                 mInterruptProtocol,
                                 WS0_INTERRUPT_SOURCE
                                 );
  ASSERT_EFI_ERROR (Status);

  gBS->RestoreTPL (CurrentTpl);

  Status = mInterruptProtocol->SetTriggerType (
                                 mInterruptProtocol,
                                 WS0_INTERRUPT_SOURCE,
                                 EFI_HARDWARE_INTERRUPT2_TRIGGER_LEVEL_HIGH
                                 );
  ASSERT_EFI_ERROR (Status);

  /* Install the Timer Architectural Protocol onto a new handle */
  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiWatchdogTimerArchProtocolGuid,
                  &gWatchdogTimer,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  mNumTimerTicks = 0;

  if (WatchdogTimerProtocol != NULL) {
    *WatchdogTimerProtocol = &gWatchdogTimer;
  }

  return Status;
}
