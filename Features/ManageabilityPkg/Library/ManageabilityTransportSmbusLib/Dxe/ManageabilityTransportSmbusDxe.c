/** @file
  SMBus instance of Manageability Transport Library

  Copyright (C) 2023 Ampere Computing LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <IndustryStandard/IpmiSsif.h>
#include <IndustryStandard/IpmiNetFnApp.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ManageabilityTransportLib.h>
#include <Library/ManageabilityTransportIpmiLib.h>
#include <Library/ManageabilityTransportHelperLib.h>

#include "SmbusCommon.h"

// Initialize SSIF Interface capability
IPMI_SSIF_CAPABILITY  mIpmiSsifCapility = {
  .MaxRequestSize     = 0x20,
  .MaxResponseSize    = 0x20,
  .PecSupport         = FALSE,
  .TransactionSupport = 0
};

#define MANAGEABILITY_SMBUS_INIT_RETRY  10

EFI_GUID  *SmbusSupportedManageabilityProtocol[] = {
  &gManageabilityProtocolIpmiGuid
};

UINT8  mNumberOfSupportedProtocol =
  (sizeof (SmbusSupportedManageabilityProtocol) /sizeof (EFI_GUID *));

MANAGEABILITY_TRANSPORT_SMBUS  *mSingleSessionToken2 = NULL;

/**
  This function enables submitting Ipmi command via SSIF interface.

  @param[in]         This              This point for IPMI_PROTOCOL structure.
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
IpmiSsifCmd (
  IN     UINT8   NetFunction,
  IN     UINT8   Command,
  IN     UINT8   *RequestData,
  IN     UINT32  RequestDataSize,
  OUT    UINT8   *ResponseData,
  IN OUT UINT32  *ResponseDataSize
  )
{
  EFI_STATUS  Status;
  UINT8       Buffer[IPMI_SSIF_MAX_INPUT_MESSAGE_SIZE];
  UINT32      BufferSize;

  if ((RequestDataSize + 2) > mIpmiSsifCapility.MaxRequestSize) {
    return EFI_INVALID_PARAMETER;
  }

  BufferSize = IPMI_SSIF_MAX_INPUT_MESSAGE_SIZE;

  Status = IpmiSsifCommonCmd (NetFunction, Command, RequestData, RequestDataSize, Buffer, &BufferSize);

  if (EFI_ERROR (Status)) {
    *ResponseDataSize = 0;
    return Status;
  }

  if ((*ResponseDataSize + 2) < BufferSize) {
    return EFI_INVALID_PARAMETER;
  }

  *ResponseDataSize = BufferSize - 2;
  CopyMem (ResponseData, &Buffer[2], *ResponseDataSize);

  return Status;
}

/**
  This function initializes the transport interface.

  @param [in]  TransportToken           The transport token acquired through
                                        AcquireTransportSession function.
  @param [in]  HardwareInfo             The hardware information
                                        assigned to KCS transport interface.

  @retval      EFI_SUCCESS              Transport interface is initialized
                                        successfully.
  @retval      EFI_INVALID_PARAMETER    The invalid transport token.
  @retval      EFI_NOT_READY            The transport interface works fine but
  @retval                               is not ready.
  @retval      EFI_DEVICE_ERROR         The transport interface has problems.
  @retval      EFI_ALREADY_STARTED      Teh protocol interface has already initialized.
  @retval      Otherwise                Other errors.

**/
EFI_STATUS
EFIAPI
SmbusTransportInit (
  IN  MANAGEABILITY_TRANSPORT_TOKEN                 *TransportToken,
  IN  MANAGEABILITY_TRANSPORT_HARDWARE_INFORMATION  HardwareInfo OPTIONAL
  )
{
  EFI_STATUS                                            Status;
  IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_REQUEST        Request;
  IPMI_GET_SYSTEM_INTERFACE_SSIF_CAPABILITIES_RESPONSE  Response;
  UINT32                                                ResponseSize;
  UINTN                                                 Index;

  Request.Uint8 = IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_INTERFACE_TYPE_SSIF;
  ResponseSize  = sizeof (Response);

  for (Index = 0; Index < MANAGEABILITY_SMBUS_INIT_RETRY; Index++) {
    Status = IpmiSsifCmd (
               IPMI_NETFN_APP,
               IPMI_APP_GET_SYSTEM_INTERFACE_CAPABILITIES,
               (UINT8 *)&Request,
               sizeof (Request),
               (UINT8 *)&Response,
               &ResponseSize
               );
    if (EFI_ERROR (Status)) {
      continue;
    }

    mIpmiSsifCapility.MaxRequestSize     = Response.InputMsgSize;
    mIpmiSsifCapility.MaxResponseSize    = Response.OutputMsgSize;
    mIpmiSsifCapility.PecSupport         = Response.InterfaceCap.Bits.PecSupport;
    mIpmiSsifCapility.TransactionSupport = Response.InterfaceCap.Bits.TransactionSupport;
    DEBUG ((
      DEBUG_ERROR,
      "SSIF Capabilities transaction 0x%02X, insize %x, outsize %x, pec %x\n",
      mIpmiSsifCapility.TransactionSupport,
      mIpmiSsifCapility.MaxRequestSize,
      mIpmiSsifCapility.MaxResponseSize,
      mIpmiSsifCapility.PecSupport
      ));

    break;
  }

  return EFI_SUCCESS;
}

