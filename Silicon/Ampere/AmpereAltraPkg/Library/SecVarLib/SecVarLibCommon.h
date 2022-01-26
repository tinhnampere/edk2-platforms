/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SEC_VAR_LIB_COMMON_H_
#define SEC_VAR_LIB_COMMON_H_

#define MM_SECVAR_RES_SUCCESS           0xAABBCC00
#define MM_SECVAR_RES_NOT_SET           0xAABBCC01
#define MM_SECVAR_RES_INSUFFICIENT_RES  0xAABBCC02
#define MM_SECVAR_RES_IO_ERROR          0xAABBCC03
#define MM_SECVAR_RES_ACCESS_DENIED     0xAABBCC04
#define MM_SECVAR_RES_AUTH_ERROR        0xAABBCC05
#define MM_SECVAR_RES_FAIL              0xAABBCCFF

#define EFI_MM_MAX_PAYLOAD_U64_E  10
#define EFI_MM_MAX_PAYLOAD_SIZE   (EFI_MM_MAX_PAYLOAD_U64_E * sizeof (UINT64))

// MM PayLoad Request ID from Non Secure caller.
#define MM_SECVAR_KEYGET        1 ///< Secure variable MM Sub function Get
#define MM_SECVAR_KEYSET        2 ///< Secure variable MM Sub function Set
#define MM_SECVAR_AUTHEN        3 ///< Secure variable MM sub function Enable Authenticate


typedef struct {
  //
  // Allows for disambiguation of the message format
  //
  EFI_GUID    HeaderGuid;

  //
  // Describes the size of Data (in bytes) and does not include the size
  // of the header
  //
  UINTN       MsgLength;
} EFI_MM_COMM_HEADER_NOPAYLOAD;

typedef struct {
  //
  // Payload data
  //
  UINT64    Data[EFI_MM_MAX_PAYLOAD_U64_E];
} EFI_MM_COMM_PAYLOAD;

typedef struct {
  //
  // MM Request header
  //
  EFI_MM_COMM_HEADER_NOPAYLOAD    EfiMmHdr;

  //
  // MM Request payload data
  //
  EFI_MM_COMM_PAYLOAD             PayLoad;
} EFI_MM_COMM_REQUEST;

typedef struct {
  UINT64    Status;
  UINT64    Len;
  UINT64    Attr;
} EFI_MM_COMMUNICATE_SECVAR_RES;

#endif /* SEC_VAR_LIB_COMMON_H_ */
