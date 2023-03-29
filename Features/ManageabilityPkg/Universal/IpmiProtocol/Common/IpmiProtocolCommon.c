/** @file

  IPMI Manageability Protocol common file.

  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ManageabilityTransportIpmiLib.h>
#include <Library/ManageabilityTransportLib.h>

#include "IpmiProtocolCommon.h"

/**
  This functions setup the IPMI transport hardware information according
  to the specification of transport token acquired from transport library.

  @param[in]         TransportToken       The transport interface.
  @param[out]        HardwareInformation  Pointer to receive the hardware information.

  @retval EFI_SUCCESS            Hardware information is returned in HardwareInformation.
                                 Caller must free the memory allocated for HardwareInformation
                                 once it doesn't need it.
  @retval EFI_UNSUPPORTED        No hardware information for the specification specified
                                 in the transport token.
  #retval EFI_OUT_OF_RESOURCES   Not enough memory for MANAGEABILITY_TRANSPORT_KCS_HARDWARE_INFO.
**/
EFI_STATUS
SetupIpmiTransportHardwareInformation (
  IN   MANAGEABILITY_TRANSPORT_TOKEN                 *TransportToken,
  OUT  MANAGEABILITY_TRANSPORT_HARDWARE_INFORMATION  *HardwareInformation
  )
{
  MANAGEABILITY_TRANSPORT_KCS_HARDWARE_INFO  *KcsHardwareInfo;

  KcsHardwareInfo = AllocatePool (sizeof (MANAGEABILITY_TRANSPORT_KCS_HARDWARE_INFO));
  if (KcsHardwareInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Not enough memory for MANAGEABILITY_TRANSPORT_KCS_HARDWARE_INFO.\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  if (CompareGuid (&gManageabilityTransportKcsGuid, TransportToken->Transport->ManageabilityTransportSpecification)) {
    // This is KCS transport interface.
    KcsHardwareInfo->MemoryMap                    = MANAGEABILITY_TRANSPORT_KCS_IO_MAP_IO;
    KcsHardwareInfo->IoBaseAddress.IoAddress16    = IPMI_KCS_BASE_ADDRESS;
    KcsHardwareInfo->IoDataInAddress.IoAddress16  = IPMI_KCS_REG_DATA_IN;
    KcsHardwareInfo->IoDataOutAddress.IoAddress16 = IPMI_KCS_REG_DATA_OUT;
    KcsHardwareInfo->IoCommandAddress.IoAddress16 = IPMI_KCS_REG_COMMAND;
    KcsHardwareInfo->IoStatusAddress.IoAddress16  = IPMI_KCS_REG_STATUS;
    *HardwareInformation                          =
      (MANAGEABILITY_TRANSPORT_HARDWARE_INFORMATION)KcsHardwareInfo;
    return EFI_SUCCESS;
  } else if (CompareGuid (&gManageabilityTransportI2CGuid, TransportToken->Transport->ManageabilityTransportSpecification)) {
    return EFI_SUCCESS;
  }
  else {
    DEBUG ((DEBUG_ERROR, "%a: No implementation of setting hardware information.", __FUNCTION__));
    ASSERT (FALSE);
  }

  return EFI_UNSUPPORTED;
}

/**
  This functions setup the final header/body/trailer packets for
  the acquired transport interface.

  @param[in]         TransportToken  The transport interface.
  @param[in]         NetFunction     IPMI function.
  @param[in]         Command         IPMI command.
  @param[out]        PacketHeader    The pointer to receive header of request.
  @param[in, out]    PacketBody      The request body.
                                     When IN, it is the caller's request body.
                                     When OUT and NULL, the request body is not
                                     changed.
                                     When OUT and non-NULL, the request body is
                                     changed to conform the transport interface.
  @param[in, out]    PacketBodySize  The request body size.
                                     When OUT and non-zero, it is the new data
                                     length of request body.
                                     When OUT and zero, the request body is unchanged.
  @param[out]        PacketTrailer   The pointer to receive trailer of request.

  @retval EFI_SUCCESS            Request packet is returned.
  @retval EFI_UNSUPPORTED        Request packet is not returned because
                                 the unsupported transport interface.
**/
EFI_STATUS
SetupIpmiRequestTransportPacket (
  IN   MANAGEABILITY_TRANSPORT_TOKEN    *TransportToken,
  IN   UINT8                            NetFunction,
  IN   UINT8                            Command,
  OUT  MANAGEABILITY_TRANSPORT_HEADER   *PacketHeader OPTIONAL,
  IN OUT UINT8                          **PacketBody OPTIONAL,
  IN OUT UINT32                         *PacketBodySize OPTIONAL,
  OUT  MANAGEABILITY_TRANSPORT_TRAILER  *PacketTrailer OPTIONAL
  )
{
  MANAGEABILITY_IPMI_TRANSPORT_HEADER  *IpmiHeader;

  if (CompareGuid (&gManageabilityTransportKcsGuid, TransportToken->Transport->ManageabilityTransportSpecification)) {
    // This is KCS transport interface.
    IpmiHeader = AllocateZeroPool (sizeof (MANAGEABILITY_IPMI_TRANSPORT_HEADER));
    if (IpmiHeader == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    IpmiHeader->Command = Command;
    IpmiHeader->Lun     = 0;
    IpmiHeader->NetFn   = NetFunction;
    if (PacketHeader != NULL) {
      *PacketHeader = (MANAGEABILITY_TRANSPORT_HEADER *)IpmiHeader;
    }

    if (PacketTrailer != NULL) {
      *PacketTrailer = NULL;
    }

    if (PacketBody != NULL) {
      *PacketBody = NULL;
    }

    if (PacketBodySize != NULL) {
      *PacketBodySize = 0;
    }
  } else if (CompareGuid (&gManageabilityTransportI2CGuid, TransportToken->Transport->ManageabilityTransportSpecification)) {
    // This is SMBus transport interface
    IpmiHeader = AllocateZeroPool (sizeof (MANAGEABILITY_IPMI_TRANSPORT_HEADER));
    if (IpmiHeader == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    IpmiHeader->Command = Command;
    IpmiHeader->Lun     = 0;
    IpmiHeader->NetFn   = NetFunction;
    if (PacketHeader != NULL) {
      *PacketHeader = (MANAGEABILITY_TRANSPORT_HEADER *)IpmiHeader;
    }

    if (PacketTrailer != NULL) {
      *PacketTrailer = NULL;
    }

    if (PacketBody != NULL) {
      *PacketBody = NULL;
    }

    if (PacketBodySize != NULL) {
      *PacketBodySize = 0;
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a: No implementation of building up packet.", __FUNCTION__));
    ASSERT (FALSE);
  }

  return EFI_SUCCESS;
}

/**
  Common code to submit IPMI commands

  @param[in]         TransportToken    TRansport token.
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
CommonIpmiSubmitCommand (
  IN     MANAGEABILITY_TRANSPORT_TOKEN  *TransportToken,
  IN     UINT8                          NetFunction,
  IN     UINT8                          Command,
  IN     UINT8                          *RequestData OPTIONAL,
  IN     UINT32                         RequestDataSize,
  OUT    UINT8                          *ResponseData OPTIONAL,
  IN OUT UINT32                         *ResponseDataSize OPTIONAL
  )
{
  EFI_STATUS                                 Status;
  UINT8                                      *ThisRequestData;
  UINT32                                     ThisRequestDataSize;
  MANAGEABILITY_TRANSFER_TOKEN               TransferToken;
  MANAGEABILITY_TRANSPORT_HEADER             IpmiTransportHeader;
  MANAGEABILITY_TRANSPORT_TRAILER            IpmiTransportTrailer;
  MANAGEABILITY_TRANSPORT_ADDITIONAL_STATUS  TransportAdditionalStatus;

  if (TransportToken == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: No transport toke for IPMI\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  Status = TransportToken->Transport->Function.Version1_0->TransportStatus (
                                                             TransportToken,
                                                             &TransportAdditionalStatus
                                                             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Transport for IPMI has problem - (%r)\n", __FUNCTION__, Status));
    return Status;
  }

  ThisRequestData      = RequestData;
  ThisRequestDataSize  = RequestDataSize;
  IpmiTransportHeader  = NULL;
  IpmiTransportTrailer = NULL;
  Status               = SetupIpmiRequestTransportPacket (
                           TransportToken,
                           NetFunction,
                           Command,
                           &IpmiTransportHeader,
                           &ThisRequestData,
                           &ThisRequestDataSize,
                           &IpmiTransportTrailer
                           );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to build packets - (%r)\n", __FUNCTION__, Status));
    return Status;
  }

  ZeroMem (&TransferToken, sizeof (MANAGEABILITY_TRANSFER_TOKEN));
  TransferToken.TransmitHeader  = IpmiTransportHeader;
  TransferToken.TransmitTrailer = IpmiTransportTrailer;

  // Transmit packet.
  if ((ThisRequestData == NULL) || (ThisRequestDataSize == 0)) {
    // Transmit parameter were not changed by SetupIpmiRequestTransportPacket().
    TransferToken.TransmitPackage.TransmitPayload    = RequestData;
    TransferToken.TransmitPackage.TransmitSizeInByte = RequestDataSize;
  } else {
    TransferToken.TransmitPackage.TransmitPayload    = ThisRequestData;
    TransferToken.TransmitPackage.TransmitSizeInByte = ThisRequestDataSize;
  }

  TransferToken.TransmitPackage.TransmitTimeoutInMillisecond = MANAGEABILITY_TRANSPORT_NO_TIMEOUT;

  // Receive packet.
  TransferToken.ReceivePackage.ReceiveBuffer                = ResponseData;
  TransferToken.ReceivePackage.ReceiveSizeInByte            = *ResponseDataSize;
  TransferToken.ReceivePackage.TransmitTimeoutInMillisecond = MANAGEABILITY_TRANSPORT_NO_TIMEOUT;
  TransportToken->Transport->Function.Version1_0->TransportTransmitReceive (
                                                    TransportToken,
                                                    &TransferToken
                                                    );

  if (IpmiTransportHeader != NULL) {
    FreePool ((VOID *)IpmiTransportHeader);
  }

  if (IpmiTransportTrailer != NULL) {
    FreePool ((VOID *)IpmiTransportTrailer);
  }

  if (ThisRequestData != NULL) {
    FreePool ((VOID *)ThisRequestData);
  }

  // Return transfer status.
  //
  Status                    = TransferToken.TransferStatus;
  TransportAdditionalStatus = TransferToken.TransportAdditionalStatus;
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to send IPMI command.\n", __FUNCTION__));
    return Status;
  }

  if (ResponseDataSize != NULL) {
    *ResponseDataSize = TransferToken.ReceivePackage.ReceiveSizeInByte;
  }

  return Status;
}
