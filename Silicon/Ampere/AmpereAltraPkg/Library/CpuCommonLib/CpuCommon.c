/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <PiPei.h>
#include <Uefi/UefiSpec.h>
#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/AmpereCpuLib.h>
#include <Library/HobLib.h>
#include <Library/PlatformInfo.h>
#include <Library/CpuCommonLib.h>

#define MONOLITIC_NUM_OF_REGION        1
#define HEMISPHERE_NUM_OF_REGION       2
#define QUADRANT_NUM_OF_REGION         4
#define SUBNUMA_CPM_REGION_SIZE        4
#define NUM_OF_CPM_PER_MESH_ROW        8

EFIAPI
UINT8
CPUGetSubNumaMode (VOID)
{
  EFI_GUID                    PlatformHobGuid = PLATFORM_INFO_HOB_GUID_V2;
  PlatformInfoHob_V2          *PlatformHob;
  VOID                        *Hob;

  /* Get the Platform HOB */
  Hob = GetFirstGuidHob (&PlatformHobGuid);
  if (!Hob) {
    return SUBNUMA_MODE_MONOLITHIC;
  }
  PlatformHob = (PlatformInfoHob_V2 *) GET_GUID_HOB_DATA (Hob);

  return PlatformHob->SubNumaMode[0];
}

EFIAPI
UINT8
CPUGetNumOfSubNuma (VOID)
{
  UINT8               SubNumaMode = CPUGetSubNumaMode();

  switch (SubNumaMode) {
  case SUBNUMA_MODE_MONOLITHIC:
    return MONOLITIC_NUM_OF_REGION;
  case SUBNUMA_MODE_HEMISPHERE:
    return HEMISPHERE_NUM_OF_REGION;
  case SUBNUMA_MODE_QUADRANT:
    return QUADRANT_NUM_OF_REGION;
  }

  return QUADRANT_NUM_OF_REGION;
}

EFIAPI
UINT8
CPUGetSubNumNode (UINT8 Socket, UINT32 Cpm)
{
  UINT8               MaxNumOfCPM = GetMaximumNumberCPMs();
  UINT8               SubNumaMode = CPUGetSubNumaMode();
  INTN                Ret = 0;
  UINT8               AsymMesh = 0;
  UINT8               AsymMeshRow = 0;

  switch (SubNumaMode) {
  case SUBNUMA_MODE_MONOLITHIC:
    if (Socket == 0) {
      Ret = 0;
    } else {
      Ret = 1;
    }
    break;

  case SUBNUMA_MODE_HEMISPHERE:
    if ((Cpm % NUM_OF_CPM_PER_MESH_ROW) / SUBNUMA_CPM_REGION_SIZE) {
      Ret = 1;
    } else {
      Ret = 0;
    }
    if (Socket == 1) {
      Ret += HEMISPHERE_NUM_OF_REGION;
    }
    break;

  case SUBNUMA_MODE_QUADRANT:
    // For asymmetric mesh, the CPM at the middle row is distributed
    // equally to each node. As each mesh row has 8 CPMs,
    //   First pair of CPMs: Node 0
    //   Second pair of CPMs: Node 1
    //   Third pair of CPMs: Node 3
    //   Forth paif of CPMs: Node 2
    AsymMesh = (MaxNumOfCPM / NUM_OF_CPM_PER_MESH_ROW) % 2;
    if (AsymMesh) {
      AsymMeshRow = (MaxNumOfCPM / NUM_OF_CPM_PER_MESH_ROW) / 2;
    }
    if (AsymMesh && ((Cpm / NUM_OF_CPM_PER_MESH_ROW) == AsymMeshRow)) {
      switch ((Cpm % NUM_OF_CPM_PER_MESH_ROW) / 2) {
      case 0:
        Ret = 0;
        break;
      case 1:
        Ret = 1;
        break;
      case 2:
        Ret = 3;
        break;
      case 3:
        Ret = 2;
        break;
      }
    } else {
      if (Cpm < (MaxNumOfCPM / 2)) {
        if ((Cpm % NUM_OF_CPM_PER_MESH_ROW) / SUBNUMA_CPM_REGION_SIZE) {
          Ret = 2;
        } else {
          Ret = 0;
        }
      } else {
        if ((Cpm % NUM_OF_CPM_PER_MESH_ROW) / SUBNUMA_CPM_REGION_SIZE) {
          Ret = 3;
        } else {
          Ret = 1;
        }
      }
    }

    if (Socket == 1) {
      Ret += QUADRANT_NUM_OF_REGION;
    }
    break;
  }

  return Ret;
}

EFIAPI
UINT64
Aarch64ReadCLIDRReg ()
{
  UINT64 Val;

  asm volatile("mrs %x0, clidr_el1 " : "=r" (Val));

  return Val;
}

EFIAPI
UINT64
Aarch64ReadCCSIDRReg (UINT64 Level)
{
  UINT64 Val;

  asm volatile("msr csselr_el1, %x0 " : : "rZ" (Level));
  asm volatile("mrs %x0, ccsidr_el1 " : "=r" (Val));

  return Val;
}

EFIAPI
UINT32
CpuGetAssociativity (UINTN Level)
{
  UINT64 CacheCCSIDR;
  UINT64 CacheCLIDR = Aarch64ReadCLIDRReg ();
  UINT32  Val = 0x2; /* Unknown Set-Associativity */

  if (!CLIDR_CTYPE (CacheCLIDR, Level)) {
    return Val;
  }

  CacheCCSIDR = Aarch64ReadCCSIDRReg (Level);
  switch (CCSIDR_ASSOCIATIVITY (CacheCCSIDR)) {
  case 0:
    /* Direct mapped */
    Val = 0x3;
    break;
  case 1:
    /* 2-way Set-Associativity */
    Val = 0x4;
    break;
  case 3:
    /* 4-way Set-Associativity */
    Val = 0x5;
    break;
  case 7:
    /* 8-way Set-Associativity */
    Val = 0x7;
    break;
  case 15:
    /* 16-way Set-Associativity */
    Val = 0x8;
    break;
  case 11:
    /* 12-way Set-Associativity */
    Val = 0x9;
    break;
  case 23:
    /* 24-way Set-Associativity */
    Val = 0xA;
    break;
  case 31:
    /* 32-way Set-Associativity */
    Val = 0xB;
    break;
  case 47:
    /* 48-way Set-Associativity */
    Val = 0xC;
    break;
  case 63:
    /* 64-way Set-Associativity */
    Val = 0xD;
    break;
  case 19:
    /* 20-way Set-Associativity */
    Val = 0xE;
    break;
  }

  return Val;
}

EFIAPI
UINT32
CpuGetCacheSize (UINTN Level)
{
  UINT32 CacheLineSize;
  UINT32 Count;
  UINT64 CacheCCSIDR;
  UINT64 CacheCLIDR = Aarch64ReadCLIDRReg ();

  if (!CLIDR_CTYPE (CacheCLIDR, Level)) {
    return 0;
  }

  CacheCCSIDR = Aarch64ReadCCSIDRReg (Level);
  CacheLineSize = 1;
  Count = CCSIDR_LINE_SIZE (CacheCCSIDR) + 4;
  while (Count-- > 0) {
    CacheLineSize *= 2;
  }

  return ((CCSIDR_NUMSETS (CacheCCSIDR) + 1) *
          (CCSIDR_ASSOCIATIVITY (CacheCCSIDR) + 1) *
          CacheLineSize);
}
