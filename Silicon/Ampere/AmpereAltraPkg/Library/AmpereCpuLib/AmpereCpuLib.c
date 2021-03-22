/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Uefi.h>

#include <Guid/PlatformInfoHobGuid.h>
#include <Library/AmpereCpuLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/NVParamLib.h>
#include <NVParamDef.h>
#include <Platform/Ac01.h>
#include <PlatformInfoHob.h>

STATIC PlatformInfoHob *
GetPlatformHob (
  VOID
  )
{
  VOID *Hob;

  Hob = GetFirstGuidHob (&gPlatformHobGuid);
  if (Hob == NULL) {
    return NULL;
  }

  return (PlatformInfoHob *)GET_GUID_HOB_DATA (Hob);
}

/**
  Get the SubNUMA mode.

  @return   UINT8      The SubNUMA mode.

**/
UINT8
EFIAPI
CpuGetSubNumaMode (
  VOID
  )
{
  PlatformInfoHob    *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
    return SUBNUMA_MODE_MONOLITHIC;
  }

  return PlatformHob->SubNumaMode[0];
}

/**
  Get the number of SubNUMA region.

  @return   UINT8      The number of SubNUMA region.

**/
UINT8
EFIAPI
CpuGetNumOfSubNuma (
  VOID
  )
{
  UINT8 SubNumaMode = CpuGetSubNumaMode ();

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

/**
  Get the SubNUMA node of a CPM.

  @param    SocketId    Socket index.
  @param    Cpm         CPM index.
  @return   UINT8       The SubNUMA node of a CPM.

**/
UINT8
EFIAPI
CpuGetSubNumNode (
  UINT8  SocketId,
  UINT32 Cpm
  )
{
  BOOLEAN IsAsymMesh;
  INTN    Ret;
  UINT8   MaxNumOfCPM;
  UINT8   MiddleRow;
  UINT8   QuadrantHigherRowNodeNumber[NUM_OF_CPM_PER_MESH_ROW] = {1, 1, 1, 1, 3, 3, 3, 3};
  UINT8   QuadrantLowerRowNodeNumber[NUM_OF_CPM_PER_MESH_ROW]  = {0, 0, 0, 0, 2, 2, 2, 2};
  UINT8   QuadrantMiddleRowNodeNumber[NUM_OF_CPM_PER_MESH_ROW] = {0, 0, 1, 1, 3, 3, 2, 2};
  UINT8   SubNumaMode;

  MaxNumOfCPM = GetMaximumNumberCPMs ();
  SubNumaMode = CpuGetSubNumaMode ();

  switch (SubNumaMode) {
  case SUBNUMA_MODE_MONOLITHIC:
    Ret = (SocketId == 0) ? 0 : 1;
    break;

  case SUBNUMA_MODE_HEMISPHERE:
    if (CPM_PER_ROW_OFFSET (Cpm) >= SUBNUMA_CPM_REGION_SIZE) {
      Ret = 1;
    } else {
      Ret = 0;
    }

    if (SocketId == 1) {
      Ret += HEMISPHERE_NUM_OF_REGION;
    }
    break;

  case SUBNUMA_MODE_QUADRANT:
    //
    // CPM Mesh Rows
    //
    // |---------------------------------------|
    // | 00 ----------- 03 | 04 ----------- 07 | Row 0
    // |-------------------|-------------------|
    // | 08 ----------- 11 | 12 ----------- 15 | Row 1
    // |-------------------|-------------------|
    // | 16 - 17 | 18 - 19 | 20 - 21 | 22 - 23 | Middle Row
    // |-------------------|-------------------|
    // | 24 ----------- 27 | 28 ----------- 31 | Row 3
    // |-------------------|-------------------|
    // | 32 ----------- 35 | 36 ----------- 39 | Row 4
    // |---------------------------------------|
    //

    IsAsymMesh = (BOOLEAN)(CPM_ROW_NUMBER (MaxNumOfCPM) % 2 != 0);
    MiddleRow = CPM_ROW_NUMBER (MaxNumOfCPM) / 2;
    if (IsAsymMesh
        && CPM_ROW_NUMBER (Cpm) == MiddleRow)
    {
      Ret = QuadrantMiddleRowNodeNumber[CPM_PER_ROW_OFFSET (Cpm)];

    } else if (CPM_ROW_NUMBER (Cpm) >= MiddleRow) {
      Ret = QuadrantHigherRowNodeNumber[CPM_PER_ROW_OFFSET (Cpm)];

    } else {
      Ret = QuadrantLowerRowNodeNumber[CPM_PER_ROW_OFFSET (Cpm)];
    }

    if (SocketId == 1) {
      Ret += QUADRANT_NUM_OF_REGION;
    }
    break;
  }

  return Ret;
}

/**
  Get the value of CLIDR register.

  @return   UINT64      The value of CLIDR register.

**/
UINT64
EFIAPI
AArch64ReadCLIDRReg (
  VOID
  )
{
  UINT64 Value;

  asm volatile ("mrs %x0, clidr_el1 " : "=r" (Value));

  return Value;
}

/**
  Get the value of CCSID register.

  @param    Level       Cache level.
  @return   UINT64      The value of CCSID register.

**/
UINT64
EFIAPI
AArch64ReadCCSIDRReg (
  UINT64 Level
  )
{
  UINT64 Value;

  asm volatile ("msr csselr_el1, %x0 " : : "rZ" (Level));
  asm volatile ("mrs %x0, ccsidr_el1 " : "=r" (Value));

  return Value;
}

/**
  Get the associativity of cache.

  @param    Level       Cache level.
  @return   UINT32      Associativity of cache.

**/
UINT32
EFIAPI
CpuGetAssociativity (
  UINTN Level
  )
{
  UINT64 CacheCCSIDR;
  UINT64 CacheCLIDR = AArch64ReadCLIDRReg ();
  UINT32 Value = 0x2; /* Unknown Set-Associativity */

  if (!CLIDR_CTYPE (CacheCLIDR, Level)) {
    return Value;
  }

  CacheCCSIDR = AArch64ReadCCSIDRReg (Level);
  switch (CCSIDR_ASSOCIATIVITY (CacheCCSIDR)) {
  case 0:
    /* Direct mapped */
    Value = 0x3;
    break;

  case 1:
    /* 2-way Set-Associativity */
    Value = 0x4;
    break;

  case 3:
    /* 4-way Set-Associativity */
    Value = 0x5;
    break;

  case 7:
    /* 8-way Set-Associativity */
    Value = 0x7;
    break;

  case 15:
    /* 16-way Set-Associativity */
    Value = 0x8;
    break;

  case 11:
    /* 12-way Set-Associativity */
    Value = 0x9;
    break;

  case 23:
    /* 24-way Set-Associativity */
    Value = 0xA;
    break;

  case 31:
    /* 32-way Set-Associativity */
    Value = 0xB;
    break;

  case 47:
    /* 48-way Set-Associativity */
    Value = 0xC;
    break;

  case 63:
    /* 64-way Set-Associativity */
    Value = 0xD;
    break;

  case 19:
    /* 20-way Set-Associativity */
    Value = 0xE;
    break;
  }

  return Value;
}

/**
  Get the cache size.

  @param    Level       Cache level.
  @return   UINT32      Cache size.

**/
UINT32
EFIAPI
CpuGetCacheSize (
  UINTN Level
  )
{
  UINT32 CacheLineSize;
  UINT32 Count;
  UINT64 CacheCCSIDR;
  UINT64 CacheCLIDR = AArch64ReadCLIDRReg ();

  if (!CLIDR_CTYPE (CacheCLIDR, Level)) {
    return 0;
  }

  CacheCCSIDR = AArch64ReadCCSIDRReg (Level);
  CacheLineSize = 1;
  Count = CCSIDR_LINE_SIZE (CacheCCSIDR) + 4;
  while (Count-- > 0) {
    CacheLineSize *= 2;
  }

  return ((CCSIDR_NUMSETS (CacheCCSIDR) + 1) *
          (CCSIDR_ASSOCIATIVITY (CacheCCSIDR) + 1) *
          CacheLineSize);
}

/**
  Get the number of supported socket.

  @return   UINT32      Number of supported socket.

**/
UINT32
EFIAPI
GetNumberSupportedSockets (
  VOID
  )
{
  PlatformInfoHob    *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
    return 0;
  }

  return (sizeof (PlatformHob->ClusterEn) / sizeof (PlatformClusterEn));
}

