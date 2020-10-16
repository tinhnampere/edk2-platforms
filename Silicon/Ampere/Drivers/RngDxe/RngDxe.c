/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/BaseMemoryLib.h>
#include <Uefi/UefiBaseType.h>
#include <Library/SMProLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/SMProInterface.h>
#include <Protocol/Rng.h>
#include <Platform/Ac01.h>

/*
 * Supported RNG Algorithms list by this driver.
 */
EFI_RNG_ALGORITHM mSupportedRngAlgorithms[] = {
  EFI_RNG_ALGORITHM_RAW
};

enum SCP_RNG_MSG_REQ {
	SCP_RNG_GET_TRNG = 1,
};

STATIC
EFI_STATUS
SMProRNGRead (
  IN  OUT UINT8 *Data,
  IN  UINTN     DataLen
  )
{
  UINT32      Msg[3];
  EFI_STATUS  Status;

  /* SMPro only supports 64bits at a time */
  if (DataLen != sizeof (UINT64)) {
    return EFI_INVALID_PARAMETER;
  }

  Msg[0] = SMPRO_RNG_ENCODE_MSG (SCP_RNG_GET_TRNG, 0);
  Msg[1] = 0;
  Msg[2] = 0;

  Status = SMProDBWr (SMPRO_NS_RNG_MAILBOX_INDEX, Msg[0], Msg[1], Msg[2], SMPRO_DB_BASE_REG);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = SMProDBRd (SMPRO_NS_RNG_MAILBOX_INDEX, &Msg[0], &Msg[1], &Msg[2], SMPRO_DB_BASE_REG);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  CopyMem (Data, &Msg[1], sizeof (Msg[1]));
  CopyMem (Data + sizeof (Msg[1]), &Msg[2], sizeof (Msg[2]));

  return EFI_SUCCESS;

}

