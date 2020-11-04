/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _MM_LIB_H_
#define _MM_LIB_H_

enum {
    MM_SPINOR_FUNC_GET_INFO,
    MM_SPINOR_FUNC_READ,
    MM_SPINOR_FUNC_WRITE,
    MM_SPINOR_FUNC_ERASE,
    MM_SPINOR_FUNC_GET_NVRAM_INFO,
    MM_SPINOR_FUNC_GET_NVRAM2_INFO,
    MM_SPINOR_FUNC_GET_FAILSAFE_INFO
};

#define MM_SPINOR_RES_SUCCESS           0xAABBCC00
#define MM_SPINOR_RES_FAIL              0xAABBCCFF

#define EFI_MM_MAX_PAYLOAD_U64_E  10
#define EFI_MM_MAX_PAYLOAD_SIZE   (EFI_MM_MAX_PAYLOAD_U64_E * sizeof(UINT64))
#define EFI_MM_MAX_TMP_BUF_SIZE   0x1000000

typedef struct {
  /* Allows for disambiguation of the message format */
  EFI_GUID HeaderGuid;
  /*
   * Describes the size of Data (in bytes) and does not include the size
   * of the header
   */
  UINTN MsgLength;
} EFI_MM_COMM_HEADER_NOPAYLOAD;

typedef struct {
  UINT64 Data[EFI_MM_MAX_PAYLOAD_U64_E];
} EFI_MM_COMM_SPINOR_PAYLOAD;

typedef struct {
  EFI_MM_COMM_HEADER_NOPAYLOAD EfiMmHdr;
  EFI_MM_COMM_SPINOR_PAYLOAD   PayLoad;
} EFI_MM_COMM_REQUEST;

typedef struct {
  UINT64 Status;
  UINT64 DeviceBase;
  UINT64 PageSize;
  UINT64 SectorSize;
  UINT64 DeviceSize;
} EFI_MM_COMMUNICATE_SPINOR_RES;

typedef struct {
  UINT64 Status;
  UINT64 NVBase;
  UINT64 NVSize;
} EFI_MM_COMMUNICATE_SPINOR_NVINFO_RES;

#endif /* _MM_LIB_H_ */