/**
  Get the number of active socket.

  @return   UINT32      Number of active socket.

**/
UINT32
EFIAPI
GetNumberActiveSockets (
  VOID
  )
{
  UINTN              NumberActiveSockets, Count, Index, Index1;
  PlatformClusterEn  *Socket;
  PlatformInfoHob    *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
    return 0;
  }

  NumberActiveSockets = 0;

  for (Index = 0; Index < GetNumberSupportedSockets (); Index++) {
    Socket = &PlatformHob->ClusterEn[Index];
    Count = ARRAY_SIZE (Socket->EnableMask);
    for (Index1 = 0; Index1 < Count; Index1++) {
      if (Socket->EnableMask[Index1]) {
        NumberActiveSockets++;
        break;
      }
    }
  }

  return NumberActiveSockets;
}

/**
  Get the number of active CPM per socket.

  @param    SocketId    Socket index.
  @return   UINT32      Number of CPM.

**/
UINT32
EFIAPI
GetNumberActiveCPMsPerSocket (
  UINT32 SocketId
  )
{
  UINTN              NumberCPMs, Count, Index;
  UINT32             Val32;
  PlatformClusterEn  *Socket;
  PlatformInfoHob    *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
    return 0;
  }

  if (SocketId >= GetNumberSupportedSockets ()) {
    return 0;
  }

  NumberCPMs = 0;
  Socket = &PlatformHob->ClusterEn[SocketId];
  Count = ARRAY_SIZE (Socket->EnableMask);
  for (Index = 0; Index < Count; Index++) {
    Val32 = Socket->EnableMask[Index];
    while (Val32) {
      if (Val32 & 0x1) {
        NumberCPMs++;
      }
      Val32 >>= 1;
    }
  }

  return NumberCPMs;
}

