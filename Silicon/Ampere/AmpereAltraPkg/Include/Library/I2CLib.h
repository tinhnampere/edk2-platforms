/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef I2CLIB_H_
#define I2CLIB_H_

#include <Uefi/UefiBaseType.h>

/**
 Write to I2C bus.

 @Bus:          Bus ID.
 @SlaveAddr:    The address of slave device in the bus.
 @Buf:          Buffer that holds data to write.
 @WriteLength:  Pointer to length of buffer.
 @return:       EFI_INVALID_PARAMETER if parameter is invalid.
                EFI_UNSUPPORTED if the bus is not supported.
                EFI_NOT_READY if the device/bus is not ready.
                EFI_TIMEOUT if timeout why transferring data.
                Otherwise, 0 for success.
 **/
EFI_STATUS
EFIAPI
I2CWrite (
  IN     UINT32 Bus,
  IN     UINT32 SlaveAddr,
  IN OUT UINT8  *Buf,
  IN OUT UINT32 *WriteLength
  );

/**
 Read data from I2C bus.

 @Bus:          Bus ID.
 @SlaveAddr:    The address of slave device in the bus.
 @BufCmd:       Buffer where to send the command.
 @CmdLength:    Pointer to length of BufCmd.
 @Buf:          Buffer where to put the read data to.
 @ReadLength:   Pointer to length of buffer.
 @return:       EFI_INVALID_PARAMETER if parameter is invalid.
                EFI_UNSUPPORTED if the bus is not supported.
                EFI_NOT_READY if the device/bus is not ready.
                EFI_TIMEOUT if timeout why transferring data.
                EFI_CRC_ERROR if there are errors on receiving data.
                Otherwise, 0 for success.
 **/
EFI_STATUS
EFIAPI
I2CRead (
  IN     UINT32 Bus,
  IN     UINT32 SlaveAddr,
  IN     UINT8  *BufCmd,
  IN     UINT32 CmdLength,
  IN OUT UINT8  *Buf,
  IN OUT UINT32 *ReadLength
  );

/**
 Setup new transaction with I2C slave device.

 @Bus:      Bus ID.
 @BusSpeed: Bus speed in Hz.
 @return:   EFI_INVALID_PARAMETER if parameter is invalid.
            Otherwise, 0 for success.
 **/
EFI_STATUS
EFIAPI
I2CProbe (
  IN UINT32 Bus,
  IN UINTN  BusSpeed
  );

/**
 Setup a bus that to be used in runtime service.

 @Bus:      Bus ID.
 @return:   0 for success.
            Otherwise, error code.
 **/
EFI_STATUS
EFIAPI
I2CSetupRuntime (
  IN UINT32 Bus
  );

#endif /* I2CLIB_H_ */
