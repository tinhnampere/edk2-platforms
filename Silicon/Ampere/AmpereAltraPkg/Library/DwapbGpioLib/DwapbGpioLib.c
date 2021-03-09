/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/DwapbGpioLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Platform/Ac01.h>

/* Runtime needs to be 64K alignment */
#define RUNTIME_ADDRESS_MASK           (~(SIZE_64KB - 1))
#define RUNTIME_ADDRESS_LENGTH         SIZE_64KB

#define GPIO_MUX_VAL(Gpio)              (0x00000001 << (Gpio))
#define GPIO_IN                         0
#define GPIO_OUT                        1

/* Address GPIO_REG Registers */
#define GPIO_SWPORTA_DR_ADDR            0x00000000
#define GPIO_SWPORTA_DDR_ADDR           0x00000004
#define GPIO_EXT_PORTA_ADDR             0x00000050

STATIC UINT64    GpioBaseAddr[] = { GPIO_DWAPB_BASE_ADDR };
STATIC UINT64    GpiBaseAddr[] = { GPI_DWAPB_BASE_ADDR };
STATIC BOOLEAN   GpioRuntimeEnableArray[sizeof (GpioBaseAddr) / sizeof (GpioBaseAddr[0])] = { FALSE };
STATIC EFI_EVENT mVirtualAddressChangeEvent = NULL;

UINT64
GetBaseAddr (
  IN UINT32 Pin
  )
{
  UINT32 NumberOfControllers = sizeof (GpioBaseAddr) / sizeof (GpioBaseAddr[0]);
  UINT32 TotalPins = GPIO_DWAPB_PINS_PER_CONTROLLER * NumberOfControllers;

  if (NumberOfControllers == 0 || Pin >= TotalPins) {
    return 0;
  }

  return GpioBaseAddr[Pin / GPIO_DWAPB_PINS_PER_CONTROLLER];
}

VOID
DwapbGpioWrite (
  IN UINT64 Base,
  IN UINT32 Val
  )
{
  MmioWrite32 ((UINTN)Base, Val);
}

VOID
DwapbGpioRead (
  IN  UINT64 Base,
  OUT UINT32 *Val
  )
{
  ASSERT (Val != NULL);
  *Val = MmioRead32 (Base);
}

VOID
EFIAPI
DwapbGpioWriteBit (
  IN UINT32 Pin,
  IN UINT32 Val
  )
{
  UINT64 Reg;
  UINT32 GpioPin;
  UINT32 ReadVal;

  Reg = GetBaseAddr (Pin);
  if (Reg == 0) {
    return;
  }

  GpioPin = Pin % GPIO_DWAPB_PINS_PER_CONTROLLER;

  Reg += GPIO_SWPORTA_DR_ADDR;
  DwapbGpioRead (Reg, &ReadVal);

  if (Val != 0) {
    DwapbGpioWrite (Reg, ReadVal | GPIO_MUX_VAL (GpioPin));
  } else {
    DwapbGpioWrite (Reg, ReadVal & ~GPIO_MUX_VAL (GpioPin));
  }
}

UINTN
EFIAPI
DwapbGpioReadBit (
  IN UINT32 Pin
  )
{
  UINT64 Reg;
  UINT32 Val;
  UINT32 GpioPin;
  UINT8  Index;
  UINT32 MaxIndex;

  Reg = GetBaseAddr (Pin);
  if (Reg == 0) {
    return 0;
  }

  GpioPin = Pin % GPIO_DWAPB_PINS_PER_CONTROLLER;

  /* Check if a base address is GPI */
  MaxIndex = sizeof (GpiBaseAddr) / sizeof (GpiBaseAddr[0]);
  for (Index = 0; Index < MaxIndex; Index++) {
    if (Reg == GpiBaseAddr[Index]) {
      break;
    }
  }
  if (Index == MaxIndex) {
    /* Only GPIO has GPIO_EXT_PORTA register, not for GPI */
    Reg +=  GPIO_EXT_PORTA_ADDR;
  }

  DwapbGpioRead (Reg, &Val);

  return Val & GPIO_MUX_VAL (GpioPin) ? 1 : 0;
}

