/** @file
  Defines platform Arm CRB interface that TPM2 can only be accessed in
  Arm Trusted Firmware (ATF) secure partition.

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef TPM2_ARM_CRB_H_
#define TPM2_ARM_CRB_H_

#include <Uefi.h>

//
// Command Response Buffer (CRB) interface definition
//
// Refer to TPM 2.0 Mobile Command Response Buffer Interface
// Level 00 Revision 12
//

//
// Set structure alignment to 1-byte
//
#pragma pack (1)

//
// Register set map as specified in Section 3
//
typedef struct {
  UINT32 CrbControlRequest;        // 00h
  ///
  /// Register used by the TPM to provide status of the CRB interface.
  ///
  UINT32 CrbControlStatus;         // 04h
  ///
  /// Register used by software to cancel command processing.
  ///
  UINT32 CrbControlCancel;         // 08h
  ///
  /// Register used to indicate presence of command or response data in the CRB buffer.
  ///
  UINT32 CrbControlStart;          // 0Ch
  ///
  /// Register used to configure and respond to interrupts.
  ///
  UINT32 CrbInterruptEnable;       // 10h
  UINT32 CrbInterruptStatus;       // 14h
  ///
  /// Size of the Command buffer.
  ///
  UINT32 CrbControlCommandSize;    // 18h
  ///
  /// Command buffer start address
  ///
  UINT32 CrbControlCommandAddressLow;  // 1Ch
  UINT32 CrbControlCommandAddressHigh; // 20h
  ///
  /// Size of the Response buffer
  ///
  UINT32 CrbControlResponseSize;   // 24h
  ///
  /// Address of the start of the Response buffer
  ///
  UINT64 CrbControlResponseAddrss; // 28h
} PLATFORM_TPM2_CONTROL_AREA;

//
// Define pointer types used to access TPM2 Control Area
//
typedef PLATFORM_TPM2_CONTROL_AREA *PLATFORM_TPM2_CONTROL_AREA_PTR;

//
// Define bits of CRB Control Area Request Register
//

///
/// Used by Software to indicate transition the TPM to and from the Idle state
/// 1: Set by Software to indicate response has been read from the response buffer
/// and TPM can transition to Idle.
/// 0: Cleared to 0 by TPM to acknowledge the request when TPM enters Idle state.
///
#define CRB_CONTROL_AREA_REQUEST_GO_IDLE              BIT1

///
/// Used by Software to request the TPM transition to the Ready State.
/// 1: Set to 1 by Software to indicate the TPM should be ready to receive a command.
/// 0: Cleared to 0 by TPM to acknowledge the request.
///
#define CRB_CONTROL_AREA_REQUEST_COMMAND_READY        BIT0

//
// Define bits of CRB Control Area Status Register
//

///
/// Used by TPM to indicate it is in the Idle State
/// 1: Set by TPM when in the Idle State.
/// 0: Cleared by TPM when TPM transitions to the Ready State.
///
#define CRB_CONTROL_AREA_STATUS_TPM_IDLE              BIT1

///
/// Used by the TPM to indicate current status.
/// 1: Set by TPM to indicate a FATAL Error
/// 0: Indicates TPM is operational
///
#define CRB_CONTROL_AREA_STATUS_TPM_STATUS            BIT0

//
// Define bits of CRB Control Cancel Register
//

///
/// Used by software to cancel command processing Reads return correct value
/// Writes (0000 0001h): Cancel a command
/// Writes (0000 0000h): Clears field when command has been cancelled
///
#define CRB_CONTROL_CANCEL                            BIT0

//
// Define bits of CRB Control Start Register
//

///
/// When set by software, indicates a command is ready for processing.
/// Writes (0000 0001h): TPM transitions to Command Execution
/// Writes (0000 0000h): TPM clears this field and transitions to Command Completion
///
#define CRB_CONTROL_START                             BIT0

//
// Restore original structure alignment
//
#pragma pack ()

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
  IN     UINT32 InputParameterBlockSize,
  IN     UINT8  *InputParameterBlock,
  IN OUT UINT32 *OutputParameterBlockSize,
  IN     UINT8  *OutputParameterBlock
  );

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
  );

EFI_STATUS
EFIAPI
Tpm2ArmCrbInitialize (
  VOID
  );

#endif /* TPM2_ARM_CRB_H_ */