/**
  This function returns the transport interface status.
  The generic EFI_STATUS is returned to caller directly, The additional
  information of transport interface could be optionally returned in
  TransportAdditionalStatus to describes the status that can't be
  described obviously through EFI_STATUS.
  See the definition of MANAGEABILITY_TRANSPORT_STATUS.

  @param [in]   TransportToken             The transport token acquired through
                                           AcquireTransportSession function.
  @param [out]  TransportAdditionalStatus  The additional status of transport
                                           interface.
                                           NULL means no additional status of this
                                           transport interface.

  @retval      EFI_SUCCESS              Transport interface status is returned.
  @retval      EFI_INVALID_PARAMETER    The invalid transport token.
  @retval      EFI_DEVICE_ERROR         The transport interface has problems to return
  @retval      EFI_UNSUPPORTED          The transport interface doesn't have status report.
               Otherwise                Other errors.

**/
EFI_STATUS
EFIAPI
SmbusTransportStatus (
  IN  MANAGEABILITY_TRANSPORT_TOKEN              *TransportToken,
  OUT MANAGEABILITY_TRANSPORT_ADDITIONAL_STATUS  *TransportAdditionalStatus OPTIONAL
  )
{
  if (TransportToken == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid transport token.\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  if (TransportAdditionalStatus == NULL) {
    return EFI_SUCCESS;
  }

  return EFI_SUCCESS;
}

/**
  This function resets the transport interface.
  The generic EFI_STATUS is returned to caller directly after reseting transport
  interface. The additional information of transport interface could be optionally
  returned in TransportAdditionalStatus to describes the status that can't be
  described obviously through EFI_STATUS.
  See the definition of MANAGEABILITY_TRANSPORT_ADDITIONAL_STATUS.

  @param [in]   TransportToken             The transport token acquired through
                                           AcquireTransportSession function.
  @param [out]  TransportAdditionalStatus  The additional status of specific transport
                                           interface after the reset.
                                           NULL means no additional status of this
                                           transport interface.

  @retval      EFI_SUCCESS              Transport interface status is returned.
  @retval      EFI_INVALID_PARAMETER    The invalid transport token.
  @retval      EFI_TIMEOUT              The reset process is time out.
  @retval      EFI_DEVICE_ERROR         The transport interface has problems to return
                                        status.
               Otherwise                Other errors.

**/
EFI_STATUS
EFIAPI
SmbusTransportReset (
  IN  MANAGEABILITY_TRANSPORT_TOKEN              *TransportToken,
  OUT MANAGEABILITY_TRANSPORT_ADDITIONAL_STATUS  *TransportAdditionalStatus OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

/**
  This function transmit the request over target transport interface.
  The generic EFI_STATUS is returned to caller directly after reseting transport
  interface. The additional information of transport interface could be optionally
  returned in TransportAdditionalStatus to describes the status that can't be
  described obviously through EFI_STATUS.
  See the definition of MANAGEABILITY_TRANSPORT_ADDITIONAL_STATUS.

  @param [in]  TransportToken           The transport token acquired through
                                        AcquireTransportSession function.
  @param [in]  TransferToken            The transfer token, see the definition of
                                        MANAGEABILITY_TRANSFER_TOKEN.

  @retval      The EFI status is returned in MANAGEABILITY_TRANSFER_TOKEN.

**/
VOID
EFIAPI
SmbusTransportTransmitReceive (
  IN  MANAGEABILITY_TRANSPORT_TOKEN  *TransportToken,
  IN  MANAGEABILITY_TRANSFER_TOKEN   *TransferToken
  )
{
  EFI_STATUS                           Status;
  MANAGEABILITY_IPMI_TRANSPORT_HEADER  *TransmitHeader;

  if ((TransportToken == NULL) || (TransferToken == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid transport token or transfer token.\n", __FUNCTION__));
    return;
  }

  TransmitHeader = (MANAGEABILITY_IPMI_TRANSPORT_HEADER *)TransferToken->TransmitHeader;
  if (TransmitHeader == NULL) {
    TransferToken->TransferStatus = EFI_INVALID_PARAMETER;
    return;
  }

  Status = IpmiSsifCmd (
             TransmitHeader->NetFn,
             TransmitHeader->Command,
             TransferToken->TransmitPackage.TransmitPayload,
             TransferToken->TransmitPackage.TransmitSizeInByte,
             TransferToken->ReceivePackage.ReceiveBuffer,
             &TransferToken->ReceivePackage.ReceiveSizeInByte
             );
  TransferToken->TransferStatus = Status;
}

/**
  This function acquires to create a transport session to transmit manageability
  packet. A transport token is returned to caller for the follow up operations.

  @param [in]   ManageabilityProtocolSpec  The protocol spec the transport interface is acquired.
  @param [out]  TransportToken             The pointer to receive the transport token created by
                                           the target transport interface library.
  @retval       EFI_SUCCESS                Token is created successfully.
  @retval       EFI_OUT_OF_RESOURCES       Out of resource to create a new transport session.
  @retval       EFI_UNSUPPORTED            Protocol is not supported on this transport interface.
  @retval       Otherwise                  Other errors.

**/
EFI_STATUS
AcquireTransportSession (
  IN  EFI_GUID                       *ManageabilityProtocolSpec,
  OUT MANAGEABILITY_TRANSPORT_TOKEN  **TransportToken
  )
{
  EFI_STATUS                     Status;
  MANAGEABILITY_TRANSPORT_SMBUS  *SmbusTransportToken;
  MANAGEABILITY_TRANSPORT        *Transport;
  MANAGEABILITY_TRANSPORT_TOKEN  *Token;

  if (ManageabilityProtocolSpec == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (TransportToken == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // check weather support IPMI/MCTP or not
  Status = HelperManageabilityCheckSupportedSpec (
             &gManageabilityTransportI2CGuid,
             SmbusSupportedManageabilityProtocol,
             mNumberOfSupportedProtocol,
             ManageabilityProtocolSpec
             );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  // not support multiple session
  if (mSingleSessionToken2 != NULL) {
    DEBUG ((DEBUG_ERROR, "%a: This manageability transport library only supports one session transport token.\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  SmbusTransportToken = AllocateZeroPool (sizeof (MANAGEABILITY_TRANSPORT_SMBUS));
  if (SmbusTransportToken == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to allocate memory for Token -> bug\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  Token = &SmbusTransportToken->Token;

  Transport = AllocateZeroPool (sizeof (MANAGEABILITY_TRANSPORT));
  if (Transport == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to allocate memory for Transport -> bug\n", __func__));
    FreePool (SmbusTransportToken);
    return EFI_OUT_OF_RESOURCES;
  }

  Token->Transport = Transport;

  SmbusTransportToken->Signature                 = MANAGEABILITY_TRANSPORT_SMBUS_SIGNATURE;
  Token->ManageabilityProtocolSpecification      = ManageabilityProtocolSpec;
  Transport->TransportVersion                    = MANAGEABILITY_TRANSPORT_TOKEN_VERSION;
  Transport->ManageabilityTransportSpecification = &gManageabilityTransportI2CGuid;
  Transport->TransportName                       = L"SMBUS_I2C";
  Transport->Function.Version1_0                 = AllocateZeroPool (sizeof (MANAGEABILITY_TRANSPORT_FUNCTION_V1_0));
  if (Transport->Function.Version1_0 == NULL) {
    FreePool (SmbusTransportToken);
    FreePool (Transport);
    return EFI_OUT_OF_RESOURCES;
  }

  Transport->Function.Version1_0->TransportInit            = SmbusTransportInit;
  Transport->Function.Version1_0->TransportReset           = SmbusTransportReset;
  Transport->Function.Version1_0->TransportStatus          = SmbusTransportStatus;
  Transport->Function.Version1_0->TransportTransmitReceive = SmbusTransportTransmitReceive;
  mSingleSessionToken2                                     = SmbusTransportToken;
  *TransportToken                                          = Token;

  return EFI_SUCCESS;
}

/**
  This function returns the transport capabilities.

  @param [out]  TransportFeature        Pointer to receive transport capabilities.
                                        See the definitions of
                                        MANAGEABILITY_TRANSPORT_CAPABILITY.

**/
VOID
GetTransportCapability (
  OUT MANAGEABILITY_TRANSPORT_CAPABILITY  *TransportCapability
  )
{
  *TransportCapability = 0;
}

/**
  This function releases the manageability session.

  @param [in]  TransportToken   The transport token acquired through
                                AcquireTransportSession.
  @retval      EFI_SUCCESS      Token is released successfully.
               Otherwise        Other errors.

**/
EFI_STATUS
ReleaseTransportSession (
  IN MANAGEABILITY_TRANSPORT_TOKEN  *TransportToken
  )
{
  return EFI_SUCCESS;
}
