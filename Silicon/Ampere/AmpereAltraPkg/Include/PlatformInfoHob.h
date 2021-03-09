/** @file

  Copyright (c) 2020-2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_INFO_HOB_H_
#define PLATFORM_INFO_HOB_H_

#include <IndustryStandard/Tpm20.h>
#include <Platform/Ac01.h>

/* DIMM type */
enum {
  UDIMM,
  RDIMM,
  SODIMM,
  RSODIMM,
  LRDIMM,
  NVRDIMM
};

/* DIMM status */
enum {
  DIMM_NOT_INSTALLED = 0,
  DIMM_INSTALLED_OPERATIONAL,    /* installed and operational     */
  DIMM_INSTALLED_NONOPERATIONAL, /* installed and non-operational */
  DIMM_INSTALLED_FAILED          /* installed and failed          */
};

typedef struct {
  UINT32 NumRegion;
  UINT64 TotalSize;
  UINT64 Base[PLATFORM_DRAM_INFO_MAX_REGION];
  UINT64 Size[PLATFORM_DRAM_INFO_MAX_REGION];
  UINT32 CurrentSpeed;
} PlatformDramInfo;

typedef struct {
  UINT32 NumRegion;
  UINT64 TotalSize;
  UINT64 Base[PLATFORM_DRAM_INFO_MAX_REGION];
  UINT64 Size[PLATFORM_DRAM_INFO_MAX_REGION];
  UINT64 Node[PLATFORM_DRAM_INFO_MAX_REGION];
  UINT64 Socket[PLATFORM_DRAM_INFO_MAX_REGION];
  UINT32 MaxSpeed;
  UINT32 McuMask[PLATFORM_CPU_MAX_SOCKET];
  UINT32 NvdRegion[PLATFORM_DRAM_INFO_MAX_REGION];
  UINT32 NvdimmMode[PLATFORM_CPU_MAX_SOCKET];
} PlatformDramInfoV2;

typedef struct {
  CHAR8  PartNumber[32];
  UINT64 DimmSize;
  UINT8  DimmNrRank;
  UINT8  DimmType;
  UINT8  DimmStatus;
  UINT16 DimmMfcId;
  UINT8  DimmDevType;
} PlatformDimmInfo;

typedef struct {
  CHAR8  PartNumber[32];
  UINT64 DimmSize;
  UINT16 DimmMfcId;
  UINT16 Reserved;
  UINT8  DimmNrRank;
  UINT8  DimmType;
  UINT8  DimmStatus;
  UINT8  DimmDevType;
} PlatformDimmInfoV2;

/* Raw memory SPD Data structure */
typedef struct {
  /* Memory Type */
  UINT8 Byte2;
  /* Attribute, Total Width, Data Width (DDR2&3) */
  UINT8 Byte5To8[9 - 5];
  /* ECC Data Width, Data Width (DDR4) */
  UINT8 Byte11To14[15 - 11];
  /* Manufacturer (DDR2) */
  UINT8 Byte64To71[72 - 64];
  /* Part Number (DDR2) */
  UINT8 Byte73To90[91 - 73];
  /* Serial Number (DDR2) */
  UINT8 Byte95To98[99 - 95];
  /* Manufacturer (DDR3) */
  UINT8 Byte117To118[119 - 117];
  /* Serial Number (DDR3) */
  UINT8 Byte122To125[126 - 122];
  /* Part Number (DDR3) */
  UINT8 Byte128To145[146 - 128];
  /* Manufacturer (DDR4) */
  UINT8 Byte320To321[322 - 320];
  /* Serial Number (DDR4) */
  UINT8 Byte325To328[329 - 325];
  /* Part Number (DDR4) */
  UINT8 Byte329To348[349 - 329];
} PlatformDimmSpdData;

typedef struct {
  UINT8 Data[512];
} PlatformDimmSpdDataV2;

typedef struct {
  PlatformDimmInfo    Info;
  PlatformDimmSpdData SpdData;
} PlatformDimm;

typedef struct {
  PlatformDimmInfoV2    Info;
  PlatformDimmSpdDataV2 SpdData;
  UINT32                NodeId;
} PlatformDimmV2;

typedef struct {
  UINT32       NumSlot;
  PlatformDimm Dimm[PLATFORM_DIMM_INFO_MAX_SLOT];
} PlatformDimmList;

typedef struct {
  UINT32         BoardDimmSlots;
  PlatformDimmV2 Dimm[PLATFORM_DIMM_INFO_MAX_SLOT];
} PlatformDimmListV2;

