/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/AmpereCpuLib.h>
#include <Library/ArmGenericTimerCounterLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BoardPcieLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/PrintLib.h>
#include <Library/SerialPortLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Platform/Ac01.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>

#include "PcieCore.h"
#include "PciePatchAcpi.h"

STATIC UINT64  RCRegBase[AC01_MAX_PCIE_ROOT_COMPLEX] = { AC01_PCIE_REGISTER_BASE };
STATIC UINT64  RCMmioBase[AC01_MAX_PCIE_ROOT_COMPLEX] = { AC01_PCIE_MMIO_BASE };
STATIC UINT64  RCMmioSize[AC01_MAX_PCIE_ROOT_COMPLEX] = {AC01_PCIE_MMIO_SIZE};
STATIC UINT64  RCMmio32Base[AC01_MAX_PCIE_ROOT_COMPLEX] = { AC01_PCIE_MMIO32_BASE };
STATIC UINT64  RCMmio32Base1P[AC01_MAX_PCIE_ROOT_COMPLEX] = { AC01_PCIE_MMIO32_BASE_1P };
STATIC UINT64  RCMmio32Size[AC01_MAX_PCIE_ROOT_COMPLEX] = {AC01_PCIE_MMIO32_SIZE};
STATIC UINT64  RCMmio32Size1P[AC01_MAX_PCIE_ROOT_COMPLEX] = {AC01_PCIE_MMIO32_SIZE_1P};
STATIC AC01_RC RCList[AC01_MAX_PCIE_ROOT_COMPLEX];
STATIC INT8    PciList[AC01_MAX_PCIE_ROOT_COMPLEX];

STATIC
VOID
SerialPrint (
  IN CONST CHAR8 *FormatString,
  ...
  )
{
  UINT8   Buf[64];
  VA_LIST Marker;
  UINTN   NumberOfPrinted;

  VA_START (Marker, FormatString);
  NumberOfPrinted = AsciiVSPrint ((CHAR8 *)Buf, sizeof (Buf), FormatString, Marker);
  SerialPortWrite (Buf, NumberOfPrinted);
  VA_END (Marker);
}

AC01_RC *
GetRCList (
  UINT8 Idx
  )
{
  return &RCList[Idx];
}

/**
   Map BusDxe Host bridge and Root bridge Indexes to PCIE core BSP driver Root Complex Index.

   @param  HBIndex               Index to identify of PCIE Host bridge.
   @param  RBIndex               Index to identify of PCIE Root bridge.
   @retval UINT8                 Index to identify Root Complex instance from global RCList.
**/
STATIC
UINT8
GetRCIndex (
  IN UINT8 HBIndex,
  IN UINT8 RBIndex
  )
{
  //
  // BusDxe addresses resources based on Host bridge and Root bridge.
  // Map those to Root Complex index/instance known for Pcie Core BSP driver
  //
  return HBIndex * AC01_MAX_PCIE_ROOT_BRIDGE + RBIndex;
}

/**
  Prepare to start PCIE core BSP driver

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
Ac01PcieSetup (
  VOID
  )
{
  AC01_RC *RC;
  INTN    RCIndex;

  ZeroMem (&RCList, sizeof (AC01_RC) * AC01_MAX_PCIE_ROOT_COMPLEX);

  // Adjust MMIO32 base address 1P vs 2P
  if (!IsSlaveSocketPresent ()) {
    CopyMem ((VOID *)&RCMmio32Base, (VOID *)&RCMmio32Base1P, sizeof (RCMmio32Base1P));
    CopyMem ((VOID *)&RCMmio32Size, (VOID *)&RCMmio32Size1P, sizeof (RCMmio32Size1P));
  }

  for (RCIndex = 0; RCIndex < AC01_MAX_PCIE_ROOT_COMPLEX; RCIndex++) {
    RC = &RCList[RCIndex];
    RC->Socket = RCIndex / AC01_MAX_RCS_PER_SOCKET;
    RC->ID = RCIndex % AC01_MAX_RCS_PER_SOCKET;

    Ac01PcieCoreBuildRCStruct (
      RC,
      RCRegBase[RCIndex],
      RCMmioBase[RCIndex],
      RCMmioSize[RCIndex],
      RCMmio32Base[RCIndex],
      RCMmio32Size[RCIndex]
      );
  }

  // Build the UEFI menu
  BoardPcieScreenInitialize (RCList);

  return EFI_SUCCESS;
}

/**
  Get Total HostBridge.

  @retval UINTN                     Return Total HostBridge.
**/
UINT8
Ac01PcieGetTotalHBs (
  VOID
  )
{
  return AC01_MAX_PCIE_ROOT_COMPLEX;
}

