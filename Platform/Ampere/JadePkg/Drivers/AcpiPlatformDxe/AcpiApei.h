/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ACPIAPEI_H_
#define ACPIAPEI_H_

#include <AcpiNVDataStruc.h>
#include <Base.h>
#include <Guid/AcpiConfigFormSet.h>
#include <IndustryStandard/Acpi63.h>
#include <Library/AcpiHelperLib.h>
#include <Library/AmpereCpuLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Platform/Ac01.h>
#include <Protocol/AcpiTable.h>

#pragma pack(1)
#define BERT_MSG_SIZE                0x2C
#define BERT_ERROR_TYPE              0x7F
#define BERT_UEFI_FAILURE            5
#define RAS_2P_TYPE                  0x03
#define BERT_DEFAULT_BLOCK_TYPE      0x10
#define BERT_DEFAULT_ERROR_SEVERITY  0x1
#define GENERIC_ERROR_DATA_REVISION  0x300


#define PLAT_CRASH_ITERATOR_SIZE     0x238
#define SMPRO_CRASH_SIZE             0x800
#define PMPRO_CRASH_SIZE             0x400
#define HEST_NUM_ENTRIES_PER_SOC     3

#define CURRENT_BERT_VERSION         0x10
#define BERT_FLASH_OFFSET            0x91B30000ULL
#define BERT_DDR_OFFSET              0x88230000ULL

typedef struct {
  UINT32 BlockStatus;
  UINT32 RawDataOffset;
  UINT32 RawDataLength;
  UINT32 DataLength;
  UINT32 ErrorSeverity;
} APEI_GENERIC_ERROR_STATUS;

typedef struct {
  UINT8  SectionType[16];
  UINT32 ErrorSeverity;
  UINT16 Revision;
  UINT8  ValidationBits;
  UINT8  Flags;
  UINT32 ErrorDataLength;
  UINT8  FruId[16];
  UINT8  FruText[20];
  UINT64 Timestamp;
} APEI_GENERIC_ERROR_DATA;

typedef struct {
  UINT8  Type;
  UINT8  SubType;
  UINT16 Instance;
  CHAR8  Msg[BERT_MSG_SIZE];
} APEI_BERT_ERROR_DATA;

typedef struct {
  APEI_BERT_ERROR_DATA vendor;
  UINT8                BertRev;
  UINT8                S0PmproRegisters[PMPRO_CRASH_SIZE];
  UINT8                S0SmproRegisters[SMPRO_CRASH_SIZE];
  UINT8                S1PmproRegisters[PMPRO_CRASH_SIZE];
  UINT8                S1SmproRegisters[SMPRO_CRASH_SIZE];
  UINT8                AtfDump[PLATFORM_CPU_MAX_NUM_CORES * PLAT_CRASH_ITERATOR_SIZE];
} APEI_CRASH_DUMP_DATA;

typedef struct {
  APEI_GENERIC_ERROR_STATUS Ges;
  APEI_GENERIC_ERROR_DATA   Ged;
  APEI_CRASH_DUMP_DATA      Bed;
} APEI_CRASH_DUMP_BERT_ERROR;
#pragma pack()

VOID
CreateDefaultBertData (
  APEI_BERT_ERROR_DATA *Data
  );

VOID
WrapBertErrorData (
  APEI_CRASH_DUMP_BERT_ERROR *WrappedError
  );

VOID
PullBertSpinorData (
  APEI_CRASH_DUMP_DATA *BertErrorData
  );

VOID
AdjustBERTRegionLen (
  UINT32 Len
  );

UINT32
IsBertEnabled (
  VOID
  );

VOID
WriteDDRBertTable (
  APEI_CRASH_DUMP_BERT_ERROR *Data
  );

VOID
WriteSpinorDefaultBertTable (
  APEI_CRASH_DUMP_DATA *SpiRefrenceData
  );

EFI_STATUS
EFIAPI
AcpiApeiUpdate (
  VOID
  );

EFI_STATUS
AcpiPopulateBert (
  VOID
  );

#endif /* ACPIAPEI_H_ */
