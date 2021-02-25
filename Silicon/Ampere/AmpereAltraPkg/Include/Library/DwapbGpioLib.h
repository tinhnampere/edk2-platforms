/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef DWAPB_GPIO_LIB_H_
#define DWAPB_GPIO_LIB_H_

enum SocGpioConfigMode {
  GPIO_CONFIG_OUT_LOW = 0,
  GPIO_CONFIG_OUT_HI,
  GPIO_CONFIG_OUT_LOW_TO_HIGH,
  GPIO_CONFIG_OUT_HIGH_TO_LOW,
  GPIO_CONFIG_IN,
  MAX_GPIO_CONFIG_MODE
};

/*
 *  DwapbGpioWriteBit: Use to Set/Clear GPIOs
 *  Input:
 *              Pin : Pin Identification
 *              Val : 1 to Set, 0 to Clear
 */
VOID
EFIAPI
DwapbGpioWriteBit (
  IN UINT32 Pin,
  IN UINT32 Val
  );

/*
 *   DwapbGpioReadBit:
 *   Input:
 *              Pin : Pin Identification
 *   Return:
 *              1 : On/High
 *              0 : Off/Low
 */
UINTN
EFIAPI
DwapbGpioReadBit (
  IN UINT32 Pin
  );

/*
 *  DwapbGPIOModeConfig: Use to configure GPIOs as Input/Output
 *  Input:
 *              Pin : Pin Identification
 *              InOut : GPIO_OUT/1 as Output
 *                      GPIO_IN/0  as Input
 */
EFI_STATUS
EFIAPI
DwapbGPIOModeConfig (
  UINT8 Pin,
  UINTN Mode
  );

/*
 *  Setup a controller that to be used in runtime service.
 *  Input:
 *              Pin: Pin belongs to the controller.
 *  return:     0 for success.
 *              Otherwise, error code.
 */
EFI_STATUS
EFIAPI
DwapbGPIOSetupRuntime (
  IN UINT32 Pin
  );

#endif /* DWAPB_GPIO_LIB_H_ */