/**
  Get Total RootBridge per HostBridge.

  @param[in]  RCIndex               Index to identify of Root Complex.

  @retval UINTN                     Return Total RootBridge per HostBridge.
**/
UINT8
Ac01PcieGetTotalRBsPerHB (
  IN UINTN RCIndex
  )
{
  return AC01_MAX_PCIE_ROOT_BRIDGE;
}

/**
  Get RootBridge Attribute.

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.
  @param[in]  RBIndex               Index to identify of underneath PCIE Root bridge.

  @retval UINTN                     Return RootBridge Attribute.
**/
UINTN
Ac01PcieGetRootBridgeAttribute (
  IN UINTN HBIndex,
  IN UINTN RBIndex
  )
{
  return EFI_PCI_HOST_BRIDGE_MEM64_DECODE;
}

/**
  Get RootBridge Segment number

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.
  @param[in]  RBIndex               Index to identify of underneath PCIE Root bridge.

  @retval UINTN                     Return RootBridge Segment number.
**/
UINTN
Ac01PcieGetRootBridgeSegmentNumber (
  IN UINTN HBIndex,
  IN UINTN RBIndex
  )
{
  UINTN   RCIndex;
  AC01_RC *RC;
  UINTN   SegmentNumber;

  RCIndex = GetRCIndex (HBIndex, RBIndex);
  RC = &RCList[RCIndex];
  SegmentNumber = RCIndex;

  // Get board specific overrides
  BoardPcieGetRCSegmentNumber (RC, &SegmentNumber);
  RC->Logical = SegmentNumber;

  return SegmentNumber;
}

STATIC
VOID
SortPciList (
  INT8 *PciList
  )
{
  INT8 Idx1, Idx2;

  for (Idx2 = 0, Idx1 = 0; Idx2 < AC01_MAX_PCIE_ROOT_COMPLEX; Idx2++) {
    if (PciList[Idx2] < 0) {
      continue;
    }
    PciList[Idx1] = PciList[Idx2];
    if (Idx1 != Idx2) {
      PciList[Idx2] = -1;
    }
    Idx1++;
  }

  for (Idx2 = 0; Idx2 < Idx1; Idx2++) {
    DEBUG_PCIE_INFO (
      " %a: PciList[%d]=%d TcuAddr=0x%llx\n",
      __FUNCTION__,
      Idx2,
      PciList[Idx2],
      RCList[PciList[Idx2]].TcuAddr
      );
  }
}

/**
  Get RootBridge disable status.

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.
  @param[in]  RBIndex               Index to identify of underneath PCIE Root bridge.

  @retval BOOLEAN                   Return RootBridge disable status.
**/
BOOLEAN
Ac01PcieCheckRootBridgeDisabled (
  IN UINTN HBIndex,
  IN UINTN RBIndex
  )
{
  UINTN RCIndex;
  INT8  Ret;

  RCIndex = HBIndex;
  Ret = !RCList[RCIndex].Active;
  if (Ret) {
    PciList[HBIndex] = -1;
  } else {
    PciList[HBIndex] = HBIndex;
  }
  if (HBIndex == (AC01_MAX_PCIE_ROOT_COMPLEX -1)) {
    SortPciList (PciList);
    if (!IsSlaveSocketPresent ()) {
      AcpiPatchPciMem32 (PciList);
    }
    AcpiInstallMcfg (PciList);
    AcpiInstallIort (PciList);
  }
  return Ret;
}

