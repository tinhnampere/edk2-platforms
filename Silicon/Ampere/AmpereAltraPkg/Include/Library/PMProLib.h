/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PMPRO_LIB_H_
#define PMPRO_LIB_H_

#include <Uefi.h>

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
PMProDBWr (
  UINT8  Db,
  UINT32 Data,
  UINT32 Param,
  UINT32 Param1,
  UINT64 MsgReg
  );

/*
 * Send Turbo enable message to PMpro
 *
 * Enable       Enable value
 */
EFI_STATUS
EFIAPI
PMProTurboEnable (
  UINT8 Socket,
  UINT8 Enable
  );

/*
 * Send PSCI Statistic address to ACL
 * Socket   Socket to send address to
 * AdrHi    Upper 32 bits of the address
 * AdrLo    Lower 32 bits of the address
 */
EFI_STATUS
EFIAPI
PMProAclAdd (
  UINT8  Socket,
  UINT32 AdrHi,
  UINT32 AdrLo
  );

/*
 * Send PSCI Statistic address to PMPro
 * Socket   Socket to send address to
 * AdrHi    Upper 32 bits of the address
 * AdrLo    Lower 32 bits of the address
 */
EFI_STATUS
EFIAPI
PMProSendPSCIAddr (
  UINT8  Socket,
  UINT32 AdrHi,
  UINT32 AdrLo
  );

#endif /* PMPRO_LIB_H_*/
