/** @file
  This library is TPM2 device lib.

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/Tpm2DeviceLib.h>

#include "Tpm2ArmCrb.h"

/**
  This service enables the sending of commands to the TPM2.

  @param[in]      InputParameterBlockSize  Size of the TPM2 input parameter block.
  @param[in]      InputParameterBlock      Pointer to the TPM2 input parameter block.
  @param[in,out]  OutputParameterBlockSize Size of the TPM2 output parameter block.
  @param[in]      OutputParameterBlock     Pointer to the TPM2 output parameter block.

  @retval EFI_SUCCESS            The command byte stream was successfully sent to
                                 the device and a response was successfully received.
  @retval EFI_DEVICE_ERROR       The command was not successfully sent to the device
                                 or a response was not successfully received from the device.
  @retval EFI_BUFFER_TOO_SMALL   The output parameter block is too small.
**/
EFI_STATUS
EFIAPI
Tpm2SubmitCommand (
  IN     UINT32 InputParameterBlockSize,
  IN     UINT8  *InputParameterBlock,
  IN OUT UINT32 *OutputParameterBlockSize,
  IN     UINT8  *OutputParameterBlock
  )
{
  return Tpm2ArmCrbSubmitCommand (
           InputParameterBlockSize,
           InputParameterBlock,
           OutputParameterBlockSize,
           OutputParameterBlock
           );
}

/**
  This service requests use TPM2.

  @retval EFI_SUCCESS      Get the control of TPM2 chip.
  @retval EFI_NOT_FOUND    TPM2 not found.
  @retval EFI_DEVICE_ERROR Unexpected device behavior.
**/
EFI_STATUS
EFIAPI
Tpm2RequestUseTpm (
  VOID
  )
{
  return Tpm2ArmCrbRequestUseTpm ();
}

/**
  This service register TPM2 device.

  @param Tpm2Device  TPM2 device

  @retval EFI_SUCCESS          This TPM2 device is registered successfully.
  @retval EFI_UNSUPPORTED      System does not support register this TPM2 device.
  @retval EFI_ALREADY_STARTED  System already register this TPM2 device.
**/
EFI_STATUS
EFIAPI
Tpm2RegisterTpm2DeviceLib (
  IN TPM2_DEVICE_INTERFACE *Tpm2Device
  )
{
  return EFI_UNSUPPORTED;
}

/**
  The function caches current active TPM interface type.

  @retval EFI_SUCCESS   TPM2.0 instance is registered, or system
                        does not support register TPM2.0 instance
**/
EFI_STATUS
EFIAPI
Tpm2DeviceLibConstructor (
  VOID
  )
{
  EFI_STATUS Status;

  Status = Tpm2ArmCrbInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR, "%a:%d Failed to initialize TPM2 CRB interface \n",
      __FUNCTION__,
      __LINE__
      ));

    return Status;
  }

  return EFI_SUCCESS;
}