/**
  Initialize Host bridge

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
Ac01PcieSetupHostBridge (
  IN UINTN HBIndex
  )
{
  return EFI_SUCCESS;
}

/**
  Initialize Root bridge

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.
  @param[in]  RBIndex               Index to identify of underneath PCIE Root bridge.
  @param[in]  RootBridgeInstance    The pointer of instance of the Root bridge IO.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
Ac01PcieSetupRootBridge (
  IN UINTN           HBIndex,
  IN UINTN           RBIndex,
  IN PCI_ROOT_BRIDGE *RootBridge
  )
{
  UINTN   RCIndex;
  AC01_RC *RC;
  UINT32  Result;

  RCIndex = GetRCIndex (HBIndex, RBIndex);
  RC = &RCList[RCIndex];
  if (!RC->Active) {
    return EFI_DEVICE_ERROR;
  }

  RC->RootBridge = (VOID *)RootBridge;

  // Initialize Root Complex and underneath controllers
  Result = Ac01PcieCoreSetupRC (RC, 0, 0);
  if (Result) {
    DEBUG_PCIE_ERR ("RootComplex[%d]: Init Failed\n", RCIndex);

    goto Error;
  }

  // Populate resource aperture
  RootBridge->Bus.Base = 0x0;
  RootBridge->Bus.Limit = 0xFF;
  RootBridge->Io.Base = RC->IoAddr;
  RootBridge->Io.Limit = RC->IoAddr + IO_SPACE - 1;
  RootBridge->Mem.Base = RC->Mmio32Addr;
  RootBridge->Mem.Limit = (RootBridge->Mem.Base) ? (RootBridge->Mem.Base + RC->Mmio32Size - 1) : 0;
  RootBridge->PMem.Base = RootBridge->Mem.Base;
  RootBridge->PMem.Limit = RootBridge->Mem.Limit;
  RootBridge->MemAbove4G.Base = MAX_UINT64;
  RootBridge->MemAbove4G.Limit = 0x0;
  RootBridge->PMemAbove4G.Base = RC->MmioAddr;
  RootBridge->PMemAbove4G.Limit = (RootBridge->PMemAbove4G.Base) ? (RootBridge->PMemAbove4G.Base + RC->MmioSize - 1) : 0;

  DEBUG_PCIE_INFO (" +    Bus: 0x%lx - 0x%lx\n", RootBridge->Bus.Base, RootBridge->Bus.Limit);
  DEBUG_PCIE_INFO (" +     Io: 0x%lx - 0x%lx\n", RootBridge->Io.Base, RootBridge->Io.Limit);
  DEBUG_PCIE_INFO (" +    Mem: 0x%lx - 0x%lx\n", RootBridge->Mem.Base, RootBridge->Mem.Limit);
  DEBUG_PCIE_INFO (" +   PMem: 0x%lx - 0x%lx\n", RootBridge->PMem.Base, RootBridge->PMem.Limit);
  DEBUG_PCIE_INFO (" +  4GMem: 0x%lx - 0x%lx\n", RootBridge->MemAbove4G.Base, RootBridge->MemAbove4G.Limit);
  DEBUG_PCIE_INFO (" + 4GPMem: 0x%lx - 0x%lx\n", RootBridge->PMemAbove4G.Base, RootBridge->PMemAbove4G.Limit);

  return EFI_SUCCESS;

Error:
  RC->Active = FALSE;
  return EFI_DEVICE_ERROR;
}

/**
  Reads/Writes an PCI configuration register.

  @param[in]  RootInstance          Pointer to RootInstance structure.
  @param[in]  Address               Address which want to read or write to.
  @param[in]  Write                 Indicate that this is a read or write command.
  @param[in]  Width                 Specify the width of the data.
  @param[in, out]  Data             The buffer to hold the data.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
Ac01PcieConfigRW (
  IN     VOID    *RootInstance,
  IN     UINT64  Address,
  IN     BOOLEAN Write,
  IN     UINTN   Width,
  IN OUT VOID    *Data
  )
{
  AC01_RC *RC = NULL;
  VOID    *CfgBase = NULL;
  UINTN   RCIndex;
  UINT32  Reg;
  UINT32  Segment;

  Segment = RShiftU64 (Address, 32) & 0xFFFF;

  for (RCIndex = 0; RCIndex < AC01_MAX_PCIE_ROOT_COMPLEX; RCIndex++) {
    RC = &RCList[RCIndex];
    if (RC->RootBridge != NULL) {
      if (RC->RootBridge == RootInstance
         || (RootInstance == NULL
            && ((PCI_ROOT_BRIDGE *)RC->RootBridge)->Segment == Segment)) {
      break;
      }
    }
  }

  if ((RCIndex == AC01_MAX_PCIE_ROOT_COMPLEX) || (RC == NULL)) {
    DEBUG_PCIE_ERR ("Can't find Root Bridge instance:%p\n", RootInstance);
    return EFI_INVALID_PARAMETER;
  }

  Reg = Address & 0xFFF;

  CfgBase = (VOID *)((UINT64)RC->MmcfgAddr + (Address & 0x0FFFF000));
  if (Write) {
    switch (Width) {
    case 1:
      Ac01PcieCfgOut8 ((VOID *)(CfgBase + (Reg & (~(Width - 1)))), *((UINT8 *)Data));
      break;

    case 2:
      Ac01PcieCfgOut16 ((VOID *)(CfgBase + (Reg & (~(Width - 1)))), *((UINT16 *)Data));
      break;

    case 4:
      Ac01PcieCfgOut32 ((VOID *)(CfgBase + (Reg & (~(Width - 1)))), *((UINT32 *)Data));
      break;

    default:
      return EFI_INVALID_PARAMETER;
    }
  } else {
    switch (Width) {
    case 1:
      Ac01PcieCfgIn8 ((VOID *)(CfgBase + (Reg & (~(Width - 1)))), (UINT8 *)Data);
      break;

    case 2:
      Ac01PcieCfgIn16 ((VOID *)(CfgBase + (Reg & (~(Width - 1)))), (UINT16 *)Data);
      if (Reg == 0xAE && (*((UINT16 *)Data)) == 0xFFFF) {
        SerialPrint ("PANIC due to PCIE RC:%d link issue\n", RC->ID);
        // Loop forever waiting for failsafe/watch dog time out
        do {
        } while (1);
      }
      break;

    case 4:
      Ac01PcieCfgIn32 ((VOID *)(CfgBase + (Reg & (~(Width - 1)))), (UINT32 *)Data);
      break;

    default:
      return EFI_INVALID_PARAMETER;
    }
  }

  return EFI_SUCCESS;
}

VOID
Ac01PcieCorePollLinkUp (
  VOID
  )
{
  INTN    RCIndex, PcieIndex, i;
  BOOLEAN IsNextRoundNeeded = FALSE, NextRoundNeeded;
  UINT64  PrevTick, CurrTick, ElapsedCycle;
  UINT64  TimerTicks64;
  UINT8   ReInit;
  INT8    FailedPciePtr[MAX_PCIE_B];
  INT8    FailedPcieCount;

  ReInit = 0;

_link_polling:
  NextRoundNeeded = 0;
  //
  // It is not guaranteed the timer service is ready prior to PCI Dxe.
  // Calculate system ticks for link training.
  //
  TimerTicks64 = ArmGenericTimerGetTimerFreq (); /* 1 Second */
  PrevTick = ArmGenericTimerGetSystemCount ();
  ElapsedCycle = 0;

  do {
    // Update timer
    CurrTick = ArmGenericTimerGetSystemCount ();
    if (CurrTick < PrevTick) {
      ElapsedCycle += (UINT64)(~0x0ULL) - PrevTick;
      PrevTick = 0;
    }
    ElapsedCycle += (CurrTick - PrevTick);
    PrevTick = CurrTick;
  } while (ElapsedCycle < TimerTicks64);

  for (RCIndex = 0; RCIndex < AC01_MAX_PCIE_ROOT_COMPLEX; RCIndex++) {
    Ac01PcieCoreUpdateLink (&RCList[RCIndex], &IsNextRoundNeeded, FailedPciePtr, &FailedPcieCount);
    if (IsNextRoundNeeded) {
      NextRoundNeeded = 1;
    }
  }

  if (NextRoundNeeded && ReInit < MAX_REINIT) {
    // Timer is up. Give another chance to re-program controller
    ReInit++;
    for (RCIndex = 0; RCIndex < AC01_MAX_PCIE_ROOT_COMPLEX; RCIndex++) {
      Ac01PcieCoreUpdateLink (&RCList[RCIndex], &IsNextRoundNeeded, FailedPciePtr, &FailedPcieCount);
      if (IsNextRoundNeeded) {
        for (i = 0; i < FailedPcieCount; i++) {
          PcieIndex = FailedPciePtr[i];
          if (PcieIndex == -1) {
            continue;
          }

          // Some controller still observes link-down. Re-init controller
          Ac01PcieCoreSetupRC (&RCList[RCIndex], 1, PcieIndex);
        }
      }
    }

    goto _link_polling;
  }
}

