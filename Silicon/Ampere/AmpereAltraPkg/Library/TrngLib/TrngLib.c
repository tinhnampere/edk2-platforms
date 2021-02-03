/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SMProLib.h>
#include <Library/SMProInterface.h>
#include <Library/TrngLib.h>
#include <Platform/Ac01.h>

//
// RNG Message Type
//
enum {
  SCP_RNG_GET_TRNG = 1
};


/**
  Request for generating a 64-bits random number from SMPro
  via Mailbox interface.

  @param[out] Buffer      Buffer to receive the 64-bits random number.

  @retval EFI_SUCCESS           The random value was returned successfully.
  @retval EFI_DEVICE_ERROR      A random value could not be retrieved
                                due to a hardware or firmware error.
  @retval EFI_INVALID_PARAMETER Buffer is NULL.
**/
EFI_STATUS
SMProGetRandomNumber64 (
  OUT UINT8     *Buffer
  )
{
  UINT32        Message[3];
  EFI_STATUS    Status;

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Message[0] = SMPRO_RNG_ENCODE_MSG (SCP_RNG_GET_TRNG, 0);
  Message[1] = 0;
  Message[2] = 0;

  Status = SMProDBWr (
             SMPRO_NS_RNG_MAILBOX_INDEX,
             Message[0],
             Message[1],
             Message[2],
             SMPRO_DB_BASE_REG
             );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = SMProDBRd (
             SMPRO_NS_RNG_MAILBOX_INDEX,
             &Message[0],
             &Message[1],
             &Message[2],
             SMPRO_DB_BASE_REG
             );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  CopyMem (Buffer, &Message[1], sizeof (Message[1]));
  CopyMem (Buffer + sizeof (Message[1]), &Message[2], sizeof (Message[2]));

  return EFI_SUCCESS;
}

/**
  Generates a random number by using Hardware RNG in SMPro.

  @param[out] Buffer      Buffer to receive the random number.
  @param[in]  BufferSize  Number of bytes in Buffer.

  @retval EFI_SUCCESS           The random value was returned successfully.
  @retval EFI_DEVICE_ERROR      A random value could not be retrieved
                                due to a hardware or firmware error.
  @retval EFI_INVALID_PARAMETER Buffer is NULL or BufferSize is zero.
**/
EFI_STATUS
EFIAPI
GenerateRandomNumbers (
  OUT UINT8     *Buffer,
  IN  UINTN     BufferSize
  )
{
  UINTN         Count;
  UINTN         RandSize;
  UINT64        Value;
  EFI_STATUS    Status;

  if ((BufferSize == 0) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // SMPro only supports generating a 64-bits random number once.
  //
  RandSize = sizeof (UINT64);
  for (Count = 0; Count < (BufferSize / sizeof (UINT64)) + 1; Count++) {
    if (Count == (BufferSize / sizeof (UINT64))) {
      RandSize = BufferSize % sizeof (UINT64);
    }

    if (RandSize != 0) {
      Status = SMProGetRandomNumber64 ((UINT8 *)&Value);
      if (EFI_ERROR (Status)) {
        return EFI_DEVICE_ERROR;
      }
      CopyMem (Buffer + Count * sizeof (UINT64), &Value, RandSize);
    }
  }

  return EFI_SUCCESS;
}
