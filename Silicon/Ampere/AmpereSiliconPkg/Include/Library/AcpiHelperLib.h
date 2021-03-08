/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ACPIHELPERLIB_H_
#define ACPIHELPERLIB_H_

#include <Uefi.h>

#include <Protocol/AcpiSystemDescriptionTable.h>

#define MAX_ACPI_NODE_PATH    256


typedef struct {
  EFI_ACPI_SDT_HEADER        *Table;
  EFI_ACPI_TABLE_VERSION      TableVersion;
  UINTN                       TableKey;
} ACPI_TABLE_DESCRIPTOR;

/**
  This function calculates and updates an UINT8 checksum.

  @param[in]  Buffer          Pointer to buffer to checksum
  @param[in]  Size            Number of bytes to checksum

**/
VOID
EFIAPI
AcpiTableChecksum (
  IN UINT8      *Buffer,
  IN UINTN      Size
  );

/**
  This function calculates and updates the ACPI DSDT checksum.

  @param[in]  AcpiTableProtocol          Pointer to ACPI table protocol

**/
VOID
EFIAPI
AcpiDSDTUpdateChecksum (
  IN EFI_ACPI_SDT_PROTOCOL  *AcpiTableProtocol
  );

/**
  This function update the _STA value of a ACPI DSDT node.

  @param[in]  AsciiNodePath          Pointer to the path of the node.
  @param[in]  NodeStatus             The status value needed to be updated.

**/
EFI_STATUS
EFIAPI
AcpiDSDTSetNodeStatusValue (
  IN CHAR8     *AsciiNodePath,
  IN CHAR8     NodeStatus
  );

/**
  This function return the handle of the ACPI DSDT table.

  @param[in]   AcpiTableProtocol          Pointer to ACPI table protocol.
  @param[out]  TableHandle                Pointer to table handle.

**/
EFI_STATUS
EFIAPI
AcpiOpenDSDT (
  IN EFI_ACPI_SDT_PROTOCOL  *AcpiTableProtocol,
  OUT EFI_ACPI_HANDLE       *TableHandle
  );

/**
  This function return the ACPI table matching a signature.

  @param[in]   TableDescriptor        Pointer to ACPI table descriptor.
  @param[in]   TableSignature         ACPI table signature.

**/
EFI_STATUS
EFIAPI
AcpiGetTable (
  IN  EFI_ACPI_SDT_PROTOCOL   *AcpiTableSdtProtocol,
  IN  UINT32                  TableSignature,
  OUT ACPI_TABLE_DESCRIPTOR   *TableDescriptor
  );

/**
  Check whether the ACPI table is installed or not.

  @param[in]    AcpiTableSignature        ACPI table signature.

  @retval       TRUE     Already installed.
  @retval       FALSE    Not installed.

**/
BOOLEAN
EFIAPI
IsAcpiInstalled (
  IN  UINT32                  AcpiTableSignature
  );

#endif /* ACPIHELPERLIB_H_ */