/**
  Prepare to end PCIE core BSP driver
**/
VOID
Ac01PcieEnd (
  VOID
  )
{
  Ac01PcieCorePollLinkUp ();
}

/**
  Callback funciton for EndEnumeration notification from PCI stack.

  @param[in]  HBIndex               Index to identify of PCIE Host bridge.
  @param[in]  RBIndex               Index to identify of underneath PCIE Root bridge.
  @param[in]  Phase                 The phase of enumeration as informed from PCI stack.
**/
VOID
Ac01PcieHostBridgeNotifyPhase (
  IN UINTN                                         HBIndex,
  IN UINTN                                         RBIndex,
  IN EFI_PCI_HOST_BRIDGE_RESOURCE_ALLOCATION_PHASE Phase
  )
{
  AC01_RC *RC;
  UINTN   RCIndex;

  RCIndex = GetRCIndex (HBIndex, RBIndex);
  RC = &RCList[RCIndex];

  switch (Phase) {
  case EfiPciHostBridgeEndEnumeration:
    Ac01PcieCoreEndEnumeration (RC);
    break;

  case EfiPciHostBridgeBeginEnumeration:
    /* 100ms that help fixing completion timeout issue */
    MicroSecondDelay (100000);
    break;

  case EfiPciHostBridgeBeginBusAllocation:
  case EfiPciHostBridgeEndBusAllocation:
  case EfiPciHostBridgeBeginResourceAllocation:
  case EfiPciHostBridgeAllocateResources:
  case EfiPciHostBridgeSetResources:
  case EfiPciHostBridgeFreeResources:
  case EfiPciHostBridgeEndResourceAllocation:
  case EfiMaxPciHostBridgeEnumerationPhase:
    break;
  }
}
