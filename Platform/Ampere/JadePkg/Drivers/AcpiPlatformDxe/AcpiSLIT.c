/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "AcpiPlatform.h"

#define SELF_DISTANCE                 10
#define REMOTE_DISTANCE               20

EFI_ACPI_6_3_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_HEADER SLITTableHeaderTemplate = {
  __ACPI_HEADER (
    EFI_ACPI_6_3_SYSTEM_LOCALITY_INFORMATION_TABLE_SIGNATURE,
    0, /* need fill in */
    EFI_ACPI_6_3_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_REVISION
    ),
  0,
};

EFI_STATUS
AcpiInstallSlitTable (
  VOID
  )
{
  EFI_ACPI_TABLE_PROTOCOL                                        *AcpiTableProtocol;
  EFI_STATUS                                                     Status;
  UINTN                                                          NumDomain, Count, Count1;
  EFI_ACPI_6_3_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_HEADER *SlitTablePointer;
  UINT8                                                          *TmpPtr;
  UINTN                                                          SlitTableKey;
  UINTN                                                          NumDomainPerSocket;

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NumDomainPerSocket = CpuGetNumOfSubNuma ();
  NumDomain = NumDomainPerSocket * GetNumberActiveSockets ();

  SlitTablePointer = (EFI_ACPI_6_3_SYSTEM_LOCALITY_DISTANCE_INFORMATION_TABLE_HEADER *)
                     AllocateZeroPool (sizeof (SLITTableHeaderTemplate) + NumDomain * NumDomain);
  if (SlitTablePointer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  CopyMem ((VOID *)SlitTablePointer, (VOID *)&SLITTableHeaderTemplate, sizeof (SLITTableHeaderTemplate));
  SlitTablePointer->NumberOfSystemLocalities = NumDomain;
  TmpPtr = (UINT8 *)SlitTablePointer + sizeof (SLITTableHeaderTemplate);
  for (Count = 0; Count < NumDomain; Count++) {
    for (Count1 = 0; Count1 < NumDomain; Count1++, TmpPtr++) {
      if (Count == Count1) {
        *TmpPtr = (UINT8)SELF_DISTANCE;
        continue;
      }
      if ((Count1 / NumDomainPerSocket) == (Count / NumDomainPerSocket)) {
        *TmpPtr = (UINT8)(SELF_DISTANCE + 1);
      } else {
        *TmpPtr = (UINT8)(REMOTE_DISTANCE);
      }
    }
  }

  SlitTablePointer->Header.Length = sizeof (SLITTableHeaderTemplate) + NumDomain * NumDomain;

  AcpiTableChecksum (
    (UINT8 *)SlitTablePointer,
    SlitTablePointer->Header.Length
    );

  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                (VOID *)SlitTablePointer,
                                SlitTablePointer->Header.Length,
                                &SlitTableKey
                                );
  FreePool ((VOID *)SlitTablePointer);

  return Status;
}
