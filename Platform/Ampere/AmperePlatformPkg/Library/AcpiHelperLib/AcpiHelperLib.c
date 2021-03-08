/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/Acpi63.h>
#include <IndustryStandard/AcpiAml.h>
#include <Library/AcpiHelperLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/AcpiTable.h>

#define DSDT_SIGNATURE                  0x54445344
#define FADT_SIGNATURE                  0x50434146

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
  )
{
  UINTN ChecksumOffset;

  ASSERT (Buffer != NULL);

  ChecksumOffset = OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER, Checksum);

  /*
   * Set checksum to 0 first.
   */
  Buffer[ChecksumOffset] = 0;

  /*
   * Update checksum value.
   */
  Buffer[ChecksumOffset] = 0 - CalculateSum8 (Buffer, Size);
}

/**
  This function calculates and updates the ACPI DSDT checksum.

  @param[in]  AcpiTableProtocol          Pointer to ACPI table protocol

**/
VOID
EFIAPI
AcpiDSDTUpdateChecksum (
  IN EFI_ACPI_SDT_PROTOCOL  *AcpiTableProtocol
  )
{
  EFI_STATUS                                Status = EFI_SUCCESS;
  EFI_ACPI_SDT_HEADER                       *DsdtHdr = NULL;
  ACPI_TABLE_DESCRIPTOR                     TableDescriptor;
  EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *FadtPtr = NULL;

  ASSERT (AcpiTableProtocol != NULL);

  Status = AcpiGetTable (AcpiTableProtocol, FADT_SIGNATURE, &TableDescriptor);
  if (EFI_ERROR (Status) || TableDescriptor.Table == NULL) {
    return;
  }

  FadtPtr = (EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *) TableDescriptor.Table;

  if (FadtPtr->Dsdt) {
    DsdtHdr = (EFI_ACPI_SDT_HEADER *)(UINT64) FadtPtr->Dsdt;
  } else if (FadtPtr->XDsdt) {
    DsdtHdr = (EFI_ACPI_SDT_HEADER *) FadtPtr->XDsdt;
  }

  if (DsdtHdr != NULL) {
    AcpiTableChecksum ((UINT8 *) DsdtHdr, DsdtHdr->Length);
  }
}

/**
  This function return the handle of the ACPI DSDT table.

  @param[in]   AcpiTableProtocol          Pointer to ACPI table protocol.
  @param[out]  TableHandle                Pointer to table handle.

**/
EFI_STATUS
EFIAPI
AcpiOpenDSDT (
  IN  EFI_ACPI_SDT_PROTOCOL  *AcpiTableProtocol,
  OUT EFI_ACPI_HANDLE       *TableHandle
  )
{
  EFI_STATUS                 Status = EFI_SUCCESS;
  ACPI_TABLE_DESCRIPTOR      TableDescriptor;

  Status = AcpiGetTable (AcpiTableProtocol, DSDT_SIGNATURE, &TableDescriptor);
  if (!EFI_ERROR (Status) && (TableDescriptor.Table != NULL)) {
    return AcpiTableProtocol->OpenSdt (TableDescriptor.TableKey, TableHandle);
  }

  return Status;
}

EFI_STATUS
EFIAPI
AcpiDSDTSetNodeStatusValue (
  IN CHAR8     *AsciiNodePath,
  IN CHAR8     NodeStatus
  )
{
  EFI_STATUS                        Status = EFI_SUCCESS;
  EFI_ACPI_SDT_PROTOCOL             *AcpiTableProtocol;
  EFI_ACPI_HANDLE                   TableHandle;
  EFI_ACPI_HANDLE                   ChildHandle;
  EFI_ACPI_DATA_TYPE                DataType;
  CHAR8                             *Buffer;
  UINTN                             DataSize;

  Status = gBS->LocateProtocol (&gEfiAcpiSdtProtocolGuid, NULL, (VOID**) &AcpiTableProtocol);
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to locate ACPI table protocol\n"));
    return Status;
  }

  /* Open DSDT Table */
  Status = AcpiOpenDSDT (AcpiTableProtocol, &TableHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AcpiTableProtocol->FindPath (TableHandle, AsciiNodePath, &ChildHandle);
  if (EFI_ERROR (Status)) {
    /* Close DSDT Table */
    AcpiTableProtocol->Close (TableHandle);
    return EFI_SUCCESS;
  }

  Status = AcpiTableProtocol->GetOption (ChildHandle, 2, &DataType, (VOID *) &Buffer, &DataSize);
  if (Status == EFI_SUCCESS && Buffer[2] == AML_BYTE_PREFIX) {
    /*
     * Only patch when the initial value is byte object.
     */
    Buffer[3] = NodeStatus;
  }

  /* Close DSDT Table */
  AcpiTableProtocol->Close (TableHandle);

  /* Update DSDT Checksum */
  AcpiDSDTUpdateChecksum (AcpiTableProtocol);

  return EFI_SUCCESS;
}

/**
  This function return the ACPI table matching a signature.

  @param[in]   AcpiTableSdtProtocol   Pointer to ACPI SDT protocol.
  @param[in]   TableSignature         ACPI table signature.
  @param[out]  TableDescriptor        Pointer to ACPI table descriptor.

**/
EFI_STATUS
EFIAPI
AcpiGetTable (
  IN  EFI_ACPI_SDT_PROTOCOL   *AcpiTableSdtProtocol,
  IN  UINT32                  TableSignature,
  OUT ACPI_TABLE_DESCRIPTOR   *TableDescriptor
  )
{
  EFI_STATUS                  Status = EFI_SUCCESS;
  UINTN                       TableIndex = 0;

  ASSERT (AcpiTableSdtProtocol != NULL);
  ASSERT (TableDescriptor != NULL);

  /*
   * Search for ACPI Table Signature
   */
  while (!EFI_ERROR (Status)) {
    Status = AcpiTableSdtProtocol->GetAcpiTable (
                                 TableIndex,
                                 &(TableDescriptor->Table),
                                 &(TableDescriptor->TableVersion),
                                 &(TableDescriptor->TableKey)
                                 );
    if (!EFI_ERROR (Status)) {
      TableIndex++;

      if (((EFI_ACPI_SDT_HEADER *)TableDescriptor->Table)->Signature == TableSignature) {
        return EFI_SUCCESS;
      }
    }
  }

  /* Nothing was found.  Clear the table descriptor. */
  ZeroMem(&TableDescriptor, sizeof(TableDescriptor));

  return EFI_NOT_FOUND;
}

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
  )
{
  EFI_STATUS                Status;
  ACPI_TABLE_DESCRIPTOR     TableDescriptor;
  EFI_ACPI_SDT_PROTOCOL     *AcpiTableSdtProtocol = NULL;

  Status = gBS->LocateProtocol (&gEfiAcpiSdtProtocolGuid, NULL, (VOID**) &AcpiTableSdtProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AcpiGetTable (AcpiTableSdtProtocol, AcpiTableSignature, &TableDescriptor);
  if (!EFI_ERROR (Status) && (TableDescriptor.Table != NULL)) {
    return TRUE;
  }

  return FALSE;
}
