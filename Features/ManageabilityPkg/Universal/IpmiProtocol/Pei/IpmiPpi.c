/** @file
  This file provides IPMI PPI implementation.

  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ManageabilityTransportLib.h>
#include <Library/ManageabilityTransportIpmiLib.h>
#include <Library/ManageabilityTransportHelperLib.h>
#include <Library/PeiServicesLib.h>

#include <Ppi/IpmiPpi.h>

#include "IpmiProtocolCommon.h"
#include "IpmiPpiInternal.h"

/**
  This service enables submitting commands via Ipmi.

  @param[in]         This              This point for PEI_IPMI_PPI structure.
  @param[in]         NetFunction       Net function of the command.
  @param[in]         Command           IPMI Command.
  @param[in]         RequestData       Command Request Data.
  @param[in]         RequestDataSize   Size of Command Request Data.
  @param[out]        ResponseData      Command Response Data. The completion code is the first byte of response data.
  @param[in, out]    ResponseDataSize  Size of Command Response Data.

  @retval EFI_SUCCESS            The command byte stream was successfully submit to the device and a response was successfully received.
  @retval EFI_NOT_FOUND          The command was not successfully sent to the device or a response was not successfully received from the device.
  @retval EFI_NOT_READY          Ipmi Device is not ready for Ipmi command access.
  @retval EFI_DEVICE_ERROR       Ipmi Device hardware error.
  @retval EFI_TIMEOUT            The command time out.
  @retval EFI_UNSUPPORTED        The command was not successfully sent to the device.
  @retval EFI_OUT_OF_RESOURCES   The resource allocation is out of resource or data size error.
**/
EFI_STATUS
EFIAPI
PeiIpmiSubmitCommand (
  IN     PEI_IPMI_PPI  *This,
  IN     UINT8         NetFunction,
  IN     UINT8         Command,
  IN     UINT8         *RequestData,
  IN     UINT32        RequestDataSize,
  OUT    UINT8         *ResponseData,
  IN OUT UINT32        *ResponseDataSize
  )
{
  EFI_STATUS            Status;
  PEI_IPMI_PPI_INTERNAL *PeiIpmiPpiinternal;

  PeiIpmiPpiinternal = MANAGEABILITY_IPMI_PPI_INTERNAL_FROM_LINK(This);
  Status = CommonIpmiSubmitCommand (
             PeiIpmiPpiinternal->TransportToken,
             NetFunction,
             Command,
             RequestData,
             RequestDataSize,
             ResponseData,
             ResponseDataSize
             );
  return Status;
}

/**
  The entry point of the Ipmi PPI PEIM.

  @param  FileHandle  Handle of the file being invoked.
  @param  PeiServices Describes the list of possible PEI Services.

  @retval EFI_SUCCESS   Indicates that Ipmi initialization completed successfully.
  @retval Others        Indicates that Ipmi initialization could not complete successfully.
**/
EFI_STATUS
EFIAPI
PeiIpmiEntry (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                                    Status;
  CHAR16                                        *TransportName;
  PEI_IPMI_PPI_INTERNAL                         *PeiIpmiPpiinternal;
  EFI_PEI_PPI_DESCRIPTOR                        *PpiDescriptor;
  MANAGEABILITY_TRANSPORT_CAPABILITY            TransportCapability;
  MANAGEABILITY_TRANSPORT_ADDITIONAL_STATUS     TransportAdditionalStatus;
  MANAGEABILITY_TRANSPORT_HARDWARE_INFORMATION  HardwareInformation;

  PeiIpmiPpiinternal = (PEI_IPMI_PPI_INTERNAL *)AllocateZeroPool (sizeof(PEI_IPMI_PPI_INTERNAL));
  if (PeiIpmiPpiinternal == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Not enough memory for PEI_IPMI_PPI_INTERNAL.\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }
  PpiDescriptor = (EFI_PEI_PPI_DESCRIPTOR *)AllocateZeroPool (sizeof(EFI_PEI_PPI_DESCRIPTOR));
  if (PpiDescriptor == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Not enough memory for EFI_PEI_PPI_DESCRIPTOR.\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  PeiIpmiPpiinternal->Signature = MANAGEABILITY_IPMI_PPI_INTERNAL_SIGNATURE;
  PeiIpmiPpiinternal->PeiIpmiPpi.IpmiSubmitCommand = PeiIpmiSubmitCommand;

  PpiDescriptor->Flags = EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST;
  PpiDescriptor->Guid  = &gPeiIpmiPpiGuid;
  PpiDescriptor->Ppi   = &PeiIpmiPpiinternal->PeiIpmiPpi;

  GetTransportCapability (&TransportCapability);
  Status = HelperAcquireManageabilityTransport (
             &gManageabilityProtocolIpmiGuid,
             &PeiIpmiPpiinternal->TransportToken
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to acquire transport interface for IPMI protocol - %r\n", __FUNCTION__, Status));
    return Status;
  }

  TransportName = HelperManageabilitySpecName (PeiIpmiPpiinternal->TransportToken->Transport->ManageabilityTransportSpecification);
  DEBUG ((DEBUG_INFO, "%a: IPMI protocol over %s.\n", __FUNCTION__, TransportName));

  //
  // Setup hardware information according to the transport interface.
  Status = SetupIpmiTransportHardwareInformation (
             PeiIpmiPpiinternal->TransportToken,
             &HardwareInformation
             );
  if (EFI_ERROR (Status)) {
    if (Status == EFI_UNSUPPORTED) {
      DEBUG ((DEBUG_ERROR, "%a: No hardware information of %s transport interface.\n", __FUNCTION__, TransportName));
    }

    return Status;
  }

  //
  // Initial transport interface with the hardware information assigned.
  Status = HelperInitManageabilityTransport (
             PeiIpmiPpiinternal->TransportToken,
             HardwareInformation,
             &TransportAdditionalStatus
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Install IPMI PPI.
  //
  Status = PeiServicesInstallPpi (PpiDescriptor);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to install IPMI PPI - %r\n", __FUNCTION__, Status));
  }

  return Status;
}
