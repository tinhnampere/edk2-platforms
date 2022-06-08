/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef IPMI_SSIF_COMMON_H_
#define IPMI_SSIF_COMMON_H_

#include <Library/PcdLib.h>

#define IPMI_LUN_NUMBER       (FixedPcdGet8 (PcdIpmiLunNumber))
#define IPMI_MAX_LUN          0x3
#define IPMI_MAX_NETFUNCTION  0x3F

#define IPMI_SSIF_BLOCK_LEN  0x20

#define IPMI_SSIF_SINGLE_PART_WRITE_SMBUS_CMD        0x02
#define IPMI_SSIF_MULTI_PART_WRITE_START_SMBUS_CMD   0x06
#define IPMI_SSIF_MULTI_PART_WRITE_MIDDLE_SMBUS_CMD  0x07
#define IPMI_SSIF_MULTI_PART_WRITE_END_SMBUS_CMD     0x08

#define IPMI_SSIF_SINGLE_PART_READ_SMBUS_CMD        0x03
#define IPMI_SSIF_MULTI_PART_READ_START_SMBUS_CMD   0x03
#define IPMI_SSIF_MULTI_PART_READ_MIDDLE_SMBUS_CMD  0x09
#define IPMI_SSIF_MULTI_PART_READ_END_SMBUS_CMD     0x09
#define IPMI_SSIF_MULTI_PART_READ_RETRY_SMBUS_CMD   0x0A

#define IPMI_SSIF_MULTI_PART_READ_START_SIZE      30
#define IPMI_SSIF_MULTI_PART_READ_START_PATTERN1  0x0
#define IPMI_SSIF_MULTI_PART_READ_START_PATTERN2  0x1
#define IPMI_SSIF_MULTI_PART_READ_END_PATTERN     0xFF

#define IPMI_SSIF_SLAVE_ADDRESS  (FixedPcdGet8 (PcdBmcSlaveAddr))

#define IPMI_SSIF_REQUEST_RETRY_COUNT     (FixedPcdGet32 (PcdIpmiSsifRequestRetryCount))
#define IPMI_SSIF_REQUEST_RETRY_INTERVAL  (FixedPcdGet32 (PcdIpmiSsifRequestRetryInterval))

#define IPMI_SSIF_RESPONSE_RETRY_COUNT     (FixedPcdGet32 (PcdIpmiSsifResponseRetryCount))
#define IPMI_SSIF_RESPONSE_RETRY_INTERVAL  (FixedPcdGet32 (PcdIpmiSsifResponseRetryInterval))

#define SSIF_SINGLE_PART_RW  0x0
#define SSIF_START_END_RW    0x1
#define SSIF_MULTI_PART_RW   0x2

//
// Initialize SSIF Interface capability
//
extern BOOLEAN  mPecSupport;
extern UINT8    mMaxRequestSize;
extern UINT8    mMaxResponseSize;
extern UINT8    mTransactionSupport;

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
  );

#endif // IPMI_SSIF_COMMON_H_