/**
  Get the configured number of CPM per socket. This number
  should be the same for all sockets.

  @param    SocketId    Socket index.
  @return   UINT32      Configured number of CPM.

**/
UINT32
EFIAPI
GetConfiguredNumberCPMs (
  UINTN SocketId
  )
{
  EFI_STATUS Status;
  UINT32     Value;
  UINT32     Param, ParamStart, ParamEnd;
  INTN       Count;

  Count = 0;
  ParamStart = NV_SI_S0_PCP_ACTIVECPM_0_31 + SocketId * NV_PARAM_ENTRYSIZE * (PLATFORM_CPU_MAX_CPM / 32);
  ParamEnd = ParamStart + NV_PARAM_ENTRYSIZE * (PLATFORM_CPU_MAX_CPM / 32);
  for (Param = ParamStart; Param < ParamEnd; Param += NV_PARAM_ENTRYSIZE) {
    Status = NVParamGet (
               Param,
               NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
               &Value
               );
    if (EFI_ERROR (Status)) {
      break;
    }
    while (Value) {
      if (Value & 1) {
        Count++;
      }
      Value >>= 1;
    }
  }

  return Count;
}

/**
  Set the configured number of CPM per socket.

  @param    SocketId        Socket index.
  @param    Number          Number of CPM to be configured.
  @return   EFI_SUCCESS     Operation succeeded.
  @return   Others          An error has occurred.

**/
EFI_STATUS
EFIAPI
SetConfiguredNumberCPMs (
  UINTN Socket,
  UINTN Number
  )
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINT32     Value;
  UINT32     Param, ParamStart, ParamEnd;
  BOOLEAN    IsClear = FALSE;

  if (Number == 0) {
    IsClear = TRUE;
  }

  ParamStart = NV_SI_S0_PCP_ACTIVECPM_0_31 + Socket * NV_PARAM_ENTRYSIZE * (PLATFORM_CPU_MAX_CPM / 32);
  ParamEnd = ParamStart + NV_PARAM_ENTRYSIZE * (PLATFORM_CPU_MAX_CPM / 32);
  for (Param = ParamStart; Param < ParamEnd; Param += NV_PARAM_ENTRYSIZE) {
    if (Number >= 32) {
      Value = 0xffffffff;
      Number -= 32;
    } else {
      Value = 0;
      while (Number > 0) {
        Value |= (1 << (--Number));
      }
    }
    if (IsClear) {
      /* Clear this param */
      Status = NVParamClr (
                 Param,
                 NV_PERM_BIOS | NV_PERM_MANU
                 );
    } else {
      Status = NVParamSet (
                 Param,
                 NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
                 NV_PERM_BIOS | NV_PERM_MANU,
                 Value
                 );
    }
  }

  return Status;
}

