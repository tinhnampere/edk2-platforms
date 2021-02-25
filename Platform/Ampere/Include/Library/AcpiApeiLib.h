/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ACPI_APEI_LIB_H_
#define ACPI_APEI_LIB_H_

#include <Base.h>
#include <Guid/Cper.h>
#include <IndustryStandard/Acpi63.h>

/*
 * BERT entry list
 *
 * All errors in BERT fall under one Error Source
 */

#define ACPI_APEI_BERT_MAX 1

/*
 * GHES entry list
 *
 * NOTE, adding more GHES entries requires you to add more GHES
 * entries in the HEST.adt file.
 */

typedef enum {
    ACPI_APEI_GHES_CPU = 0,
    ACPI_APEI_GHES_L2C,
    ACPI_APEI_GHES_L3C,
    ACPI_APEI_GHES_MCU,
    ACPI_APEI_GHES_IOB_RBM,
    ACPI_APEI_GHES_IOB_GLBL,
    ACPI_APEI_GHES_IOB_TRANS,
    ACPI_APEI_GHES_XGIC,
    ACPI_APEI_GHES_SMMU,
    ACPI_APEI_GHES_SOC,
    ACPI_APEI_GHES_SOC_MCU,
    ACPI_APEI_GHES_MPA,
    ACPI_APEI_GHES_MAX
} ACPI_APEI_GHES_ENTRY;

/*
 * For each GHES entry, there is Error Status and Error Data that needs
 * to be gathered and reported. The Error Data is stored in the following
 * APEI Data Structures.
 */

#pragma pack(1)
typedef struct {
  EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE GED;
  EFI_PROCESSOR_GENERIC_ERROR_DATA                PError;
} ACPI_APEI_ERROR_DATA;

typedef struct {
  EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE GED;
  EFI_PLATFORM_MEMORY_ERROR_DATA                  MError;
} ACPI_APEI_MEM_ERROR_DATA;

/*
 * Error Status with one or more Error Data sections per error
 * record make up an Error Status Block (ESB). The ESB will be
 * linked to a GHES entry via the Error Status Address.
 */

#define ACPI_APEI_ESB_MAX_BERT_ERRORS    8
#define ACPI_APEI_ESB_MAX_GHES_ERRORS    1
#define ACPI_APEI_ESB_MAX_ERROR_INFO     23
#define ACPI_APEI_ESB_FIRMWARE_MEMORY    984

typedef struct {
  EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE    GES;
  ACPI_APEI_ERROR_DATA                           data;
  ACPI_APEI_ERROR_DATA                           info[ACPI_APEI_ESB_MAX_ERROR_INFO];
} ACPI_APEI_ESB;


/*
 * Version 2 of RAS_APEI_ES between BIOS and Firmware.
 *
 * This is the memory view of Error Source Data for BERT and GHES entries
 */

typedef struct {
  UINT32                     ErrorDataEntryCount;
  UINT32                     Length;
  ACPI_APEI_ESB              ESB[ACPI_APEI_ESB_MAX_BERT_ERRORS];
} RAS_APEI_BERT_DATA;

typedef struct {
  UINT64                     ErrorStatusAddress;
  UINT32                     ErrorDataEntryCount;
  UINT32                     Length;
  ACPI_APEI_ESB              ESB[ACPI_APEI_ESB_MAX_GHES_ERRORS];
  UINT32                     Reserved; /* Reserved - keep data 64bit aligned */
  UINT8                      FirmwareMemory[ACPI_APEI_ESB_FIRMWARE_MEMORY];
} RAS_APEI_GHES_DATA;

typedef struct {
  UINT64 Resv1;              /* v1 ErrorStatusAddress set to 0 */
  UINT64 Resv2;              /* v1 GES set to 0 */
  UINT32                     TotalLength;
  UINT32                     ErrorSourceCount;
  RAS_APEI_BERT_DATA         ErrorSourceData[ACPI_APEI_BERT_MAX];
} RAS_APEI_BERT_ES;

typedef struct {
  UINT64 Resv1;              /* v1 ErrorStatusAddress set to 0 */
  UINT64 Resv2;              /* v1 GES set to 0 */
  UINT32                     TotalLength;
  UINT32                     ErrorSourceCount;
  RAS_APEI_GHES_DATA         ErrorSourceData[ACPI_APEI_GHES_MAX];
} RAS_APEI_GHES_ES;

/*
 * Version 1 of RAS_APEI_ES between BIOS and Firmware.
 *
 * This interface structure is no longer used. It is here
 * only as reference for Version 2.
 */

typedef struct {
  UINT64 ErrorStatusAddress; /* pointer to GHES.ErrorStatusAddress */
  UINT64 Ges;                /* pointer to GES (use to report ErrorBlockStatus) */
  UINT32 ErrorBlockStatus;   /* ErrorBlockStatus to report */
  UINT32 Resv;               /* Reserved */
} RAS_APEI_ES_VERSION_1;
#pragma pack()

/*
 * AcpiApeiLib interface
 */

EFI_STATUS
AcpiApeiLibInit (VOID);

RAS_APEI_BERT_DATA*
AcpiApeiLibGetBertData (VOID);

RAS_APEI_GHES_DATA*
AcpiApeiLibGetGhesData (
  IN UINT32 ErrorSourceIdx
  );

#define APEI_ERROR_DETECTION_PMPRO        0
#define APEI_ERROR_DETECTION_ATF          1

EFI_STATUS
AcpiApeiLibEnable (
  UINT8 Enable,
  UINT8 FwErrorDetection
  );

#endif /* ACPI_APEI_LIB_H_ */
