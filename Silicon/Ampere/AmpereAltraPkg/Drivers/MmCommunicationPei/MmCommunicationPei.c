/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Uefi/UefiSpec.h>

#include <Library/HobLib.h>
#include <Library/PeimEntryPoint.h>

/**
  Entry point function for the PEIM

  @param FileHandle      Handle of the file being invoked.
  @param PeiServices     Describes the list of possible PEI Services.

  @return EFI_SUCCESS    If we installed our PPI

**/
EFI_STATUS
EFIAPI
MmCommunicationPeiEntryPoint (
  IN       EFI_PEI_FILE_HANDLE FileHandle,
  IN CONST EFI_PEI_SERVICES    **PeiServices
  )
{
  EFI_PHYSICAL_ADDRESS MmBufferBase = PcdGet64 (PcdMmBufferBase);
  UINT64               MmBufferSize = PcdGet64 (PcdMmBufferSize);

  BuildMemoryAllocationHob (MmBufferBase, MmBufferSize, EfiRuntimeServicesData);

  return EFI_SUCCESS;
}
