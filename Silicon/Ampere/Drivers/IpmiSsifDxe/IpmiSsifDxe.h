/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef IPMI_SSIF_DXE_H_
#define IPMI_SSIF_DXE_H_

#include <Library/PcdLib.h>

#define IPMI_LUN_NUMBER          (FixedPcdGet8 (PcdIpmiLunNumber))
#define IPMI_MAX_LUN             0x3
#define IPMI_MAX_NETFUNCTION     0x3F

#define IPMI_SSIF_SINGLE_PART_WRITE_SMBUS_CMD       0x02
#define IPMI_SSIF_MULTI_PART_WRITE_START_SMBUS_CMD  0x06
#define IPMI_SSIF_MULTI_PART_WRITE_MIDDLE_SMBUS_CMD 0x07
#define IPMI_SSIF_MULTI_PART_WRITE_END_SMBUS_CMD    0x08

#define IPMI_SSIF_SINGLE_PART_READ_SMBUS_CMD        0x03
#define IPMI_SSIF_MULTI_PART_READ_START_SMBUS_CMD   0x03
#define IPMI_SSIF_MULTI_PART_READ_MIDDLE_SMBUS_CMD  0x09
#define IPMI_SSIF_MULTI_PART_READ_END_SMBUS_CMD     0x09
#define IPMI_SSIF_MULTI_PART_READ_RETRY_SMBUS_CMD   0x0A

#define IPMI_SSIF_MULTI_PART_READ_START_SIZE        30
#define IPMI_SSIF_MULTI_PART_READ_START_PATTERN1    0x0
#define IPMI_SSIF_MULTI_PART_READ_START_PATTERN2    0x1
#define IPMI_SSIF_MULTI_PART_READ_END_PATTERN       0xFF

#define IPMI_SSIF_SLAVE_ADDRESS       (FixedPcdGet8 (PcdBmcSlaveAddr))
#define IPMI_SSIF_RETRY_DELAY         (FixedPcdGet32 (PcdIpmiSsifRetryDelay))
#define IPMI_SSIF_BLOCK_LEN           0x20
#define IPMI_SSIF_MAX_REQUEST_RETRY   5     // 12.17 SSIF Timing

#define SSIF_SINGLE_PART_RW           0x0
#define SSIF_START_END_RW             0x1
#define SSIF_MULTI_PART_RW            0x2

#endif
