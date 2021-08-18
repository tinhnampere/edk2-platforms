/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ACPIHELPERLIB_H_
#define ACPIHELPERLIB_H_

#include <Uefi.h>

#include <Protocol/AcpiSystemDescriptionTable.h>

#define MAX_ACPI_NODE_PATH    256


typedef struct {
  EFI_ACPI_SDT_HEADER    *Table;
  EFI_ACPI_TABLE_VERSION TableVersion;
  UINTN                  TableKey;
} ACPI_TABLE_DESCRIPTOR;

#pragma pack(1)

typedef struct {
  UINT8   DWordPrefix;
  UINT32  DWordData;
} OP_REGION_DWORD_DATA;

typedef struct {
  UINT8                 ExtOpPrefix;
  UINT8                 ExtOpCode;
  UINT8                 NameString[4];
  UINT8                 RegionSpace;
  OP_REGION_DWORD_DATA  RegionBase;
  OP_REGION_DWORD_DATA  RegionLen;
} AML_OP_REGION;

#pragma pack()

/**
  This function calculates and updates an UINT8 checksum.

  @param[in]  Buffer          Pointer to buffer to checksum
  @param[in]  Size            Number of bytes to checksum

**/
VOID
EFIAPI
AcpiTableChecksum (
  IN UINT8 *Buffer,
  IN UINTN Size
  );

/**
  This function calculates and updates the ACPI DSDT checksum.

  @param[in]  AcpiTableProtocol          Pointer to ACPI table protocol

**/
VOID
EFIAPI
AcpiDSDTUpdateChecksum (
  IN EFI_ACPI_SDT_PROTOCOL *AcpiTableProtocol
  );

/**
  This function update the _STA value of a ACPI DSDT node.

  @param[in]  AsciiNodePath          Pointer to the path of the node.
  @param[in]  NodeStatus             The status value needed to be updated.

**/
EFI_STATUS
EFIAPI
AcpiDSDTSetNodeStatusValue (
  IN CHAR8 *AsciiNodePath,
  IN CHAR8 NodeStatus
  );

/**
  This function return the handle of the ACPI DSDT table.

  @param[in]   AcpiTableProtocol          Pointer to ACPI table protocol.
  @param[out]  TableHandle                Pointer to table handle.

**/
EFI_STATUS
EFIAPI
AcpiOpenDSDT (
  IN  EFI_ACPI_SDT_PROTOCOL *AcpiTableProtocol,
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
  IN  EFI_ACPI_SDT_PROTOCOL *AcpiTableSdtProtocol,
  IN  UINT32                TableSignature,
  OUT ACPI_TABLE_DESCRIPTOR *TableDescriptor
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
  IN UINT32 AcpiTableSignature
  );

/**
  This function outputs base address of a given OpRegion specified by its node path.

  @param[in]   AsciiNodePath    Node path of the OpRegion whose base address will be retrieved.
  @param[out]  Value            Address to put in the OpRegion's base address.

  @retval EFI_SUCCESS           The base address of OpeRegion was got successfully.
  @retval EFI_INVALID_PARAMETER The AsciiNodePath or Value parameter are NULL.
  @retval EFI_NOT_FOUND         The Node path was not found.
  @retval Others                Other failure occurs.

**/
EFI_STATUS
EFIAPI
AcpiDSDTGetOpRegionBase (
  IN   CHAR8     *AsciiNodePath,
  OUT  UINT32    *Value
  );

/**
  This function sets a given OpRegion's base address.

  @param[in]   AsciiNodePath    Node path of the OpRegion whose base address will be set.
  @param[in]   Value            Base address to set for the OpRegion.

  @retval EFI_SUCCESS           The base address of OpeRegion was set successfully.
  @retval EFI_INVALID_PARAMETER The AsciiNodePath parameter is NULL.
  @retval EFI_NOT_FOUND         The Node path was not found.
  @retval Others                Other failure occurs.

**/
EFI_STATUS
EFIAPI
AcpiDSDTSetOpRegionBase (
  IN  CHAR8     *AsciiNodePath,
  IN  UINT32    Value
  );

#endif /* ACPIHELPERLIB_H_ */
