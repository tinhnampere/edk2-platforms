/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef FLASH_LIB_H_
#define FLASH_LIB_H_

EFI_STATUS
EFIAPI
FlashGetNvRamInfo (
  OUT UINT64  *NvRamBase,
  OUT UINT32  *NvRamSize
  );

EFI_STATUS
EFIAPI
FlashEraseCommand (
  IN      UINT8     *pBlockAddress,
  IN      UINT32    Length
  );

EFI_STATUS
EFIAPI
FlashProgramCommand (
  IN      UINT8     *pByteAddress,
  IN      UINT8     *Byte,
  IN OUT  UINTN     *Length
  );

EFI_STATUS
EFIAPI
FlashReadCommand (
  IN      UINT8     *pByteAddress,
  OUT     UINT8     *Byte,
  IN OUT  UINTN     *Length
  );

#endif /* FLASH_LIB_H_ */
