/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ACPIAPEI_H__
#define __ACPIAPEI_H__

#include <Base.h>
#include <IndustryStandard/Acpi63.h>
#include <Library/AcpiHelperLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/AcpiTable.h>
#include <Guid/AcpiConfigFormSet.h>
#include <AcpiNVDataStruc.h>

#define BERT_MESSAGE_SIZE            0x40
#define BERT_ERROR_TYPE              0x7F
#define BERT_DEFAULT_BLOCK_TYPE      0x10
#define BERT_DEFAULT_ERROR_SEVERITY  0x1
#define GENERIC_ERROR_DATA_REVISION  0x300

struct ApeiGenericErrorStatus {
  UINT32  BlockStatus;
  UINT32  RawDataOffset;
  UINT32  RawDataLength;
  UINT32  DataLength;
  UINT32  ErrorSeverity;
} __attribute__ ((packed));

struct ApeiGenericErrorData {
  UINT8   SectionType[16];
  UINT32  ErrorSeverity;
  UINT16  Revision;
  UINT8   ValidationBits;
  UINT8   Flags;
  UINT32  ErrorDataLength;
  UINT8   FruId[16];
  UINT8   FruText[20];
  UINT64  Timestamp;
} __attribute__ ((packed));

struct ApeiBertErrorData {
  UINT8   Type;
  UINT8   SubType;
  UINT16  Instance;
  CHAR8   Msg[BERT_MESSAGE_SIZE];
} __attribute__ ((packed));

struct ApeiVendorBertError {
  struct ApeiGenericErrorStatus    Ges;
  struct ApeiGenericErrorData      Ged;
  struct ApeiBertErrorData         Bed;
} __attribute__ ((packed));

VOID CreateDefaultBertData(struct ApeiBertErrorData *Data);
VOID WrapBertErrorData(struct ApeiBertErrorData *BertErrorData,struct ApeiVendorBertError *WrappedError);
VOID PullBertSpinorData(struct ApeiBertErrorData *BertErrorData);

UINT32 IsBertEnabled(void);
VOID WriteDDRBertTable(struct ApeiVendorBertError *Data);
VOID WriteSpinorDefaultBertTable(struct ApeiBertErrorData *SpiRefrenceData);

EFI_STATUS
AcpiApeiUpdate (VOID);

EFI_STATUS
AcpiPopulateBert(VOID);

#endif /* __ACPIAPEI_H__ */