typedef struct {
  UINT32 EnableMask[4];
} PlatformClusterEn;

//
// Algorithm ID defined in pre-UEFI firmware
//
typedef enum {
  PLATFORM_ALGORITHM_SHA1 = 1,
  PLATFORM_ALGORITHM_SHA256
} PLATFORM_ALGORITHM_ID;

//
// Platform digest data definition
//
typedef union {
  unsigned char Sha1[SHA1_DIGEST_SIZE];
  unsigned char Sha256[SHA256_DIGEST_SIZE];
} PLATFORM_TPM_DIGEST;

#define MAX_VIRTUAL_PCR_INDEX   0x0002

#pragma pack(1)
typedef struct {
  PLATFORM_ALGORITHM_ID AlgorithmId;
  struct {
    PLATFORM_TPM_DIGEST Hash;
  } VPcr[MAX_VIRTUAL_PCR_INDEX]; // vPCR 0 or 1
} PLATFORM_VPCR_HASH_INFO;

typedef struct {
  UINT8  InterfaceType;              // If I/F is CRB then CRB parameters are expected
  UINT64 InterfaceParametersAddress; // Physical address of interface, by Value */
  UINT64 InterfaceParametersLength;
  UINT32 SupportedAlgorithmsBitMask;
  UINT64 EventLogAddress;
  UINT64 EventLogLength;
  UINT8  Reserved[3];
} PLATFORM_TPM2_CONFIG_DATA;

typedef struct {
  UINT64 AddressOfControlArea;
  UINT64 ControlAreaLength;
  UINT8  InterruptMode;
  UINT8  Reserved[3];
  UINT32 InterruptNumber;         // Should have a value of zero polling
  UINT32 SmcFunctionId;           // SMC Function ID
} PLATFORM_TPM2_CRB_INTERFACE_PARAMETERS;

typedef struct {
  PLATFORM_TPM2_CONFIG_DATA              Tpm2ConfigData;
  PLATFORM_TPM2_CRB_INTERFACE_PARAMETERS Tpm2CrbInterfaceParams;
  PLATFORM_VPCR_HASH_INFO                Tpm2VPcrHashInfo;
} PLATFORM_TPM2_INFO;
#pragma pack()

typedef struct {
  UINT64           PcpClk;
  UINT64           PmdClk;
  UINT64           SocClk;
  UINT64           AhbClk;
  UINT64           AxiClk;
  UINT64           ApbClk;
  UINT64           IobaxiClk;
  UINT8            CpuInfo[128];
  UINT8            CpuVer[32];
  UINT8            SmPmProVer[32];
  UINT8            SmPmProBuild[32];
  PlatformDramInfo DramInfo;
  PlatformDimmList DimmList;
} PlatformInfoHob;

typedef struct {
  UINT8              MajorNumber;
  UINT8              MinorNumber;
  UINT64             PcpClk;
  UINT64             CpuClk;
  UINT64             SocClk;
  UINT64             AhbClk;
  UINT64             SysClk;
  UINT8              CpuInfo[128];
  UINT8              CpuVer[32];
  UINT8              SmPmProVer[32];
  UINT8              SmPmProBuild[32];
  PlatformDramInfoV2 DramInfo;
  PlatformDimmListV2 DimmList;
  PlatformClusterEn  ClusterEn[2];
  UINT32             FailSafeStatus;
  UINT32             RcDisableMask[2];
  UINT8              ResetStatus;
  UINT16             CoreVoltage[2];
  UINT16             SocVoltage[2];
  UINT16             Dimm1Voltage[2];
  UINT16             Dimm2Voltage[2];

  /* Chip information */
  UINT32 ScuProductId[2];
  UINT8  MaxNumOfCore[2];
  UINT8  Warranty[2];
  UINT8  SubNumaMode[2];
  UINT8  AvsEnable[2];
  UINT32 AvsVoltageMV[2];
  UINT8  TurboCapability[2];
  UINT32 TurboFrequency[2];

  UINT8  SkuMaxTurbo[2];
  UINT8  SkuMaxCore[2];
  UINT32 AHBCId[2];

  /* TPM2 Info */
  PLATFORM_TPM2_INFO Tpm2Info;

  /* 2P link info for RCA0/RCA1 */
  UINT8 Link2PSpeed[2];
  UINT8 Link2PWidth[2];

} PlatformInfoHob_V2;

#endif /* PLATFORM_INFO_HOB_H_ */