EFI_STATUS
DwapbGpioConfig (
  IN UINT32 Pin,
  IN UINT32 InOut
  )
{
  INTN   GpioPin;
  UINT32 Val;
  UINT64 Reg;

  /*
   * Caculate GPIO Pin Number for Direction Register
   * GPIO_SWPORTA_DDR for GPIO[31...0]
   * GPIO_SWPORTB_DDR for GPIO[51...32]
   */

  Reg = GetBaseAddr (Pin);
  if (Reg == 0) {
    return EFI_UNSUPPORTED;
  }

  Reg += GPIO_SWPORTA_DDR_ADDR;
  GpioPin = Pin % GPIO_DWAPB_PINS_PER_CONTROLLER;
  DwapbGpioRead (Reg, &Val);

  if (InOut == GPIO_OUT) {
    Val |= GPIO_MUX_VAL (GpioPin);
  } else {
    Val &= ~GPIO_MUX_VAL (GpioPin);
  }
  DwapbGpioWrite (Reg, Val);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
DwapbGPIOModeConfig (
  UINT8 Pin,
  UINTN Mode
  )
{
  UINT32 NumberOfControllers = sizeof (GpioBaseAddr) / sizeof (UINT64);
  UINT32 NumersOfPins = NumberOfControllers * GPIO_DWAPB_PINS_PER_CONTROLLER;
  UINT32 Delay = 10;

  if (Mode < GPIO_CONFIG_OUT_LOW
      || Mode >= MAX_GPIO_CONFIG_MODE
      || Pin > NumersOfPins - 1
      || Pin < 0)
  {
    return EFI_INVALID_PARAMETER;
  }

  switch (Mode) {
  case GPIO_CONFIG_OUT_LOW:
    DwapbGpioConfig (Pin, GPIO_OUT);
    DwapbGpioWriteBit (Pin, 0);
    DEBUG ((DEBUG_INFO, "GPIO pin %d configured as output low\n", Pin));
    break;

  case GPIO_CONFIG_OUT_HI:
    DwapbGpioConfig (Pin, GPIO_OUT);
    DwapbGpioWriteBit (Pin, 1);
    DEBUG ((DEBUG_INFO, "GPIO pin %d configured as output high\n", Pin));
    break;

  case GPIO_CONFIG_OUT_LOW_TO_HIGH:
    DwapbGpioConfig (Pin, GPIO_OUT);
    DwapbGpioWriteBit (Pin, 0);
    MicroSecondDelay (1000 * Delay);
    DwapbGpioWriteBit (Pin, 1);
    DEBUG ((DEBUG_INFO, "GPIO pin %d configured as output low->high\n", Pin));
    break;

  case GPIO_CONFIG_OUT_HIGH_TO_LOW:
    DwapbGpioConfig (Pin, GPIO_OUT);
    DwapbGpioWriteBit (Pin, 1);
    MicroSecondDelay (1000 * Delay);
    DwapbGpioWriteBit (Pin, 0);
    DEBUG ((DEBUG_INFO, "GPIO pin %d configured as output high->low\n", Pin));
    break;

  case GPIO_CONFIG_IN:
    DwapbGpioConfig (Pin, GPIO_IN);
    DEBUG ((DEBUG_INFO, "GPIO pin %d configured as input\n", Pin));
    break;

  default:
    break;
  }

  return EFI_SUCCESS;
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
VOID
EFIAPI
GpioVirtualAddressChangeEvent (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  UINTN Count;

  EfiConvertPointer (0x0, (VOID **)&GpioBaseAddr);
  for (Count = 0; Count < sizeof (GpioBaseAddr) / sizeof (GpioBaseAddr[0]); Count++) {
    if (!GpioRuntimeEnableArray[Count]) {
      continue;
    }
    EfiConvertPointer (0x0, (VOID **)&GpioBaseAddr[Count]);
  }
}

/**
 Setup a controller that to be used in runtime service.

 @Bus:      Bus ID.
 @return:   0 for success.
            Otherwise, error code.
 **/
EFI_STATUS
EFIAPI
DwapbGPIOSetupRuntime (
  IN UINT32 Pin
  )
{
  EFI_STATUS                      Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR Descriptor;

  if (GetBaseAddr (Pin) == 0) {
    return EFI_INVALID_PARAMETER;
  }

  if (mVirtualAddressChangeEvent == NULL) {
    /*
    * Register for the virtual address change event
    */
    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_NOTIFY,
                    GpioVirtualAddressChangeEvent,
                    NULL,
                    &gEfiEventVirtualAddressChangeGuid,
                    &mVirtualAddressChangeEvent
                    );
    ASSERT_EFI_ERROR (Status);
  }

  Status = gDS->GetMemorySpaceDescriptor (
                  GetBaseAddr (Pin) & RUNTIME_ADDRESS_MASK,
                  &Descriptor
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gDS->SetMemorySpaceAttributes (
                  GetBaseAddr (Pin) & RUNTIME_ADDRESS_MASK,
                  RUNTIME_ADDRESS_LENGTH,
                  Descriptor.Attributes | EFI_MEMORY_RUNTIME
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  GpioRuntimeEnableArray[Pin / GPIO_DWAPB_PINS_PER_CONTROLLER] = TRUE;

  return Status;
}
