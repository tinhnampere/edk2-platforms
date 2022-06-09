/** @file
  IPMI Protocol implementation follows IPMI 2.0 Specification.

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PlatformBmcReadyLib.h>
#include <Library/SmbusLib.h>
#include <Library/TimerLib.h>
#include <Protocol/IpmiProtocol.h>

#include "IpmiSsifCommon.h"

/**
  Write SSIF request to BMC.

  @param[in]   RequestData       Command Request Data.
  @param[in]   RequestDataSize   Size of Command Request Data.

  @retval EFI_SUCCESS            The command byte stream was successfully submit to the device and a response was successfully received.
  @retval other                  Failed to write data to the device.
**/
EFI_STATUS
SsifWriteRequest (
  IN UINT8  *RequestData,
  IN UINT32 RequestDataSize
  )
{
  BOOLEAN     IsMultiPartWrite;
  EFI_STATUS  Status;
  UINT8       SsifCmd;
  UINT8       Idx;
  UINT8       MiddleCount;
  UINT8       WriteLen;

  ASSERT (RequestData != NULL && RequestDataSize > 0);

  IsMultiPartWrite = FALSE;
  Status = EFI_SUCCESS;

  if (RequestDataSize > IPMI_SSIF_BLOCK_LEN) {
    IsMultiPartWrite = TRUE;
    MiddleCount = ((RequestDataSize - 1) / IPMI_SSIF_BLOCK_LEN) - 1;

    if (  ((MiddleCount == 0) && (mTransactionSupport == SSIF_SINGLE_PART_RW))
       || ((MiddleCount > 0) && (mTransactionSupport != SSIF_MULTI_PART_RW)))
    {
      DEBUG ((DEBUG_ERROR, "%a: Unsupported Request transaction\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }
  }

  SsifCmd = IsMultiPartWrite ? IPMI_SSIF_MULTI_PART_WRITE_START_SMBUS_CMD
                               : IPMI_SSIF_SINGLE_PART_WRITE_SMBUS_CMD;
  WriteLen = IsMultiPartWrite ? IPMI_SSIF_BLOCK_LEN : RequestDataSize;

  SmBusWriteBlock (
    SMBUS_LIB_ADDRESS (
      IPMI_SSIF_SLAVE_ADDRESS,
      SsifCmd,
      WriteLen,
      mPecSupport
      ),
    RequestData,
    &Status
    );

  if (  EFI_ERROR (Status)
     || !IsMultiPartWrite)
  {
    goto Exit;
  }

  for (Idx = 1; Idx <= MiddleCount; Idx++) {
    SmBusWriteBlock (
      SMBUS_LIB_ADDRESS (
        IPMI_SSIF_SLAVE_ADDRESS,
        IPMI_SSIF_MULTI_PART_WRITE_MIDDLE_SMBUS_CMD,
        WriteLen,
        mPecSupport
        ),
      &RequestData[Idx * IPMI_SSIF_BLOCK_LEN],
      &Status
      );

    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  //
  // Remain RequestData for END
  //
  WriteLen = RequestDataSize - (MiddleCount + 1) * IPMI_SSIF_BLOCK_LEN;
  ASSERT (WriteLen > 0);
  SmBusWriteBlock (
    SMBUS_LIB_ADDRESS (
      IPMI_SSIF_SLAVE_ADDRESS,
      IPMI_SSIF_MULTI_PART_WRITE_END_SMBUS_CMD,
      WriteLen,
      mPecSupport
      ),
    &RequestData[(MiddleCount + 1) * IPMI_SSIF_BLOCK_LEN],
    &Status
    );

Exit:

  return Status;
}

/**
  Read SSIF response from BMC.

  @param[out]        ResponseData      Command Response Data. The completion code is the first byte of response data.
  @param[in, out]    ResponseDataSize  Size of Command Response Data.

  @retval EFI_SUCCESS                  The command byte stream was successfully submit to the device and a response was successfully received.
  @retval other                        Failed to write data to the device.
**/
EFI_STATUS
SsifReadResponse (
  OUT    UINT8  *ResponseData,
  IN OUT UINT32 *ResponseDataSize
  )
{
  BOOLEAN     IsMultiPartRead;
  EFI_STATUS  Status;
  UINT32      CopiedLen;
  UINT8       BlockNumber;
  UINT8       Offset;
  UINT8       ReadLen;
  UINT8       ResponseTemp[IPMI_SSIF_BLOCK_LEN];

  ASSERT (ResponseData != NULL && ResponseDataSize != NULL);

  BlockNumber     = 0;
  CopiedLen       = 0;
  IsMultiPartRead = FALSE;
  Offset = 2; // Ignore LUN and Command byte in return ResponseData
  Status = EFI_SUCCESS;

  ReadLen = SmBusReadBlock (
              SMBUS_LIB_ADDRESS (
                IPMI_SSIF_SLAVE_ADDRESS,
                IPMI_SSIF_SINGLE_PART_READ_SMBUS_CMD,
                0,  // Max block size
                mPecSupport
                ),
              ResponseTemp,
              &Status
              );

  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (ReadLen == 0) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  if (  (ReadLen == IPMI_SSIF_BLOCK_LEN)
     && (ResponseTemp[0] == IPMI_SSIF_MULTI_PART_READ_START_PATTERN1)
     && (ResponseTemp[1] == IPMI_SSIF_MULTI_PART_READ_START_PATTERN2))
  {
    Offset += 2;  // Ignore pattern1 and pattern2
    IsMultiPartRead = TRUE;
  }

  //
  // Copy ResponseData
  //
  ReadLen -= Offset;
  if (ReadLen > *ResponseDataSize) {
    ReadLen = *ResponseDataSize;
  }

  CopyMem (ResponseData, &ResponseTemp[Offset], ReadLen);
  CopiedLen = ReadLen;

  Offset = 1;  // Ignore block number
  while (IsMultiPartRead) {
    ReadLen = SmBusReadBlock (
                SMBUS_LIB_ADDRESS (
                  IPMI_SSIF_SLAVE_ADDRESS,
                  IPMI_SSIF_MULTI_PART_READ_MIDDLE_SMBUS_CMD,
                  0,
                  mPecSupport
                  ),
                ResponseTemp,
                &Status
                );

    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    if (ReadLen == 0) {
      DEBUG ((DEBUG_ERROR, "%a: Response data error\n", __FUNCTION__));
      Status = EFI_NOT_FOUND;
      goto Exit;
    }

    ReadLen -= Offset; // Ignore block number
    if (ReadLen > *ResponseDataSize - CopiedLen) {
      ReadLen = *ResponseDataSize - CopiedLen;
    }

    //
    // Copy to ResponseData if have space
    //
    if (ReadLen > 0) {
      CopyMem (&ResponseData[CopiedLen], &ResponseTemp[Offset], ReadLen);
      CopiedLen += ReadLen;
    }

    if (ResponseTemp[0] == IPMI_SSIF_MULTI_PART_READ_END_PATTERN) {
      break;
    }

    //
    // Verify BlockNumber
    //
    if (ResponseTemp[0] != BlockNumber++) {
      DEBUG ((DEBUG_ERROR, "%a: Block number incorrect\n", __FUNCTION__));
      Status = EFI_NOT_FOUND;
      goto Exit;
    }
  }

Exit:
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
  @retval EFI_OUT_OF_RESOURCES   The resource allcation is out of resource or data size error.
**/
EFI_STATUS
IpmiSsifCommonCmd (
  IN     UINT8  NetFunction,
  IN     UINT8  Command,
  IN     UINT8  *RequestData,
  IN     UINT32 RequestDataSize,
  OUT    UINT8  *ResponseData,
  IN OUT UINT32 *ResponseDataSize
  )
{
  EFI_STATUS  Status;
  UINT32      TempLength;
  UINT8       *RequestTemp;
  UINT8       RetryCount;

  DEBUG ((DEBUG_INFO, "%a Entry\n", __FUNCTION__));

  ASSERT (NetFunction <= IPMI_MAX_NETFUNCTION);
  ASSERT (IPMI_LUN_NUMBER <= IPMI_MAX_LUN);

  //
  // Check BMC ready
  //
  if (PlatformBmcReady () != TRUE) {
    return EFI_NOT_READY;
  }

  RequestTemp = AllocateZeroPool (RequestDataSize + 2);
  if (RequestTemp == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  RequestTemp[0] = (UINT8)((NetFunction << 2) | (IPMI_LUN_NUMBER & 0x3));
  RequestTemp[1] = Command;
  TempLength     = 2;

  if (RequestData != NULL) {
    if (RequestDataSize > 0) {
      CopyMem (&RequestTemp[2], RequestData, RequestDataSize);
      TempLength += RequestDataSize;
    } else {
      Status = EFI_OUT_OF_RESOURCES;
      DEBUG ((DEBUG_ERROR, "%a: Invalid Request info\n", __FUNCTION__));
      goto Cleanup;
    }
  }

  if (TempLength > mMaxRequestSize) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Request size defeats BMC capability\n", __FUNCTION__));
    goto Cleanup;
  }

  if (  (ResponseData == NULL)
     || (ResponseDataSize == NULL)
     || (*ResponseDataSize == 0))
  {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: Invalid Response info\n", __FUNCTION__));
    goto Cleanup;
  }

  //
  // Write Request
  //
  RetryCount = 1;
  while (TRUE) {
    DEBUG ((DEBUG_INFO, "%a: Write Request count = %d\n", __FUNCTION__, RetryCount));
    Status = SsifWriteRequest (RequestTemp, TempLength);
    if (!EFI_ERROR (Status)) {
      break;
    }

    if (++RetryCount > IPMI_SSIF_REQUEST_RETRY_COUNT) {
      DEBUG ((DEBUG_ERROR, "%a: Write Request error %r\n", __FUNCTION__, Status));
      goto Cleanup;
    }

    MicroSecondDelay (IPMI_SSIF_REQUEST_RETRY_INTERVAL);
  }

  //
  // TODO: Check for Alert
  //

  //
  // Read Response
  //
  TempLength = *ResponseDataSize; // Keep original DataSize

  RetryCount = 1;
  while (TRUE) {
    DEBUG ((DEBUG_INFO, "%a: Read Response count = %d\n", __FUNCTION__, RetryCount));
    Status = SsifReadResponse (ResponseData, ResponseDataSize);
    if (!EFI_ERROR (Status)) {
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
  FreePool (RequestTemp);

  return Status;
}
