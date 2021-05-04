/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ACPI_NFIT_H_
#define ACPI_NFIT_H_

#include <Platform/Ac01.h>

#define NVDIMM_SK0          0
#define NVDIMM_SK1          1
#define NVDIMM_NUM_PER_SK   (PLATFORM_NVDIMM_MCU_MAX_PER_SK * PLATFORM_NVDIMM_NUM_MAX_PER_MCU)
#define ONE_GB              (1024 * 1024 * 1024)

enum NvdimmMode {
  NVDIMM_DISABLED   = 0,
  NVDIMM_NON_HASHED = 1,
  NVDIMM_HASHED     = 2
};

typedef struct {
  BOOLEAN Enabled;
  UINT64  NvdSize;
  UINT32  DeviceHandle;
  UINT16  PhysId;
  UINT8   InterleaveWays;
  UINT64  RegionOffset;
  UINT16  VendorId;
  UINT16  DeviceId;
  UINT16  RevisionId;
  UINT16  SubVendorId;
  UINT16  SubDeviceId;
  UINT16  SubRevisionId;
  UINT32  SerialNumber;
} NVDIMM_INFO;

typedef struct {
  UINT8       NvdRegionNum;
  UINT8       NvdRegionId[PLATFORM_NVDIMM_REGION_MAX_PER_SK];
  UINT8       NvdMode;
  UINT8       NvdNum;
  NVDIMM_INFO NvdInfo[NVDIMM_NUM_PER_SK];
} NVDIMM_DATA;

#endif
