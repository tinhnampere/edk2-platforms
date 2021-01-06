/** @file

   Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

   SPDX-License-Identifier: BSD-2-Clause-Patent

 **/

#ifndef ARM_SPCI_LIB_H_
#define ARM_SPCI_LIB_H_

#include <Uefi.h>

/* Client ID used for SPCI calls */
#define SPCI_CLIENT_ID          0x0000ACAC

/* SPCI error codes. */
#define SPCI_SUCCESS            0
#define SPCI_NOT_SUPPORTED      -1
#define SPCI_INVALID_PARAMETER  -2
#define SPCI_NO_MEMORY          -3
#define SPCI_BUSY               -4
#define SPCI_QUEUED             -5
#define SPCI_DENIED             -6
#define SPCI_NOT_PRESENT        -7

typedef struct {
  UINT64 Token;
  UINT32 HandleId;
  UINT64 X1;
  UINT64 X2;
  UINT64 X3;
  UINT64 X4;
  UINT64 X5;
  UINT64 X6;
} ARM_SPCI_ARGS;

EFI_STATUS
EFIAPI
SpciServiceHandleOpen (
  UINT16   ClientId,
  UINT32   *HandleId,
  EFI_GUID Guid
  );

EFI_STATUS
EFIAPI
SpciServiceHandleClose (
  UINT32  HandleId
  );

EFI_STATUS
EFIAPI
SpciServiceRequestStart (
  ARM_SPCI_ARGS  *Args
  );

EFI_STATUS
EFIAPI
SpciServiceRequestResume (
  ARM_SPCI_ARGS  *Args
  );

EFI_STATUS
EFIAPI
SpciServiceGetResponse (
  ARM_SPCI_ARGS  *Args
  );

EFI_STATUS
EFIAPI
SpciServiceRequestBlocking (
  ARM_SPCI_ARGS  *Args
  );

#endif
