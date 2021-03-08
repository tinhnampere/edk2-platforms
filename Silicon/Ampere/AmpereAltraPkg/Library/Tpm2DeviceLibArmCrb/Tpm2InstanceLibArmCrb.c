/** @file
  This library is TPM2 instance.
  It can be registered to Tpm2 Device router, to be active TPM2 engine,
  based on platform setting.

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/TpmInstance.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/Tpm2DeviceLib.h>

#include "Tpm2ArmCrb.h"

TPM2_DEVICE_INTERFACE  mTpm2InternalTpm2Device = {
  TPM_DEVICE_INTERFACE_TPM20_DTPM,
  Tpm2ArmCrbSubmitCommand,
  Tpm2ArmCrbRequestUseTpm,
};

/**
  The function register TPM2.0 instance and caches current active TPM interface type.

  @retval EFI_SUCCESS   TPM2.0 instance is registered, or system
                        does not support register TPM2.0 instance.
**/
EFI_STATUS
EFIAPI
Tpm2InstanceLibArmCrbConstructor (
  VOID
  )
{
  EFI_STATUS               Status;

  Status = Tpm2ArmCrbInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a:%d Failed to initialize TPM2 CRB interface.\n",
      __FUNCTION__,__LINE__
      ));

    return Status;
  }

  Status = Tpm2RegisterTpm2DeviceLib (&mTpm2InternalTpm2Device);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a:%d Failed to register TPM2 Device.\n",
      __FUNCTION__, __LINE__
      ));

    return Status;
  }

  return EFI_SUCCESS;
}
