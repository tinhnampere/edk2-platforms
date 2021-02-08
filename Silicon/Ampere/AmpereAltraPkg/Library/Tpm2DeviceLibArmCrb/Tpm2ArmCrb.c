/** @file
  Defines platform Arm CRB interface that TPM2 can only be accessed in
  Arm Trusted Firmware (ATF) secure partition.

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/TimerLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/PcdLib.h>
#include <IndustryStandard/Tpm20.h>
#include <Guid/PlatformInfoHobGuid.h>
#include <PlatformInfoHob.h>
#include "Tpm2ArmCrb.h"

typedef enum {
  TPM2_NO_SUPPORT = 0,
  TPM2_CRB_INTERFACE
} PLATFORM_TPM2_INTERFACE_TYPE;

PLATFORM_TPM2_CONFIG_DATA               PlatformTpm2Config;
PLATFORM_TPM2_CRB_INTERFACE_PARAMETERS  PlatformTpm2InterfaceParams;

//
// Execution of the command may take from several seconds to minutes for certain
// commands, such as key generation.
//
#define CRB_TIMEOUT_MAX             (90000 * 1000)  // 90s

/**
  Check whether the value of a TPM chip register satisfies the input BIT setting.

  @param[in]  Register     Address port of register to be checked.
  @param[in]  BitSet       Check these data bits are set.
  @param[in]  BitClear     Check these data bits are clear.
  @param[in]  TimeOut      The max wait time (unit MicroSecond) when checking register.

  @retval     EFI_SUCCESS  The register satisfies the check bit.
  @retval     EFI_TIMEOUT  The register can't run into the expected status in time.
**/
EFI_STATUS
Tpm2ArmCrbWaitRegisterBits (
  IN      UINT32                    *Register,
  IN      UINT32                    BitSet,
  IN      UINT32                    BitClear,
  IN      UINT32                    TimeOut
  )
{
  UINT32                            RegRead;
  UINT32                            WaitTime;

  for (WaitTime = 0; WaitTime < TimeOut; WaitTime += 30){
    RegRead = MmioRead32 ((UINTN)Register);
    if ((RegRead & BitSet) == BitSet && (RegRead & BitClear) == 0) {
      return EFI_SUCCESS;
    }
    MicroSecondDelay (30);
  }

  return EFI_TIMEOUT;
}

