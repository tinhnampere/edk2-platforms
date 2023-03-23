/** @file
  SMBus instance of Manageability Transport Library.

  Copyright (c) 2023, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/IpmiSsif.h>
#include <IndustryStandard/IpmiNetFnApp.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SmbusLib.h>
#include <Library/TimerLib.h>
#include <Protocol/IpmiProtocol.h>
#include <Library/ManageabilityTransportLib.h>

#include "SmbusCommon.h"

#define IPMI_SSIF_SLAVE_ADDRESS            (FixedPcdGet8 (PcdIpmiSmbusSlaveAddr))
#define IPMI_SSIF_REQUEST_RETRY_COUNT      (FixedPcdGet32 (PcdIpmiSsifRequestRetryCount))
#define IPMI_SSIF_REQUEST_RETRY_INTERVAL   (FixedPcdGet32 (PcdIpmiSsifRequestRetryInterval))
#define IPMI_SSIF_RESPONSE_RETRY_COUNT     (FixedPcdGet32 (PcdIpmiSsifResponseRetryCount))
#define IPMI_SSIF_RESPONSE_RETRY_INTERVAL  (FixedPcdGet32 (PcdIpmiSsifResponseRetryInterval))

/**
  This function writes an IPMI SSIF request.

  @param[in]   RequestData       Command Request Data.
  @param[in]   RequestDataSize   Size of Command Request Data.

  @retval EFI_SUCCESS            The command byte stream was successfully submit
                                 to the device and a response was successfully received.
  @retval other                  Failed to write data to the device.
**/
EFI_STATUS
SsifWriteRequest (
  IN UINT8   *RequestData,
  IN UINT32  RequestDataSize
  )
{
  EFI_STATUS                  Status;
  UINT8                       IpmiCmd;
  UINT8                       Idx;
  UINT8                       WriteLen;
  UINTN                       SmBusAddress;
  UINTN                       TotalPacket;
  IPMI_SSIF_PACKET_ATTRIBUTE  Attribute;
  BOOLEAN                     PecSupport;

  ASSERT (RequestData != NULL && RequestDataSize > 0);

  PecSupport = mIpmiSsifCapility.PecSupport;

  TotalPacket = (RequestDataSize / IPMI_SSIF_BLOCK_LEN) + !!(RequestDataSize % IPMI_SSIF_BLOCK_LEN);

  if ((TotalPacket > 1) &&
      (mIpmiSsifCapility.TransactionSupport ==
       IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_SSIF_TRANSACTION_SUPPORT_SINGLE_PARTITION_RW))
  {
    return EFI_UNSUPPORTED;
  }

  if ((TotalPacket > 2) &&
      (mIpmiSsifCapility.TransactionSupport !=
       IPMI_GET_SYSTEM_INTERFACE_CAPABILITIES_SSIF_TRANSACTION_SUPPORT_MULTI_PARTITION_RW_WITH_MIDDLE))
  {
    return EFI_UNSUPPORTED;
  }

  for (Idx = 0, Attribute = TotalPacket > 1 ? IPMI_SSIF_START_PACKET : IPMI_SSIF_SINGLE_PACKET;
       Idx < TotalPacket;
       Idx++)
  {
    switch (Attribute) {
      case IPMI_SSIF_START_PACKET:
        IpmiCmd   = IPMI_SSIF_MULTI_PART_WRITE_START_SMBUS_CMD;
        WriteLen  = IPMI_SSIF_BLOCK_LEN;
        Attribute = (TotalPacket > 2) ? IPMI_SSIF_MIDDLE_PACKET : IPMI_SSIF_END_PACKET;
        break;

      case IPMI_SSIF_MIDDLE_PACKET:
        IpmiCmd  = IPMI_SSIF_MULTI_PART_WRITE_MIDDLE_SMBUS_CMD;
        WriteLen = IPMI_SSIF_BLOCK_LEN;

        if ((Idx + 2) == TotalPacket) {
          Attribute = IPMI_SSIF_END_PACKET;
        }

        break;

      case IPMI_SSIF_END_PACKET:
      case IPMI_SSIF_SINGLE_PACKET:
        IpmiCmd = (Attribute ==  IPMI_SSIF_SINGLE_PACKET) ?
                  IPMI_SSIF_SINGLE_PART_WRITE_SMBUS_CMD : IPMI_SSIF_MULTI_PART_WRITE_END_SMBUS_CMD;
        WriteLen = RequestDataSize - Idx * IPMI_SSIF_BLOCK_LEN;
        break;

      default:
        IpmiCmd  = 0;
        WriteLen = 0;
        ASSERT (FALSE);
    }

    SmBusAddress = SMBUS_LIB_ADDRESS (IPMI_SSIF_SLAVE_ADDRESS, IpmiCmd, WriteLen, PecSupport);
    SmBusWriteBlock (SmBusAddress, &RequestData[Idx * IPMI_SSIF_BLOCK_LEN], &Status);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
UpdateBuffer (
  IN OUT VOID    *Buffer,
  IN VOID        *Data,
  IN OUT UINT32  *BufferIdx,
  IN OUT UINT32  BufferLen,
  IN UINT32      DataLen
  )
{
  UINT8  *BufferTemp;

  if (DataLen == 0) {
    return EFI_DEVICE_ERROR;
  }

  if ((*BufferIdx + DataLen) > BufferLen) {
    *BufferIdx = 0;
    return EFI_BUFFER_TOO_SMALL;
  }

  BufferTemp = (UINT8 *)Buffer;

  CopyMem (&BufferTemp[*BufferIdx], Data, DataLen);

  *BufferIdx += DataLen;
  return EFI_SUCCESS;
}

/**
  This function reads an IPMI SSIF request.

  @param[out]        ResponseData      Command Response Data. The completion code is the first byte of response data.
                                       This also contains NetFn and IpmiCommand
  @param[in, out]    ResponseDataSize  Size of Command Response Data.

  @retval EFI_SUCCESS                  The command byte stream was successfully submit to the device and a response was successfully received.
  @retval other                        Failed to write data to the device.
**/
EFI_STATUS
SsifReadResponse (
  OUT    UINT8   *ResponseData,
  IN OUT UINT32  *ResponseDataSize
  )
{
  BOOLEAN                     Done;
  EFI_STATUS                  Status;
  IPMI_SSIF_PACKET_ATTRIBUTE  Attribute;
  IPMI_SSIF_RESPONSE_HEADER   *Header;
  UINT32                      CopiedLen;
  UINT8                       BlockNumber;
  UINT8                       IpmiCommand;
  UINT8                       ReadLen;
  UINT8                       ResponseBuffer[IPMI_SSIF_BLOCK_LEN];
  UINT8                       TryCount;
  UINT8                       WriteRetryData;
  UINTN                       SmbusAddress;
  BOOLEAN                     PecSupport;

  ASSERT (ResponseData != NULL && ResponseDataSize != NULL);

  Attribute   = IPMI_SSIF_START_PACKET;
  BlockNumber = 0;
  CopiedLen   = 0;
  Done        = FALSE;
  Header      = (IPMI_SSIF_RESPONSE_HEADER *)ResponseBuffer;
  IpmiCommand = IPMI_SSIF_MULTI_PART_READ_START_SMBUS_CMD;
  TryCount    = 0;
  PecSupport  = mIpmiSsifCapility.PecSupport;

  while (!Done) {
    SmbusAddress = SMBUS_LIB_ADDRESS (IPMI_SSIF_SLAVE_ADDRESS, IpmiCommand, 0, PecSupport);
    ReadLen      = SmBusReadBlock (SmbusAddress, ResponseBuffer, &Status);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (ReadLen == 0) {
      return EFI_NO_RESPONSE;
    }

    if (ReadLen > IPMI_SSIF_BLOCK_LEN) {
      Status = UpdateBuffer (ResponseData, NULL, NULL, 0, 0);
      Done   = TRUE;
    }

    switch (Attribute) {
      case IPMI_SSIF_START_PACKET:
        if ((ReadLen == IPMI_SSIF_BLOCK_LEN) &&
            (Header->ReadStart.StartPattern[0] == IPMI_SSIF_MULTI_PART_READ_START_PATTERN1) &&
            (Header->ReadStart.StartPattern[1] == IPMI_SSIF_MULTI_PART_READ_START_PATTERN2))
        {
          // Multiple read
          Status = UpdateBuffer (
                     ResponseData,
                     &ResponseBuffer[OFFSET_OF (IPMI_SSIF_RESPONSE_HEADER, ReadStart.NetFunc)],
                     &CopiedLen,
                     *ResponseDataSize,
                     IPMI_SSIF_BLOCK_LEN - OFFSET_OF (IPMI_SSIF_RESPONSE_HEADER, ReadStart.NetFunc)
                     );
          if (EFI_ERROR (Status)) {
            Done = TRUE;
          }

          Attribute   = IPMI_SSIF_MIDDLE_PACKET;
          IpmiCommand = IPMI_SSIF_MULTI_PART_READ_MIDDLE_SMBUS_CMD;
        } else {
          // Single read
          Status = UpdateBuffer (ResponseData, ResponseBuffer, &CopiedLen, *ResponseDataSize, ReadLen);
          Done   = TRUE;
        }

        break;

      case IPMI_SSIF_MIDDLE_PACKET:
      case IPMI_SSIF_END_PACKET:
        // Ignore BlockNumber/EndPattern
        Status = UpdateBuffer (
                   ResponseData,
                   &ResponseBuffer[1],
                   &CopiedLen,
                   *ResponseDataSize,
                   ReadLen - sizeof (UINT8)
                   );
        if (EFI_ERROR (Status)) {
          Done = TRUE;
          break;
        }

        // Handle last packet
        if (Header->ReadEnd.EndPattern == IPMI_SSIF_MULTI_PART_READ_END_PATTERN) {
          Done = TRUE;
        } else {
          if (TryCount > 5) {
            Status = UpdateBuffer (ResponseData, NULL, NULL, 0, 0);
            Done   = TRUE;
            break;
          }

          // Verify BlockNumber
          if (Header->ReadMiddle.BlockNumber != BlockNumber) {
            // Miss a packet -> Retry
            SmbusAddress = SMBUS_LIB_ADDRESS (
                             IPMI_SSIF_SLAVE_ADDRESS,
                             IPMI_SSIF_MULTI_PART_READ_RETRY_SMBUS_CMD,
                             sizeof (WriteRetryData),
                             PecSupport
                             );
            WriteRetryData = BlockNumber;
            SmBusWriteBlock (SmbusAddress, &WriteRetryData, &Status);
            TryCount++;
          } else {
            TryCount = 0;
            BlockNumber++;
          }
        }

        Attribute   = IPMI_SSIF_MIDDLE_PACKET;
        IpmiCommand = IPMI_SSIF_MULTI_PART_READ_MIDDLE_SMBUS_CMD;
        break;

      default:
        ASSERT (FALSE);
    }
  }

  *ResponseDataSize = CopiedLen;

  return Status;
}

/**
  This function enables submitting Ipmi command via Ssif interface.

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
IpmiSsifCommonCmd (
  IN     UINT8   NetFunction,
  IN     UINT8   Command,
  IN     UINT8   *RequestData,
  IN     UINT32  RequestDataSize,
  OUT    UINT8   *ResponseData,
  IN OUT UINT32  *ResponseDataSize
  )
{
  EFI_STATUS                 Status;
  IPMI_SSIF_REQUEST_HEADER   *RequestHeader;
  IPMI_SSIF_RESPONSE_HEADER  *ResponseHeader;
  UINT32                     TempLength;
  UINT8                      RetryCount;

  TempLength     = OFFSET_OF (IPMI_SSIF_REQUEST_HEADER, Data);
  ResponseHeader = (IPMI_SSIF_RESPONSE_HEADER *)ResponseData;

  if (RequestData != NULL) {
    if (ResponseDataSize > 0) {
      TempLength += RequestDataSize;
    } else {
      return EFI_OUT_OF_RESOURCES;
    }
  }

  RequestHeader = AllocateZeroPool (TempLength);

  if (RequestHeader == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  RequestHeader->NetFunc = (UINT8)((NetFunction << 2) | (0 & 0x3));
  RequestHeader->Command = Command;
  CopyMem (RequestHeader->Data, RequestData, RequestDataSize);

  if (  (ResponseData == NULL)
     || (ResponseDataSize == NULL)
     || (*ResponseDataSize == 0))
  {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Invalid Response Parameter\n", __func__));
    goto Cleanup;
  }

  // Write Request
  RetryCount = 1;
  while (TRUE) {
    DEBUG ((DEBUG_ERROR, "%a: Write SSIF request %d\n", __func__, RetryCount));
    Status = SsifWriteRequest ((UINT8 *)RequestHeader, TempLength);
    if (!EFI_ERROR (Status)) {
      break;
    }

    if (++RetryCount > IPMI_SSIF_REQUEST_RETRY_COUNT) {
      DEBUG ((DEBUG_ERROR, "%a: Write Request error %r\n", __func__, Status));
      goto Cleanup;
    }

    MicroSecondDelay (IPMI_SSIF_REQUEST_RETRY_INTERVAL);
  }

  TempLength = *ResponseDataSize;     // Keep original DataSize
  RetryCount = 1;

  while (TRUE) {
    DEBUG ((DEBUG_ERROR, "%a: Read SSIF request %d\n", __func__, RetryCount));
    Status = SsifReadResponse (ResponseData, ResponseDataSize);
    if (!EFI_ERROR (Status)) {
      if ((ResponseHeader->SinglePartRead.NetFunc >> 2) !=
          ((RequestHeader->NetFunc >> 2) + 1))
      {
        DEBUG ((
          DEBUG_ERROR,
          "%a: BMC sent wrong answer %d and %d\n",
          __func__,
          ResponseHeader->SinglePartRead.NetFunc >> 2,
          (RequestHeader->NetFunc >> 2) + 1
          ));
        continue;
      }

      break;
    }

    if (Status == EFI_BUFFER_TOO_SMALL) {
      break;
    }

    if (++RetryCount > IPMI_SSIF_RESPONSE_RETRY_COUNT) {
      DEBUG ((DEBUG_ERROR, "%a: Read Response error %r\n", __FUNCTION__, Status));
      *ResponseDataSize = 0;
      goto Cleanup;
    }

    *ResponseDataSize = TempLength;
    MicroSecondDelay (IPMI_SSIF_RESPONSE_RETRY_INTERVAL);
  }

Cleanup:
  FreePool (RequestHeader);

  return Status;
}
