/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi.h>

#include <Guid/EventGroup.h>
#include <Library/ArmGenericTimerCounterLib.h>
#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/RealTimeClockLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/RealTimeClock.h>

#include "PCF85063.h"

#define TICKS_PER_SEC     (ArmGenericTimerGetTimerFreq ())

#define TIMEZONE_0        0

/**
 * Define EPOCH (1970-JANUARY-01) in the Julian Date representation
 */
#define EPOCH_JULIAN_DATE 2440588

/**
 * Seconds per unit
 */
#define SEC_PER_MIN                    ((UINTN)    60)
#define SEC_PER_HOUR                   ((UINTN)  3600)
#define SEC_PER_DAY                    ((UINTN) 86400)

#define SEC_PER_MONTH                  ((UINTN)  2,592,000)
#define SEC_PER_YEAR                   ((UINTN) 31,536,000)

STATIC EFI_RUNTIME_SERVICES   *mRT;
STATIC EFI_EVENT              mVirtualAddressChangeEvent = NULL;

STATIC UINT64                 mLastSavedSystemCount = 0;
STATIC UINT64                 mLastSavedTimeEpoch = 0;
STATIC CONST CHAR16           mTimeZoneVariableName[] = L"RtcTimeZone";
STATIC CONST CHAR16           mDaylightVariableName[] = L"RtcDaylight";

STATIC
BOOLEAN
IsLeapYear (
  IN EFI_TIME   *Time
  )
{
  if (Time->Year % 4 == 0) {
    if (Time->Year % 100 == 0) {
      if (Time->Year % 400 == 0) {
        return TRUE;
      } else {
        return FALSE;
      }
    } else {
      return TRUE;
    }
  } else {
    return FALSE;
  }
}

STATIC
BOOLEAN
DayValid (
  IN  EFI_TIME  *Time
  )
{
  INTN  DayOfMonth[12];

  DayOfMonth[0] = 31;
  DayOfMonth[1] = 29;
  DayOfMonth[2] = 31;
  DayOfMonth[3] = 30;
  DayOfMonth[4] = 31;
  DayOfMonth[5] = 30;
  DayOfMonth[6] = 31;
  DayOfMonth[7] = 31;
  DayOfMonth[8] = 30;
  DayOfMonth[9] = 31;
  DayOfMonth[10] = 30;
  DayOfMonth[11] = 31;

  if (Time->Day < 1 ||
      Time->Day > DayOfMonth[Time->Month - 1] ||
      (Time->Month == 2 && (!IsLeapYear (Time) && Time->Day > 28))
     ) {
    return FALSE;
  }

  return TRUE;
}