EFI_STATUS
Tpm2ArmCrbInvokeTpmService (
  VOID
  )
{
  ARM_SMC_ARGS  ArmSmcArgs;

  ZeroMem (&ArmSmcArgs, sizeof (ARM_SMC_ARGS));
  ArmSmcArgs.Arg0 = PlatformTpm2InterfaceParams.SmcFunctionId;
  ArmCallSmc (&ArmSmcArgs);
  if (ArmSmcArgs.Arg0 != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a:%d Failed to invoke TPM Service Handler in Trusted Firmware EL3.\n",
      __FUNCTION__,
      __LINE__
      ));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

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
Tpm2ArmCrbSubmitCommand (
  IN UINT32            InputParameterBlockSize,
  IN UINT8             *InputParameterBlock,
  IN OUT UINT32        *OutputParameterBlockSize,
  IN UINT8             *OutputParameterBlock
  )
{
  EFI_STATUS                        Status;
  PLATFORM_TPM2_CONTROL_AREA_PTR    Tpm2ControlArea;
  UINTN                             CommandBuffer;
  UINTN                             ResponseBuffer;
  UINT32                            Index;
  UINT32                            TpmOutSize;
  UINT16                            Data16;
  UINT32                            Data32;

  DEBUG_CODE (
    UINTN  DebugSize;

    DEBUG ((DEBUG_VERBOSE, "ArmCrbTpmCommand Send - "));
    if (InputParameterBlockSize > 0x100) {
      DebugSize = 0x40;
    } else {
      DebugSize = InputParameterBlockSize;
    }
    for (Index = 0; Index < DebugSize; Index++) {
      DEBUG ((DEBUG_VERBOSE, "%02x ", InputParameterBlock[Index]));
    }
    if (DebugSize != InputParameterBlockSize) {
      DEBUG ((DEBUG_VERBOSE, "...... "));
      for (Index = InputParameterBlockSize - 0x20; Index < InputParameterBlockSize; Index++) {
        DEBUG ((DEBUG_VERBOSE, "%02x ", InputParameterBlock[Index]));
      }
    }
    DEBUG ((DEBUG_VERBOSE, "\n"));
  );
  TpmOutSize         = 0;

  Tpm2ControlArea =
    (PLATFORM_TPM2_CONTROL_AREA_PTR)(UINTN)(PlatformTpm2InterfaceParams.AddressOfControlArea);

  //
  // Write CRB Command Buffer
  //
  CommandBuffer = (UINTN)((UINTN)(Tpm2ControlArea->CrbControlCommandAddressHigh) << 32
                                | Tpm2ControlArea->CrbControlCommandAddressLow);
  MmioWriteBuffer8 (CommandBuffer, InputParameterBlockSize, InputParameterBlock);

  //
  // Set Start bit
  //
  MmioWrite32((UINTN)&Tpm2ControlArea->CrbControlStart, CRB_CONTROL_START);

  //
  // The UEFI needs to make a SMC Service Call to invoke TPM Service Handler
  // in Arm Trusted Firmware EL3.
  //
  Status = Tpm2ArmCrbInvokeTpmService ();
  if(EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
    goto GoIdle_Exit;
  }

  Status = Tpm2ArmCrbWaitRegisterBits (
             &Tpm2ControlArea->CrbControlStart,
             0,
             CRB_CONTROL_START,
             CRB_TIMEOUT_MAX
             );
  if (EFI_ERROR (Status)) {
    //
    // Try to goIdle, the behavior is agnostic.
    //
    Status = EFI_DEVICE_ERROR;
    goto GoIdle_Exit;
  }

  //
  // Get response data header
  //
  ResponseBuffer = (UINTN)(Tpm2ControlArea->CrbControlResponseAddrss);
  MmioReadBuffer8 (ResponseBuffer, sizeof (TPM2_RESPONSE_HEADER), OutputParameterBlock);
  DEBUG_CODE (
    DEBUG ((DEBUG_VERBOSE, "ArmCrbTpmCommand ReceiveHeader - "));
    for (Index = 0; Index < sizeof (TPM2_RESPONSE_HEADER); Index++) {
      DEBUG ((DEBUG_VERBOSE, "%02x ", OutputParameterBlock[Index]));
    }
    DEBUG ((DEBUG_VERBOSE, "\n"));
  );

  //
  // Check the response data header (tag, parasize and returncode)
  //
  CopyMem (&Data16, OutputParameterBlock, sizeof (UINT16));
  // TPM2 should not use this RSP_COMMAND
  if (SwapBytes16 (Data16) == TPM_ST_RSP_COMMAND) {
    DEBUG ((DEBUG_ERROR, "TPM2: TPM_ST_RSP error - %x\n", TPM_ST_RSP_COMMAND));
    Status = EFI_UNSUPPORTED;
    goto GoIdle_Exit;
  }

  CopyMem (&Data32, (OutputParameterBlock + 2), sizeof (UINT32));
  TpmOutSize  = SwapBytes32 (Data32);
  if (*OutputParameterBlockSize < TpmOutSize) {
    //
    // Command completed, but buffer is not enough
    //
    Status = EFI_BUFFER_TOO_SMALL;
    goto GoIdle_Exit;
  }
  *OutputParameterBlockSize = TpmOutSize;

  //
  // Continue reading the remaining data
  //
  MmioReadBuffer8 (ResponseBuffer, TpmOutSize, OutputParameterBlock);

  DEBUG_CODE (
    DEBUG ((DEBUG_VERBOSE, "ArmCrbTpmCommand Receive - "));
    for (Index = 0; Index < TpmOutSize; Index++) {
      DEBUG ((DEBUG_VERBOSE, "%02x ", OutputParameterBlock[Index]));
    }
    DEBUG ((DEBUG_VERBOSE, "\n"));
  );

GoIdle_Exit:
  //
  //  Always go Idle state.
  //
  MmioWrite32 ((UINTN)&Tpm2ControlArea->CrbControlRequest, CRB_CONTROL_AREA_REQUEST_GO_IDLE);

  return Status;
}

/**
  This service requests use TPM2.

  @retval EFI_SUCCESS      Get the control of TPM2 chip.
  @retval EFI_NOT_FOUND    TPM2 not found.
  @retval EFI_DEVICE_ERROR Unexpected device behavior.
**/
EFI_STATUS
EFIAPI
Tpm2ArmCrbRequestUseTpm (
  VOID
  )
{
  PLATFORM_TPM2_CONTROL_AREA_PTR ControlAreaPtr;

  if (PlatformTpm2Config.InterfaceType != TPM2_CRB_INTERFACE) {
    return EFI_NOT_FOUND;
  }

  ControlAreaPtr =
    (PLATFORM_TPM2_CONTROL_AREA_PTR)(UINTN)(PlatformTpm2InterfaceParams.AddressOfControlArea);

  if (ControlAreaPtr->CrbControlCommandAddressLow == 0
    || ControlAreaPtr->CrbControlCommandAddressLow == 0xFFFFFFFF
    || (ControlAreaPtr->CrbControlStatus & CRB_CONTROL_AREA_STATUS_TPM_STATUS) == 1)
  {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
Tpm2ArmCrbInitialize (
  VOID
  )
{
  VOID                       *GuidHob;
  PlatformInfoHob_V2         *PlatformHob;

  GuidHob = GetFirstGuidHob (&gPlatformHobV2Guid);
  if (GuidHob == NULL) {
    return EFI_DEVICE_ERROR;
  }

  PlatformHob = (PlatformInfoHob_V2 *)GET_GUID_HOB_DATA (GuidHob);
  PlatformTpm2Config = PlatformHob->Tpm2Info.Tpm2ConfigData;
  PlatformTpm2InterfaceParams = PlatformHob->Tpm2Info.Tpm2CrbInterfaceParams;

  //
  // TODO: Validate Platform TPM Info HOB.
  //

  return EFI_SUCCESS;
}
