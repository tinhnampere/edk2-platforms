/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SMPRO_LIB_H_
#define SMPRO_LIB_H_

/*
 * Read the doorbell status into data
 * Db           Doorbell number
 * Data         Data to be written from DB IN
 * Param        Data to be written from DB IN0
 * Param1       Data to be written from DB IN1
 * MsgReg       Non-secure doorbell base virtual address
 */
EFI_STATUS
EFIAPI
SMProDBRd (
  UINT8  Db,
  UINT32 *Data,
  UINT32 *Param,
  UINT32 *Param1,
  UINT64 MsgReg
  );

/*
 * Write the message to the doorbell
 * Db           Doorbell number
 * Data         Data to be written to DB OUT
 * Param        Data to be written to DB OUT0
 * Param1       Data to be written to DB OUT1
 * MsgReg       Non-secure doorbell base virtual address
 */
EFI_STATUS
EFIAPI
SMProDBWr (
  UINT8  Db,
  UINT32 Data,
  UINT32 Param,
  UINT32 Param1,
  UINT64 MsgReg
  );

/*
 * Read register from SMPro
 */
EFI_STATUS
EFIAPI
SMProRegRd (
  UINT8  Socket,
  UINT64 Addr,
  UINT32 *Value
  );

/*
 * Write register to SMPro
 */
EFI_STATUS
EFIAPI
SMProRegWr (
  UINT8  Socket,
  UINT64 Addr,
  UINT32 Value
  );

#endif /* SMPRO_LIB_H_*/
