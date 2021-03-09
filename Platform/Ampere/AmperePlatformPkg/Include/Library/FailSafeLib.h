/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef FAILSAFE_LIB_H_
#define FAILSAFE_LIB_H_

enum {
  MM_SPINOR_FUNC_GET_INFO,
  MM_SPINOR_FUNC_READ,
  MM_SPINOR_FUNC_WRITE,
  MM_SPINOR_FUNC_ERASE,
  MM_SPINOR_FUNC_GET_NVRAM_INFO,
  MM_SPINOR_FUNC_GET_NVRAM2_INFO,
  MM_SPINOR_FUNC_GET_FAILSAFE_INFO
};

#define MM_SPINOR_RES_SUCCESS           0xAABBCC00
#define MM_SPINOR_RES_FAIL              0xAABBCCFF

enum {
  FAILSAFE_BOOT_NORMAL = 0,
  FAILSAFE_BOOT_LAST_KNOWN_SETTINGS,
  FAILSAFE_BOOT_DEFAULT_SETTINGS,
  FAILSAFE_BOOT_DDR_DOWNGRADE,
  FAILSAFE_BOOT_SUCCESSFUL
};

/**
  Get the FailSafe region information.
**/
EFI_STATUS
EFIAPI
FailSafeGetRegionInfo (
  UINT64 *Offset,
  UINT64 *Size
  );

/**
  Inform to FailSafe monitor that the system boots successfully.
**/
EFI_STATUS
EFIAPI
FailSafeBootSuccessfully (
  VOID
  );

/**
  Simulate UEFI boot failure due to config wrong NVPARAM for
  testing failsafe feature
**/
EFI_STATUS
EFIAPI
FailSafeTestBootFailure (
  VOID
  );

#endif /* FAILSAFE_LIB_H_ */