/**
  Get the maximum number of core per socket.

  @return   UINT32      Maximum number of core.

**/
UINT32
EFIAPI
GetMaximumNumberOfCores (
  VOID
  )
{

  PlatformInfoHob    *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
    return 0;
  }

  return PlatformHob->MaxNumOfCore[0];
}

/**
  Get the maximum number of CPM per socket. This number
  should be the same for all sockets.

  @return   UINT32      Maximum number of CPM.

**/
UINT32
EFIAPI
GetMaximumNumberCPMs (
  VOID
  )
{
  return GetMaximumNumberOfCores () / PLATFORM_CPU_NUM_CORES_PER_CPM;
}

/**
  Get the number of active cores of a sockets.

  @param    SocketId    Socket Index.
  @return   UINT32      Number of active core.

**/
UINT32
EFIAPI
GetNumberActiveCoresPerSocket (
  UINT32 SocketId
  )
{
  return GetNumberActiveCPMsPerSocket (SocketId) * PLATFORM_CPU_NUM_CORES_PER_CPM;
}

/**
  Get the number of active cores of all sockets.

  @return   UINT32      Number of active core.

**/
UINT32
EFIAPI
GetNumberActiveCores (
  VOID
  )
{
  UINTN NumberActiveCores;
  UINTN Index;

  NumberActiveCores = 0;

  for (Index = 0; Index < GetNumberSupportedSockets (); Index++) {
    NumberActiveCores += GetNumberActiveCoresPerSocket (Index);
  }

  return NumberActiveCores;
}

/**
  Check if the logical CPU is enabled or not.

  @param    CpuId       The logical Cpu ID. Started from 0.
  @return   BOOLEAN     TRUE if the Cpu enabled
                        FALSE if the Cpu disabled/

**/
BOOLEAN
EFIAPI
IsCpuEnabled (
  UINTN CpuId
  )
{
  PlatformClusterEn  *Socket;
  PlatformInfoHob    *PlatformHob;
  UINT32             SocketId;
  UINT32             ClusterId;

  SocketId = SOCKET_ID (CpuId);
  ClusterId = CLUSTER_ID (CpuId);

  PlatformHob = GetPlatformHob ();
  if (PlatformHob == NULL) {
    return FALSE;
  }

  if (SocketId >= GetNumberSupportedSockets ()) {
    return FALSE;
  }

  Socket = &PlatformHob->ClusterEn[SocketId];
  if (Socket->EnableMask[ClusterId / 32] & (1 << (ClusterId % 32))) {
    return TRUE;
  }

  return FALSE;
}

/**
  Check if the slave socket is present

  @return   BOOLEAN     TRUE if the Slave Cpu present
                        FALSE if the Slave Cpu present

**/
BOOLEAN
EFIAPI
PlatSlaveSocketPresent (
  VOID
  )
{
  UINT32 Value;

  Value = MmioRead32 (SMPRO_EFUSE_SHADOW0 + CFG2P_OFFSET);
  return (Value & SLAVE_PRESENT_N)? FALSE : TRUE;
}