STATIC
EFI_STATUS
RtcTimeFieldsValid (
  IN EFI_TIME *Time
  )
{
  if (Time->Month < 1 ||
      Time->Month > 12 ||
      (!DayValid (Time)) ||
      Time->Hour > 23 ||
      Time->Minute > 59 ||
      Time->Second > 59 ||
      Time->Nanosecond > 999999999 ||
      (!(Time->TimeZone == EFI_UNSPECIFIED_TIMEZONE || (Time->TimeZone >= -1440 && Time->TimeZone <= 1440))) ||
      ((Time->Daylight & (~(EFI_TIME_ADJUST_DAYLIGHT | EFI_TIME_IN_DAYLIGHT))) != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

UINT8
Bin2Bcd (
  UINT32 Val
  )
{
  return (((Val / 10) << 4) | (Val % 10));
}

/**
 * Converts EFI_TIME to Epoch seconds (elapsed since 1970 JANUARY 01, 00:00:00 UTC)
 */
STATIC UINTN
EfiTimeToEpoch (
  IN  EFI_TIME  *Time
  )
{
  UINTN a;
  UINTN y;
  UINTN m;
  UINTN JulianDate;
  UINTN EpochDays;
  UINTN EpochSeconds;

  a = (14 - Time->Month) / 12 ;
  y = Time->Year + 4800 - a;
  m = Time->Month + (12*a) - 3;

  JulianDate = Time->Day + ((153*m + 2)/5) + (365*y) + (y/4) - (y/100) + (y/400) - 32045;

  ASSERT (JulianDate > EPOCH_JULIAN_DATE);
  EpochDays = JulianDate - EPOCH_JULIAN_DATE;

  EpochSeconds = (EpochDays * SEC_PER_DAY) + ((UINTN)Time->Hour * SEC_PER_HOUR) + (Time->Minute * SEC_PER_MIN) + Time->Second;

  return EpochSeconds;
}

/**
 * Converts Epoch seconds (elapsed since 1970 JANUARY 01, 00:00:00 UTC) to EFI_TIME
 */
STATIC
VOID
EpochToEfiTime (
  IN  UINTN     EpochSeconds,
  OUT EFI_TIME  *Time
  )
{
  INTN         a;
  INTN         b;
  INTN         c;
  INTN         d;
  INTN         g;
  INTN         j;
  INTN         m;
  INTN         y;
  INTN         da;
  INTN         db;
  INTN         dc;
  INTN         dg;
  INTN         hh;
  INTN         mm;
  INTN         ss;
  INTN         J;

  J  = (EpochSeconds / 86400) + 2440588;
  j  = J + 32044;
  g  = j / 146097;
  dg = j % 146097;
  c  = (((dg / 36524) + 1) * 3) / 4;
  dc = dg - (c * 36524);
  b  = dc / 1461;
  db = dc % 1461;
  a  = (((db / 365) + 1) * 3) / 4;
  da = db - (a * 365);
  y  = (g * 400) + (c * 100) + (b * 4) + a;
  m  = (((da * 5) + 308) / 153) - 2;
  d  = da - (((m + 4) * 153) / 5) + 122;

  Time->Year  = y - 4800 + ((m + 2) / 12);
  Time->Month = ((m + 2) % 12) + 1;
  Time->Day   = d + 1;

  ss = EpochSeconds % 60;
  a  = (EpochSeconds - ss) / 60;
  mm = a % 60;
  b = (a - mm) / 60;
  hh = b % 24;

  Time->Hour        = hh;
  Time->Minute      = mm;
  Time->Second      = ss;
  Time->Nanosecond  = 0;
}

/**
 * Returns the current time and date information, and the time-keeping capabilities
 * of the hardware platform.
 *
 * @param  Time                  A pointer to storage to receive a snapshot of the current time.
 * @param  Capabilities          An optional pointer to a buffer to receive the real time clock
 *                               device's capabilities.
 *
 *
 * @retval EFI_SUCCESS           The operation completed successfully.
 * @retval EFI_INVALID_PARAMETER Time is NULL.
 * @retval EFI_DEVICE_ERROR      The time could not be retrieved due to hardware error.
 */
EFI_STATUS
EFIAPI
LibGetTime (
  OUT EFI_TIME                *Time,
  OUT  EFI_TIME_CAPABILITIES  *Capabilities
  )

{
  EFI_STATUS    Status;
  UINT64        CurrentSystemCount;
  UINT64        TimeElapsed;
  INT16         TimeZone;
  UINT8         Daylight;
  UINTN         Size;
  UINTN         EpochSeconds;

  if (Time == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((mLastSavedTimeEpoch == 0) || EfiAtRuntime ()) {
    /* SMPro requires physical address for message communication */
    Status = PlatformGetTime (Time);
    if (EFI_ERROR (Status)) {
      /* Failed to read platform RTC so create fake time */
      Time->Second = 0;
      Time->Minute = 0;
      Time->Hour = 10;
      Time->Day = 1;
      Time->Month = 1;
      Time->Year = 2017;
    }
    if (!EfiAtRuntime ()) {
      mLastSavedTimeEpoch = EfiTimeToEpoch (Time);
      mLastSavedSystemCount = ArmGenericTimerGetSystemCount ();
    }
    EpochSeconds = EfiTimeToEpoch (Time);
  } else {
    CurrentSystemCount = ArmGenericTimerGetSystemCount ();
    if (CurrentSystemCount >= mLastSavedSystemCount) {
      TimeElapsed = (CurrentSystemCount - mLastSavedSystemCount) / MultU64x32 (1, TICKS_PER_SEC);
      EpochSeconds = mLastSavedTimeEpoch + TimeElapsed;
    } else {
      /* System counter overflow 64 bits */
      /* Call GetTime again to read the date from RTC HW, not using generic timer system counter */
      mLastSavedTimeEpoch = 0;
      return LibGetTime (Time, Capabilities);
    }
  }

  /* Get the current time zone information from non-volatile storage */
  Size = sizeof (TimeZone);
  Status = mRT->GetVariable (
                  (CHAR16 *) mTimeZoneVariableName,
                  &gEfiCallerIdGuid,
                  NULL,
                  &Size,
                  (VOID *) &TimeZone
                  );
  if (EFI_ERROR (Status)) {
    /* The time zone variable does not exist in non-volatile storage, so create it. */
    Time->TimeZone = TIMEZONE_0;
    /* Store it */
    Status = mRT->SetVariable (
                    (CHAR16 *) mTimeZoneVariableName,
                    &gEfiCallerIdGuid,
                    EFI_VARIABLE_NON_VOLATILE
                      | EFI_VARIABLE_BOOTSERVICE_ACCESS
                      | EFI_VARIABLE_RUNTIME_ACCESS,
                    Size,
                    (VOID *) &(Time->TimeZone)
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to save %s variable to non-volatile storage, Status = %r\n",
        __FUNCTION__,
        mTimeZoneVariableName,
        Status
        ));
      return Status;
    }
  } else {
    /* Got the time zone */
    Time->TimeZone = TimeZone;

    /* Check TimeZone bounds:   -1440 to 1440 or 2047 */
    if (((Time->TimeZone < -1440) || (Time->TimeZone > 1440))
        && (Time->TimeZone != EFI_UNSPECIFIED_TIMEZONE)) {
      Time->TimeZone = EFI_UNSPECIFIED_TIMEZONE;
    }

    /* Adjust for the correct time zone */
    if (Time->TimeZone != EFI_UNSPECIFIED_TIMEZONE) {
      EpochSeconds -= Time->TimeZone * SEC_PER_MIN;
    }
  }

  /* Get the current daylight information from non-volatile storage */
  Size = sizeof (Daylight);
  Status = mRT->GetVariable (
                  (CHAR16 *) mDaylightVariableName,
                  &gEfiCallerIdGuid,
                  NULL,
                  &Size,
                  (VOID *)&Daylight
                  );

  if (EFI_ERROR (Status)) {
    /* The daylight variable does not exist in non-volatile storage, so create it. */
    Time->Daylight = 0;
    /* Store it */
    Status = mRT->SetVariable (
                    (CHAR16 *) mDaylightVariableName,
                    &gEfiCallerIdGuid,
                    EFI_VARIABLE_NON_VOLATILE
                      | EFI_VARIABLE_BOOTSERVICE_ACCESS
                      | EFI_VARIABLE_RUNTIME_ACCESS,
                    Size,
                    (VOID *) &(Time->Daylight)
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to save %s variable to non-volatile storage, Status = %r\n",
        __FUNCTION__,
        mDaylightVariableName,
        Status
        ));
      return Status;
    }
  } else {
    /* Got the daylight information */
    Time->Daylight = Daylight;

    /* Adjust for the correct period */
    if ((Time->Daylight & EFI_TIME_IN_DAYLIGHT) == EFI_TIME_IN_DAYLIGHT) {
      /* Convert to adjusted time, i.e. spring forwards one hour */
      EpochSeconds += SEC_PER_HOUR;
    }
  }
  EpochToEfiTime (EpochSeconds, Time);

  return EFI_SUCCESS;
}

