/** @file

  This module installs Boot Progress Dxe that report boot progress to SMPro.

  This module registers report status code listener to report boot progress
  to SMPro.

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Protocol/ReportStatusCodeHandler.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesLib.h>
#include <Pi/PiStatusCode.h>
#include <Library/DebugLib.h>
#include <Library/SMProLib.h>
#include <Library/SMProInterface.h>
#include <Library/AmpereCpuLib.h>

#define BIOS_BOOT_PROG_SET     1
#define BIOS_BOOT_STAGE        8

#define BOOT_STATE_MASK        0x0000FFFF
#define BOOT_STATE_SHIFT       0
#define STATUS_MASK            0xFFFF0000
#define STATUS_SHIFT           16

#define SOCKET_BASE_OFFSET     0x400000000000

#define BASE_REG   (FixedPcdGet64 (PcdSmproDbBaseReg))
#define MAILBOX    (FixedPcdGet32 (PcdSmproNsMailboxIndex))

typedef struct {
  UINT8 Byte;
  EFI_STATUS_CODE_VALUE Value;
} STATUS_CODE_TO_CHECKPOINT;

enum BOOT_PROGRESS_STATE {
  BOOT_NOTSTART = 0,
  BOOT_START    = 1,
  BOOT_COMPLETE = 2,
  BOOT_FAILED   = 3,
};

UINT32 DxeProgressCode[] = {
  (EFI_SOFTWARE_DXE_CORE | EFI_SW_DXE_CORE_PC_ENTRY_POINT),     // DXE Core is started
  (EFI_COMPUTING_UNIT_CHIPSET | EFI_CHIPSET_PC_DXE_HB_INIT),    // PCI host bridge initialization
  (EFI_SOFTWARE_DXE_CORE | EFI_SW_DXE_CORE_PC_HANDOFF_TO_NEXT), // Boot Device Selection (BDS) phase is started 
  (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_PC_BEGIN_CONNECTING_DRIVERS), // Driver connecting is started
  (EFI_IO_BUS_PCI | EFI_IOB_PC_INIT),            // PCI Bus initialization is started
  (EFI_IO_BUS_PCI | EFI_IOB_PCI_HPC_INIT),       // PCI Bus Hot Plug Controller Initialization
  (EFI_IO_BUS_PCI | EFI_IOB_PCI_BUS_ENUM),       // PCI Bus Enumeration
  (EFI_IO_BUS_PCI | EFI_IOB_PCI_RES_ALLOC),      // PCI Bus Request Resources
  (EFI_IO_BUS_PCI | EFI_IOB_PC_ENABLE),          // PCI Bus Assign Resources
  (EFI_PERIPHERAL_LOCAL_CONSOLE | EFI_P_PC_INIT),// Console Output devices connect
  (EFI_PERIPHERAL_KEYBOARD | EFI_P_PC_INIT),     // Console input devices connect
  (EFI_IO_BUS_LPC | EFI_IOB_PC_INIT),            // Super IO Initialization
  (EFI_IO_BUS_USB | EFI_IOB_PC_INIT),            // USB initialization is started
  (EFI_IO_BUS_USB | EFI_IOB_PC_RESET),           // USB Reset
  (EFI_IO_BUS_USB | EFI_IOB_PC_DETECT),          // USB Detect
  (EFI_IO_BUS_USB | EFI_IOB_PC_ENABLE),          // USB Enable
  (EFI_IO_BUS_SCSI | EFI_IOB_PC_INIT),           // SCSI initialization is started
  (EFI_IO_BUS_SCSI | EFI_IOB_PC_RESET),          // SCSI Reset
  (EFI_IO_BUS_SCSI | EFI_IOB_PC_DETECT),         // SCSI Detect
  (EFI_IO_BUS_SCSI | EFI_IOB_PC_ENABLE),         // SCSI Enable
  (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_PC_VERIFYING_PASSWORD),          // Setup Verifying Password
  (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_PC_USER_SETUP),                         // Start of Setup
  (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_PC_INPUT_WAIT),                         // Setup Input Wait
  (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_PC_READY_TO_BOOT_EVENT),         // Ready To Boot event
  (EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_BS_PC_EXIT_BOOT_SERVICES),           // Exit Boot Services event
  (EFI_SOFTWARE_EFI_RUNTIME_SERVICE | EFI_SW_RS_PC_SET_VIRTUAL_ADDRESS_MAP),   // Runtime Set Virtual Address MAP Begin
  (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_PC_VIRTUAL_ADDRESS_CHANGE_EVENT),// Runtime Set Virtual Address MAP End
  (EFI_SOFTWARE_EFI_RUNTIME_SERVICE | EFI_SW_RS_PC_RESET_SYSTEM),              // System Reset
  (EFI_IO_BUS_USB | EFI_IOB_PC_HOTPLUG),        // USB hot plug
  (EFI_IO_BUS_PCI | EFI_IOB_PC_HOTPLUG),        // PCI bus hot plug
  0 // Must ended by 0
};

UINT32 DxeErrorCode[] = {
  (EFI_SOFTWARE_DXE_CORE | EFI_SW_DXE_CORE_EC_NO_ARCH),  // Some of the Architectural Protocols are not available
  (EFI_IO_BUS_PCI | EFI_IOB_EC_RESOURCE_CONFLICT),       // PCI resource allocation error. Out of Resources
  (EFI_PERIPHERAL_LOCAL_CONSOLE | EFI_P_EC_NOT_DETECTED),// No Console Output Devices are found
  (EFI_PERIPHERAL_KEYBOARD | EFI_P_EC_NOT_DETECTED),     // No Console Input Devices are found
  (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_EC_INVALID_PASSWORD),      // Invalid password
  (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_EC_BOOT_OPTION_LOAD_ERROR),// Error loading Boot Option (LoadImage returned error)
  (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_EC_BOOT_OPTION_FAILED),    // Boot Option is failed (StartImage returned error)
  (EFI_COMPUTING_UNIT_MEMORY | EFI_CU_MEMORY_EC_UPDATE_FAIL),            // Flash update is failed
  (EFI_SOFTWARE_EFI_RUNTIME_SERVICE | EFI_SW_PS_EC_RESET_NOT_AVAILABLE), // Reset protocol is not available
  0 // Must end by 0
};

EFI_RSC_HANDLER_PROTOCOL *mRscHandlerProtocol = NULL;

STATIC UINT8             mBootstate = BOOT_START;

STATIC
BOOLEAN
StatusCodeFilter (
  UINT32                *Map,
  EFI_STATUS_CODE_VALUE Value
  )
{
  UINTN    Index = 0;

  while (Map[Index] != 0) {
    if (Map[Index] == Value) {
      return TRUE;
    }
    Index++;
  }
  return FALSE;
}

/**
 *   Send boot progress data to SMPro.
 *
 *   @param Data to send to SMPro.
 *   @return
 *      EFI_DEVICE_ERROR: Error while sending to SMPro.
 *      EFI_SUCCESS
 *
 **/
