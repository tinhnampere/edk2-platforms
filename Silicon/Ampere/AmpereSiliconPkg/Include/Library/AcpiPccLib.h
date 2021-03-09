/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ACPI_PCC_LIB_H_
#define ACPI_PCC_LIB_H_

struct ACPI_PCCT_SHARED_MEMORY {
  UINT32 Signature;
  union {
    UINT16 Command;
    struct PCC_COMMAND {
      UINT16 CommandCode : 8;
      UINT16 Reserved : 7;
      UINT16 Interrupt : 1;
    } __attribute__ ((packed)) CmdT;
  } CmdData;
  union {
    UINT16 Status;
    struct PCC_STATUS {
      UINT16 CommandComplete : 1;
      UINT16 SciDb : 1;
      UINT16 Error : 1;
      UINT16 PlatformNotification : 1;
      UINT16 Reserved : 12;
    } __attribute__ ((packed)) StatusT;
  } StatusData;
} __attribute__((packed));

EFI_STATUS
EFIAPI
AcpiPccSendMsg (
  IN UINT32 Socket,
  IN UINT32 Subspace,
  IN VOID   *MsgBuf,
  IN UINT32 Length
  );

EFI_STATUS
EFIAPI
AcpiPccUnmaskInt (
  IN UINT32 Socket,
  IN UINT32 Subspace
  );

EFI_STATUS
EFIAPI
AcpiPccSyncSharedMemAddr (
  IN UINT32 Socket,
  IN UINT32 Subspace
  );

EFI_STATUS
EFIAPI
AcpiPccSharedMemInit (
  IN UINT32 Socket,
  IN UINT32 Subspace
  );

EFI_STATUS
EFIAPI
AcpiPccSharedMemInitV2 (
  IN UINT32 Socket,
  IN UINT32 Subspace
  );

EFI_STATUS
EFIAPI
AcpiIppPccIsSupported (
  VOID
  );

EFI_STATUS
EFIAPI
AcpiPccAllocSharedMemory (
  OUT UINT64 *PccSharedMemPointer,
  IN  UINT32 SubspaceNum
  );

VOID
EFIAPI
AcpiPccFreeSharedMemory (
  OUT UINT64 *PccSharedMemPointer,
  IN  UINT32 SubspaceNum
  );

#endif /* ACPI_PCC_LIB_H_*/