/**
 * Sets the current local time and date information.
 *
 * @param  Time                  A pointer to the current time.
 *
 * @retval EFI_SUCCESS           The operation completed successfully.
 * @retval EFI_INVALID_PARAMETER A time field is out of range.
 * @retval EFI_DEVICE_ERROR      The time could not be set due due to hardware error.
 */
EFI_STATUS
EFIAPI
LibSetTime (
  IN EFI_TIME                *Time
  )
{
  EFI_STATUS    Status;
  UINTN         EpochSeconds;

  if ((Time == NULL) || (RtcTimeFieldsValid (Time) != EFI_SUCCESS)) {
    return EFI_INVALID_PARAMETER;
  }

  /* Always default to UTC time if unspecified */
  if (Time->TimeZone == EFI_UNSPECIFIED_TIMEZONE) {
    Time->TimeZone = TIMEZONE_0;
  }

  EpochSeconds = EfiTimeToEpoch (Time);

  /* Adjust for the correct time zone, convert to UTC time zone */
  EpochSeconds += Time->TimeZone * SEC_PER_MIN;

  /* Adjust for the correct period */
  if ((Time->Daylight & EFI_TIME_IN_DAYLIGHT) == EFI_TIME_IN_DAYLIGHT) {
    EpochSeconds -= SEC_PER_HOUR;
  }

  Status = PlatformSetTime (Time);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* Save the current time zone information into non-volatile storage */
  Status = mRT->SetVariable (
                  (CHAR16 *) mTimeZoneVariableName,
                  &gEfiCallerIdGuid,
                  EFI_VARIABLE_NON_VOLATILE
                    | EFI_VARIABLE_BOOTSERVICE_ACCESS
                    | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof (Time->TimeZone),
                  (VOID *)&(Time->TimeZone)
                  );
  if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to save %s variable to non-volatile storage, Status = %r\n",
        __FUNCTION__,
        mTimeZoneVariableName,
        Status
        ));
    return Status;
  }

  /* Save the current daylight information into non-volatile storage */
  Status = mRT->SetVariable (
                  (CHAR16 *) mDaylightVariableName,
                  &gEfiCallerIdGuid,
                  EFI_VARIABLE_NON_VOLATILE
                    | EFI_VARIABLE_BOOTSERVICE_ACCESS
                    | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof(Time->Daylight),
                  (VOID *)&(Time->Daylight)
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to save %s variable to non-volatile storage, Status = %r\n",
      __FUNCTION__,
      mDaylightVariableName,
      Status
      ));
    return Status;
  }

  EpochToEfiTime (EpochSeconds, Time);

  Status = PlatformSetTime (Time);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  EpochToEfiTime (EpochSeconds, Time);

  if (!EfiAtRuntime ()) {
    mLastSavedTimeEpoch = EpochSeconds;
    mLastSavedSystemCount = ArmGenericTimerGetSystemCount ();
  }

  return EFI_SUCCESS;
}