/**
  Returns information about the random number generation implementation.

  @param[in]     This                 A pointer to the EFI_RNG_PROTOCOL instance.
  @param[in,out] RNGAlgorithmListSize On input, the size in bytes of RNGAlgorithmList.
                                      On output with a return code of EFI_SUCCESS, the size
                                      in bytes of the data returned in RNGAlgorithmList. On output
                                      with a return code of EFI_BUFFER_TOO_SMALL,
                                      the size of RNGAlgorithmList required to obtain the list.
  @param[out] RNGAlgorithmList        A caller-allocated memory buffer filled by the driver
                                      with one EFI_RNG_ALGORITHM element for each supported
                                      RNG algorithm. The list must not change across multiple
                                      calls to the same driver. The first algorithm in the list
                                      is the default algorithm for the driver.

  @retval EFI_SUCCESS                 The RNG algorithm list was returned successfully.
  @retval EFI_UNSUPPORTED             The services is not supported by this driver.
  @retval EFI_DEVICE_ERROR            The list of algorithms could not be retrieved due to a
                                      hardware or firmware error.
  @retval EFI_INVALID_PARAMETER       One or more of the parameters are incorrect.
  @retval EFI_BUFFER_TOO_SMALL        The buffer RNGAlgorithmList is too small to hold the result.

**/
EFI_STATUS
EFIAPI
RngGetInfo (
  IN EFI_RNG_PROTOCOL             *This,
  IN OUT UINTN                    *RNGAlgorithmListSize,
  OUT EFI_RNG_ALGORITHM           *RNGAlgorithmList
  )
{
  EFI_STATUS    Status;
  UINTN         RequiredSize;

  if ((This == NULL) || (RNGAlgorithmListSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  RequiredSize = sizeof (mSupportedRngAlgorithms);
  if (*RNGAlgorithmListSize < RequiredSize) {
    Status = EFI_BUFFER_TOO_SMALL;
  } else {
    if (RNGAlgorithmList != NULL) {
      CopyMem (RNGAlgorithmList, mSupportedRngAlgorithms, RequiredSize);
      Status = EFI_SUCCESS;
    } else {
      Status = EFI_INVALID_PARAMETER;
    }
  }
  *RNGAlgorithmListSize = RequiredSize;

  return Status;
}

/**
  Produces and returns an RNG value using either the default or specified RNG algorithm.

  @param[in]  This                    A pointer to the EFI_RNG_PROTOCOL instance.
  @param[in]  RNGAlgorithm            A pointer to the EFI_RNG_ALGORITHM that identifies the RNG
                                      algorithm to use. May be NULL in which case the function will
                                      use its default RNG algorithm.
  @param[in]  RNGValueLength          The length in bytes of the memory buffer pointed to by
                                      RNGValue. The driver shall return exactly this numbers of bytes.
  @param[out] RNGValue                A caller-allocated memory buffer filled by the driver with the
                                      resulting RNG value.

  @retval EFI_SUCCESS                 The RNG value was returned successfully.
  @retval EFI_UNSUPPORTED             The algorithm specified by RNGAlgorithm is not supported by
                                      this driver.
  @retval EFI_DEVICE_ERROR            An RNG value could not be retrieved due to a hardware or
                                      firmware error.
  @retval EFI_NOT_READY               There is not enough random data available to satisfy the length
                                      requested by RNGValueLength.
  @retval EFI_INVALID_PARAMETER       RNGValue is NULL or RNGValueLength is zero.

**/
EFI_STATUS
EFIAPI
RngGetRNG (
  IN EFI_RNG_PROTOCOL            *This,
  IN EFI_RNG_ALGORITHM           *RNGAlgorithm, OPTIONAL
  IN UINTN                       RNGValueLength,
  OUT UINT8                      *RNGValue
  )
{
  UINTN         Count, Length;
  UINT64        TmpVal64;
  EFI_STATUS    Status = EFI_UNSUPPORTED;

  if ((RNGValueLength == 0) || (RNGValue == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (RNGAlgorithm == NULL) {
    /*
     * Use the default RNG algorithm if RNGAlgorithm is NULL.
     */
    RNGAlgorithm = &gEfiRngAlgorithmRaw;
  }

  if (CompareGuid (RNGAlgorithm, &gEfiRngAlgorithmRaw)) {
    /* FIXME: Linux kernel only passes 64bits which violate UEFI Spec */
    /*
     * When a DRBG is used on the output of a entropy source,
     * its security level must be at least 256 bits according to UEFI Spec.
     */
    /*if (RNGValueLength < 32) {
      return EFI_INVALID_PARAMETER;
    }*/

    for (Count = 0; Count < (RNGValueLength / sizeof (TmpVal64)) + 1; Count++) {
      Length = sizeof (TmpVal64);
      if (Count == (RNGValueLength / sizeof (TmpVal64))) {
        Length = RNGValueLength % sizeof (TmpVal64);
      }

      if (Length != 0) {
        Status = SMProRNGRead ((UINT8 *)&TmpVal64, sizeof (TmpVal64));
        if (EFI_ERROR (Status)) {
          return EFI_NOT_READY;
        }
        CopyMem (RNGValue + Count * sizeof (TmpVal64), &TmpVal64, Length);
      }
    }
    Status = EFI_SUCCESS;
  }

  return Status;
}

/*
 * The Random Number Generator (RNG) protocol
 */
EFI_RNG_PROTOCOL mRng = {
  RngGetInfo,
  RngGetRNG
};

/**
  The user Entry Point for the Random Number Generator (RNG) driver.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval EFI_NOT_SUPPORTED Platform does not support RNG.
  @retval Other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
RngDriverEntry (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE    *SystemTable
  )
{
  EFI_STATUS                        Status;
  EFI_HANDLE                        Handle;

  //
  // Install UEFI RNG (Random Number Generator) Protocol
  //
  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiRngProtocolGuid,
                  &mRng,
                  NULL
                  );

  return Status;
}
