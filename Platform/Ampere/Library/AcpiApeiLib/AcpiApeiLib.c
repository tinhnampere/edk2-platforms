/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/AcpiApeiLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/ArmLib.h>

#undef ACPI_APEI_LIB_DBG
#undef ACPI_APEI_LIB_DBG_IO

#ifdef ACPI_APEI_LIB_DBG
#define DBG(arg...) DEBUG((DEBUG_VERBOSE, ## arg))
#else
#define DBG(arg...)
#endif

#ifdef ACPI_APEI_LIB_DBG_IO
#define DBG_IO(arg...) DEBUG((DEBUG_VERBOSE, ## arg))
#else
#define DBG_IO(arg...)
#endif

#define ERROR(arg...) DEBUG((DEBUG_ERROR, ## arg))

#define SMC_XRAS_PROXY_FUNC_ID  0xc300ff07

enum SMC_XRAS_FUNC_ID {
  SMC_XRAS_SET_APEI_PTR = 1,
  SMC_XRAS_GET_APEI_PTR,
  SMC_XRAS_ENABLE,
  SMC_XRAS_DISABLE
};

/*
 * Reserved Memory
 */
STATIC RAS_APEI_GHES_ES *FwRasApeiGhesLookUpTable = NULL;
STATIC RAS_APEI_BERT_ES *FwRasApeiBertLookUpTable = NULL;

/*
 * AcpiApeiLibAllocateReservedMemForErrorSourceTable
 */
STATIC EFI_STATUS
AcpiApeiLibAllocateReservedMemForErrorSourceTable(VOID)
{
  UINT32 Length;

  /*
   * Allocate reserved memory for each Error Source and initialize it.
   */
  Length = sizeof(RAS_APEI_GHES_ES);
  FwRasApeiGhesLookUpTable = (RAS_APEI_GHES_ES *)AllocateReservedZeroPool(Length);
  if (!FwRasApeiGhesLookUpTable)
    return EFI_OUT_OF_RESOURCES;

  FwRasApeiGhesLookUpTable->TotalLength = Length;
  FwRasApeiGhesLookUpTable->ErrorSourceCount = ACPI_APEI_GHES_MAX;

  /*
   * Allocate a BootErrorSource.
   *
   * BERT does not distinguish errors based on an Error Source Like the HEST
   * table does by using GHES entries. All errors in BERT fall under one Error
   * Source (let's call it the BERT Error Source).
   */
  Length = sizeof(RAS_APEI_BERT_ES);
  FwRasApeiBertLookUpTable = (RAS_APEI_BERT_ES *)AllocateReservedZeroPool(Length);
  if (!FwRasApeiBertLookUpTable)
    return EFI_OUT_OF_RESOURCES;

  FwRasApeiBertLookUpTable->TotalLength = Length;
  FwRasApeiBertLookUpTable->ErrorSourceCount = ACPI_APEI_BERT_MAX;

  return EFI_SUCCESS;
}

/*
 * AcpiApeiLibFreeReservedMem
 */
STATIC VOID
AcpiApeiLibFreeReservedMem(VOID)
{
  if (FwRasApeiGhesLookUpTable) {
    FreePool(FwRasApeiGhesLookUpTable);
    FwRasApeiGhesLookUpTable= NULL;
  }

  if (FwRasApeiBertLookUpTable) {
    FreePool(FwRasApeiBertLookUpTable);
    FwRasApeiBertLookUpTable= NULL;
  }
}

/*
 * AcpiApeiLibAllocateReservedMem
 */
STATIC EFI_STATUS
AcpiApeiLibAllocateReservedMem(VOID)
{
  EFI_STATUS Status;

  /*
   * Reserve Memory For ACPI/APEI ErrorSourceTable
   */
  Status = AcpiApeiLibAllocateReservedMemForErrorSourceTable();
  if (EFI_ERROR(Status)) {
    DBG("Allocating APEI Reserved Memory for ErrorSourceTable failed\n");
    goto Error;
  }

  return Status;

Error:
  AcpiApeiLibFreeReservedMem();
  return Status;
}

/*
 * AcpiApeiLibGetGhesData
 */
RAS_APEI_GHES_DATA*
AcpiApeiLibGetGhesData(IN UINT32 ErrorSourceIdx)
{
  if ( !FwRasApeiGhesLookUpTable || ErrorSourceIdx >= ACPI_APEI_GHES_MAX)
    return NULL;

  return &FwRasApeiGhesLookUpTable->ErrorSourceData[ErrorSourceIdx];
}

/*
 * AcpiApeiLibGetBertData
 */
RAS_APEI_BERT_DATA*
AcpiApeiLibGetBertData(VOID)
{
  if (!FwRasApeiBertLookUpTable)
    return NULL;

  return &FwRasApeiBertLookUpTable->ErrorSourceData[0];
}

/*
 * AcpiApeiLibInit
 */
EFI_STATUS
AcpiApeiLibInit(VOID)
{
  EFI_STATUS Status;

  DBG("AcpiApeiLibInit: Enter\n");

  /*
   * Allocate Reserve Memory
   */
  Status = AcpiApeiLibAllocateReservedMem();
  if (EFI_ERROR(Status)) {
    return Status;
  }

  DBG("AcpiApeiLibInit: Exit\n");
  return EFI_SUCCESS;
}

STATIC VOID AcpiApeiLibAtfRasEnable(UINT8 Enable)
{
  ARM_SMC_ARGS SmcArgs;

  SmcArgs.Arg0 = SMC_XRAS_PROXY_FUNC_ID;
  SmcArgs.Arg1 = Enable? SMC_XRAS_ENABLE: SMC_XRAS_DISABLE;
  SmcArgs.Arg2 = 0;
  SmcArgs.Arg3 = 0;
  SmcArgs.Arg4 = 0;

  ArmCallSmc(&SmcArgs);
}

STATIC VOID AcpiApeiLibAtfApeiSetup(UINT64 ApeiGhesPtr, UINT64 ApeiBertPtr)
{
  ARM_SMC_ARGS SmcArgs;

  SmcArgs.Arg0 = SMC_XRAS_PROXY_FUNC_ID;
  SmcArgs.Arg1 = SMC_XRAS_SET_APEI_PTR;
  SmcArgs.Arg2 = ApeiGhesPtr;
  SmcArgs.Arg3 = ApeiBertPtr;
  SmcArgs.Arg4 = 0;

  ArmCallSmc(&SmcArgs);
}

/*
 * AcpiApeiLibEnable
 */
EFI_STATUS
AcpiApeiLibEnable(UINT8 Enable, UINT8 FwErrorDetection)
{
  UINT64     ApeiGhesPtr, ApeiBertPtr;

  DBG("In %a:\n", __FUNCTION__);

  ArmInvalidateDataCache();

  if (Enable) {
    /*
     * Setup firmware (e.g. ATF) for RAS_APEI module support
     */
    ApeiGhesPtr = (UINT64)FwRasApeiGhesLookUpTable;
    ApeiBertPtr = (UINT64)FwRasApeiBertLookUpTable;

    if (FwErrorDetection == APEI_ERROR_DETECTION_ATF) {
      AcpiApeiLibAtfApeiSetup(ApeiGhesPtr, ApeiBertPtr);
    }
  }

  if (FwErrorDetection == APEI_ERROR_DETECTION_ATF) {
    AcpiApeiLibAtfRasEnable(Enable);
  }

  return EFI_SUCCESS;
}