/**
 * Returns the current wakeup alarm clock setting.
 *
 * @param  Enabled               Indicates if the alarm is currently enabled or disabled.
 * @param  Pending               Indicates if the alarm signal is pending and requires acknowledgement.
 * @param  Time                  The current alarm setting.
 *
 * @retval EFI_SUCCESS           The alarm settings were returned.
 * @retval EFI_INVALID_PARAMETER Any parameter is NULL.
 * @retval EFI_DEVICE_ERROR      The wakeup time could not be retrieved due to a hardware error.
 */
EFI_STATUS
EFIAPI
LibGetWakeupTime (
  OUT BOOLEAN     *Enabled,
  OUT BOOLEAN     *Pending,
  OUT EFI_TIME    *Time
  )
{
  return EFI_UNSUPPORTED;
}

/**
 * Sets the system wakeup alarm clock time.
 *
 * @param  Enabled               Enable or disable the wakeup alarm.
 * @param  Time                  If Enable is TRUE, the time to set the wakeup alarm for.
 *
 * @retval EFI_SUCCESS           If Enable is TRUE, then the wakeup alarm was enabled. If
 *                               Enable is FALSE, then the wakeup alarm was disabled.
 * @retval EFI_INVALID_PARAMETER A time field is out of range.
 * @retval EFI_DEVICE_ERROR      The wakeup time could not be set due to a hardware error.
 * @retval EFI_UNSUPPORTED       A wakeup timer is not supported on this platform.
 */
EFI_STATUS
EFIAPI
LibSetWakeupTime (
  IN BOOLEAN      Enabled,
  OUT EFI_TIME    *Time
  )
{
  return EFI_UNSUPPORTED;
}

/**
 * Notification function of EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE.
 *
 * This is a notification function registered on EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE event.
 * It convers pointer to new virtual address.
 *
 * @param  Event        Event whose notification function is being invoked.
 * @param  Context      Pointer to the notification function's context.
 */
STATIC
VOID
EFIAPI
VirtualAddressChangeEvent (
  IN EFI_EVENT                            Event,
  IN VOID                                 *Context
  )
{
  EfiConvertPointer (0x0, (VOID**) &mRT);
  PlatformVirtualAddressChangeEvent ();
}

/**
 * This is the declaration of an EFI image entry point. This can be the entry point to an application
 * written to this specification, an EFI boot service driver, or an EFI runtime driver.
 *
 * @param  ImageHandle           Handle that identifies the loaded image.
 * @param  SystemTable           System Table for this image.
 *
 * @retval EFI_SUCCESS           The operation completed successfully.
 */
EFI_STATUS
EFIAPI
LibRtcInitialize (
  IN EFI_HANDLE                            ImageHandle,
  IN EFI_SYSTEM_TABLE                      *SystemTable
  )
{
  EFI_STATUS    Status;
  EFI_HANDLE    Handle;

  Status = PlatformInitialize ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /*
   * Register for the virtual address change event
   */
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  VirtualAddressChangeEvent,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mVirtualAddressChangeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  /*
   * Setup the setters and getters
   */
  mRT = gRT;
  mRT->GetTime       = LibGetTime;
  mRT->SetTime       = LibSetTime;
  mRT->GetWakeupTime = LibGetWakeupTime;
  mRT->SetWakeupTime = LibSetWakeupTime;

  /*
   * Install the protocol
   */
  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiRealTimeClockArchProtocolGuid,  NULL,
                  NULL
                 );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