STATIC
EFI_STATUS
BootProgressSendSMpro (
  IN UINT32 Data1,
  IN UINT32 Data2
  )
{
  UINT32      NumSockets;
  UINT32      Msg;
  EFI_STATUS  Status;
  UINT32      Index;

  Msg = SMPRO_BOOT_PROCESS_ENCODE_MSG (BIOS_BOOT_PROG_SET, BIOS_BOOT_STAGE);

  NumSockets = GetNumberActiveSockets ();
  for (Index = 0; Index < NumSockets; Index++) {
    Status = SMProDBWr (MAILBOX, Msg, Data1, Data2, BASE_REG + (SOCKET_BASE_OFFSET * Index));
    if (EFI_ERROR (Status)) {
      return EFI_DEVICE_ERROR;
    }
  }

  return EFI_SUCCESS;
}

/**
  Report status code listener of Boot Progress Dxe.

  @param[in]  CodeType            Indicates the type of status code being reported.
  @param[in]  Value               Describes the current status of a hardware or software entity.
                                  This included information about the class and subclass that is used to
                                  classify the entity as well as an operation.
  @param[in]  Instance            The enumeration of a hardware or software entity within
                                  the system. Valid instance numbers start with 1.
  @param[in]  CallerId            This optional parameter may be used to identify the caller.
                                  This parameter allows the status code driver to apply different rules to
                                  different callers.
  @param[in]  Data                This optional parameter may be used to pass additional data.

  @retval EFI_SUCCESS             Status code is what we expected.
  @retval EFI_UNSUPPORTED         Status code not supported.

**/
EFI_STATUS
EFIAPI
BootProgressListenerDxe (
  IN EFI_STATUS_CODE_TYPE     CodeType,
  IN EFI_STATUS_CODE_VALUE    Value,
  IN UINT32                   Instance,
  IN EFI_GUID                 *CallerId,
  IN EFI_STATUS_CODE_DATA     *Data
  )
{
  BOOLEAN                     IsProgress = FALSE;
  BOOLEAN                     IsError = FALSE;

  if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_PROGRESS_CODE) {
    IsProgress= StatusCodeFilter (DxeProgressCode, Value);
  }
  else if ((CodeType & EFI_STATUS_CODE_TYPE_MASK) == EFI_ERROR_CODE) {
    IsError = StatusCodeFilter (DxeErrorCode, Value);
  }
  else {
    return EFI_SUCCESS;
  }

  if (!IsProgress && !IsError) {
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO,
    "BootProgressDxe: CodeType=0x%X Value=0x%X Instance=0x%X CallerIdGuid=%g Data=%p\n",
    CodeType, Value, Instance, CallerId, Data
    ));

  if (IsError) {
    mBootstate = BOOT_FAILED;
  }
  else if (Value == (EFI_SOFTWARE_DXE_BS_DRIVER | EFI_SW_DXE_BS_PC_READY_TO_BOOT_EVENT)) {
    /* Set boot complete when reach to ReadyToBoot event */
    mBootstate = BOOT_COMPLETE;
  }

  BootProgressSendSMpro (
    (((UINT32) mBootstate << BOOT_STATE_SHIFT) & BOOT_STATE_MASK) | (((UINT32) Value << STATUS_SHIFT) & STATUS_MASK),
    Value >> STATUS_SHIFT
  );

  if (Value == (EFI_SOFTWARE_EFI_BOOT_SERVICE | EFI_SW_BS_PC_EXIT_BOOT_SERVICES) &&
      mRscHandlerProtocol != NULL) {
    mRscHandlerProtocol->Unregister (BootProgressListenerDxe);
  }

  return EFI_SUCCESS;
}


/**
  The module Entry Point of the Firmware Performance Data Table DXE driver.

  @param[in]  ImageHandle    The firmware allocated handle for the EFI image.
  @param[in]  SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS    The entry point is executed successfully.
  @retval Other          Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
BootProgressDxeEntryPoint (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE    *SystemTable
  )
{
  EFI_STATUS               Status;

  //
  // Get Report Status Code Handler Protocol.
  //
  Status = gBS->LocateProtocol (&gEfiRscHandlerProtocolGuid, NULL, (VOID **) &mRscHandlerProtocol);
  ASSERT_EFI_ERROR (Status);

  //
  // Register report status code listener for OS Loader load and start.
  //
  if (!EFI_ERROR (Status)) {
    Status = mRscHandlerProtocol->Register (BootProgressListenerDxe, TPL_HIGH_LEVEL);
  }
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
