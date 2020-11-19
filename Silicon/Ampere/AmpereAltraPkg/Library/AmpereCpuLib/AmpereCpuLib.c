/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Base.h>
#include <PiPei.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/AmpereCpuLib.h>
#include <Library/NVParamLib.h>
#include <Platform/Ac01.h>
#include <PlatformInfoHob.h>
#include <NVParamDef.h>

#define MONOLITIC_NUM_OF_REGION        1
#define HEMISPHERE_NUM_OF_REGION       2
#define QUADRANT_NUM_OF_REGION         4
#define SUBNUMA_CPM_REGION_SIZE        4
#define NUM_OF_CPM_PER_MESH_ROW        8

STATIC
PlatformInfoHob_V2* GetPlatformHob (VOID)
{
  VOID                *Hob;
  CONST EFI_GUID      PlatformHobGuid = PLATFORM_INFO_HOB_GUID_V2;

  Hob = GetFirstGuidHob (&PlatformHobGuid);
  if (!Hob) {
    return NULL;
  }

  return (PlatformInfoHob_V2 *) GET_GUID_HOB_DATA (Hob);
}

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

/**
  Get the number of socket supported.

  @return   UINT32      Number of supported socket.

**/
EFIAPI
UINT32
GetNumberSupportedSockets (VOID)
{
  PlatformInfoHob_V2   *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (!PlatformHob) {
      return 0;
  }

  return (sizeof (PlatformHob->ClusterEn) / sizeof (PlatformClusterEn));
}

/**
  Get the number of active socket.

  @return   UINT32      Number of active socket.

**/
EFIAPI
UINT32
GetNumberActiveSockets (VOID)
{
  UINTN                NumberActiveSockets, Count, Index, Index1;
  PlatformClusterEn    *Socket;
  PlatformInfoHob_V2   *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (!PlatformHob) {
      return 0;
  }

  NumberActiveSockets = 0;

  for (Index = 0; Index < GetNumberSupportedSockets (); Index++) {
    Socket = &PlatformHob->ClusterEn[Index];
    Count = sizeof (Socket->EnableMask) / sizeof (Socket->EnableMask[0]);
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

  @param    UINT32      Socket index.
  @return   UINT32      Number of CPM.

**/
EFIAPI
UINT32
GetNumberActiveCPMsPerSocket (UINT32 SocketId)
{
  UINTN                NumberCPMs, Count, Index;
  UINT32               Val32;
  PlatformClusterEn    *Socket;
  PlatformInfoHob_V2   *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (!PlatformHob) {
      return 0;
  }

  if (SocketId >= GetNumberSupportedSockets ()) {
    return 0;
  }

  NumberCPMs = 0;
  Socket = &PlatformHob->ClusterEn[SocketId];
  Count = sizeof (Socket->EnableMask) / sizeof (Socket->EnableMask[0]);
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
  Get the configured number of CPM per socket.
  @param    UINTN       Socket ID

  @return   UINT32      Configurated number of CPM.

**/
EFIAPI
UINT32
GetConfiguratedNumberCPMs (IN UINTN Socket)
{
  EFI_STATUS  Status;
  UINT32      Value;
  UINT32      Param, ParamStart, ParamEnd;
  INTN        Count;

  Count = 0;
  ParamStart = NV_SI_S0_PCP_ACTIVECPM_0_31 + Socket * NV_PARAM_ENTRYSIZE * (PLATFORM_CPU_MAX_CPM / 32);
  ParamEnd = ParamStart + NV_PARAM_ENTRYSIZE * (PLATFORM_CPU_MAX_CPM / 32);
  for (Param = ParamStart; Param < ParamEnd; Param += NV_PARAM_ENTRYSIZE) {
    Status = NVParamGet (Param, NV_PERM_ATF | NV_PERM_BIOS |
                              NV_PERM_MANU |NV_PERM_BMC, &Value);
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

  @param    Number      Number of CPM to be configurated.
  @return   UINT32      Configurated number of CPM.

**/
EFIAPI
EFI_STATUS
SetConfiguratedNumberCPMs (IN UINTN Socket, IN UINTN Number)
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT32      Value;
  UINT32      Param, ParamStart, ParamEnd;
  BOOLEAN     IsClear = FALSE;

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
      Status = NVParamClr (Param, NV_PERM_BIOS | NV_PERM_MANU);
    } else {
      Status = NVParamSet (Param, NV_PERM_ATF | NV_PERM_BIOS |
                              NV_PERM_MANU |NV_PERM_BMC,
                              NV_PERM_BIOS | NV_PERM_MANU,
                              Value);
    }
  }

  return Status;
}

/**
  Get the maximum number of core per socket. This number
  should be the same for all sockets.

  @return   UINT32      Maximum number of core.

**/
EFIAPI
UINT32
GetMaximumNumberOfCores (VOID)
{

  PlatformInfoHob_V2   *PlatformHob;

  PlatformHob = GetPlatformHob ();
  if (!PlatformHob) {
      return 0;
  }

  return PlatformHob->MaxNumOfCore[0];
}

/**
  Get the maximum number of CPM per socket. This number
  should be the same for all sockets.

  @return   UINT32      Maximum number of CPM.

**/
EFIAPI
UINT32
GetMaximumNumberCPMs (VOID)
{
  return GetMaximumNumberOfCores() / PLATFORM_CPU_NUM_CORES_PER_CPM;
}

/**
  Get the number of active cores of a sockets.

  @return   UINT32      Number of active core.

**/
EFIAPI
UINT32
GetNumberActiveCoresPerSocket (UINT32 SocketId)
{
  return GetNumberActiveCPMsPerSocket (SocketId) * PLATFORM_CPU_NUM_CORES_PER_CPM;
}

/**
  Get the number of active cores of all sockets.

  @return   UINT32      Number of active core.

**/
EFIAPI
UINT32
GetNumberActiveCores (VOID)
{
  UINTN   NumberActiveCores;
  UINTN   Index;

  NumberActiveCores = 0;

  for (Index = 0; Index < GetNumberSupportedSockets (); Index++) {
    NumberActiveCores += GetNumberActiveCoresPerSocket (Index);
  }

  return NumberActiveCores;
}

/**
  Check if the logical CPU is enabled or not.

  @param    Cpu         The logical Cpu ID. Started from 0.
  @return   BOOLEAN     TRUE if the Cpu enabled
                        FALSE if the Cpu disabled/

**/
EFIAPI
BOOLEAN
IsCpuEnabled (IN UINTN Cpu)
{
  PlatformClusterEn    *Socket;
  PlatformInfoHob_V2   *PlatformHob;
  UINT32               SocketId = Cpu / (PLATFORM_CPU_MAX_CPM * PLATFORM_CPU_NUM_CORES_PER_CPM);
  UINT32               ClusterId = (Cpu / PLATFORM_CPU_NUM_CORES_PER_CPM) % PLATFORM_CPU_MAX_CPM;

  PlatformHob = GetPlatformHob ();
  if (!PlatformHob) {
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
EFIAPI
BOOLEAN
PlatSlaveSocketPresent (VOID)
{
  UINT32 Val;

  Val = MmioRead32 (SMPRO_EFUSE_SHADOW0 + CFG2P_OFFSET);
  return (Val & SLAVE_PRESENT_N)? FALSE : TRUE;
}
