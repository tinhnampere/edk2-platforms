/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_PCIE_DEVICE_CONFIG_VFR_H_
#define PLATFORM_PCIE_DEVICE_CONFIG_VFR_H_

#include <Guid/PlatformPcieDeviceConfigHii.h>

#define VARSTORE_NAME   L"PlatformPcieDeviceConfigNVData"

#define MAIN_FORM_ID    0x01
#define DEVICE_FORM_ID  0x02
#define VARSTORE_ID     0x03

#define MAIN_LABEL_UPDATE   0x21
#define MAIN_LABEL_END      0x22
#define DEVICE_LABEL_UPDATE 0x31
#define DEVICE_LABEL_END    0x32

#define DEVICE_KEY          0x6000
#define MPS_ONE_OF_KEY      0x7000
#define MRR_ONE_OF_KEY      0x8000

#define MAX_DEVICE          40

#define DEFAULT_MPS         0x00 // Section 7.5.3.4
#define DEFAULT_MRR         0x02 // Section 7.5.3.4

#define PCIE_ADD(Vid, Did, Seg, Bus, Dev) \
        (UINT64)(Vid) << 40 | (UINT64)(Did) << 24 | Seg << 16 | Bus << 8 | Dev;

#pragma pack(1)

typedef struct {
  UINT8  DEV;
  UINT8  BUS;
  UINT8  SEG;
  UINT16 DID;
  UINT16 VID;
  UINT8  SlotId;
} SLOT_INFO;

typedef struct {
  UINT8  MPS[MAX_DEVICE];
  UINT8  MRR[MAX_DEVICE];
  UINT64 SlotInfo[MAX_DEVICE];
} VARSTORE_DATA;

#pragma pack()

#endif /* PLATFORM_PCIE_DEVICE_CONFIG_VFR_H_ */
