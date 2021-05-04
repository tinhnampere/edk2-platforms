/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Guid/PlatformInfoHob.h>
#include <Guid/RootComplexInfoHob.h>
#include <Library/ArmGenericTimerCounterLib.h>
#include <Library/BaseLib.h>
#include <Library/BoardPcieLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/PciePhyLib.h>
#include <Library/SystemFirmwareInterfaceLib.h>
#include <Library/TimerLib.h>

#include "PcieCore.h"

/**
  Return the next extended capability address

  @param RootComplex      Pointer to AC01_ROOT_COMPLEX structure
  @param PcieIndex        PCIe index
  @param IsRC             TRUE: Checking RootComplex configuration space
                          FALSE: Checking EP configuration space
  @param ExtendedCapId
**/
UINT64
PcieCheckCap (
  IN AC01_ROOT_COMPLEX *RootComplex,
  IN UINT8             PcieIndex,
  IN BOOLEAN           IsRC,
  IN UINT16            ExtendedCapId
  )
{
  PHYSICAL_ADDRESS       CfgAddr;
  UINT32                 Val = 0, NextCap = 0, CapId = 0, ExCap = 0;

  if (IsRC) {
    CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);
  } else {
    CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 20);
  }

  Val = MmioRead32 (CfgAddr + TYPE1_CAP_PTR_REG);
  NextCap = Val & 0xFF;

  // Loop untill desired capability is found else return 0
  while (1) {
    if ((NextCap & 0x3) != 0) {
      // Not alignment, just return
      return 0;
    }
    Val = MmioRead32 (CfgAddr + NextCap);
    if (NextCap < EXT_CAP_OFFSET_START) {
      CapId = Val & 0xFF;
    } else {
      CapId = Val & 0xFFFF;
    }

    if (CapId == ExtendedCapId) {
      return (UINT64)(CfgAddr + NextCap);
    }

    if (NextCap < EXT_CAP_OFFSET_START) {
      NextCap = (Val & 0xFFFF) >> 8;
    } else {
      NextCap = (Val & 0xFFFF0000) >> 20;
    }

    if ((NextCap == 0) && (ExCap == 0)) {
      ExCap = 1;
      NextCap = EXT_CAP_OFFSET_START;
    }

    if ((NextCap == 0) && (ExCap == 1)) {
      return (UINT64)0;
    }
  }
}

/**
  Configure equalization settings

  @param RootComplex     Pointer to AC01_ROOT_COMPLEX structure
  @param PcieIndex       PCIe index
**/
STATIC
VOID
ConfigureEqualization (
  IN AC01_ROOT_COMPLEX *RootComplex,
  IN UINT8             PcieIndex
  )
{
  PHYSICAL_ADDRESS     CfgAddr;
  UINT32               Val;

  CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

  // Select the FoM method, need double-write to convey settings
  Val = MmioRead32 (CfgAddr + GEN3_EQ_CONTROL_OFF);
  Val = GEN3_EQ_FB_MODE (Val, 0x1);
  Val = GEN3_EQ_PRESET_VEC (Val, 0x3FF);
  Val = GEN3_EQ_INIT_EVAL (Val, 0x1);
  MmioWrite32 (CfgAddr + GEN3_EQ_CONTROL_OFF, Val);
  MmioWrite32 (CfgAddr + GEN3_EQ_CONTROL_OFF, Val);
  Val = MmioRead32 (CfgAddr + GEN3_EQ_CONTROL_OFF);
}

/**
  Configure presets for GEN3 equalization

  @param RootComplex     Pointer to AC01_ROOT_COMPLEX structure
  @param PcieIndex       PCIe index
**/
STATIC
VOID
ConfigurePresetGen3 (
  IN AC01_ROOT_COMPLEX *RootComplex,
  IN UINT8             PcieIndex
  )
{
  PHYSICAL_ADDRESS       CfgAddr, SpcieBaseAddr;
  UINT32                 Val, Idx;

  CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

  // Bring to legacy mode
  Val = MmioRead32 (CfgAddr + GEN3_RELATED_OFF);
  Val = RATE_SHADOW_SEL_SET (Val, 0);
  MmioWrite32 (CfgAddr + GEN3_RELATED_OFF, Val);
  Val = EQ_PHASE_2_3_SET (Val, 0);
  Val = RXEQ_REGRDLESS_SET (Val, 1);
  MmioWrite32 (CfgAddr + GEN3_RELATED_OFF, Val);

  // Generate SPCIE capability address
  SpcieBaseAddr = PcieCheckCap (RootComplex, PcieIndex, TRUE, SPCIE_CAP_ID);
  if (SpcieBaseAddr == 0) {
    DEBUG ((
      DEBUG_ERROR,
      "PCIE%d.%d: Cannot get SPCIE capability address\n",
      RootComplex->ID,
      PcieIndex
      ));
    return;
  }

  for (Idx=0; Idx < RootComplex->Pcie[PcieIndex].MaxWidth/2; Idx++) {
    // Program Preset to Gen3 EQ Lane Control
    Val = MmioRead32 (SpcieBaseAddr + CAP_OFF_0C + Idx*4);
    Val = DSP_TX_PRESET0_SET (Val, 0x7);
    Val = DSP_TX_PRESET1_SET (Val, 0x7);
    MmioWrite32 (SpcieBaseAddr + CAP_OFF_0C + Idx*4, Val);
  }
}

/**
  Configure presets for GEN4 equalization

  @param RootComplex     Pointer to AC01_ROOT_COMPLEX structure
  @param PcieIndex       PCIe controller index
**/
STATIC
VOID
ConfigurePresetGen4 (
  IN AC01_ROOT_COMPLEX *RootComplex,
  IN UINT8             PcieIndex
  )
{
  PHYSICAL_ADDRESS       CfgAddr, SpcieBaseAddr, Pl16BaseAddr;
  UINT32                 LinkWidth, Idx;
  UINT32                 Val;
  UINT8                  Preset;

  CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

  // Bring to legacy mode
  Val = MmioRead32 (CfgAddr + GEN3_RELATED_OFF);
  Val = RATE_SHADOW_SEL_SET (Val, 1);
  MmioWrite32 (CfgAddr + GEN3_RELATED_OFF, Val);
  Val = EQ_PHASE_2_3_SET (Val, 0);
  Val = RXEQ_REGRDLESS_SET (Val, 1);
  MmioWrite32 (CfgAddr + GEN3_RELATED_OFF, Val);

  // Generate the PL16 capability address
  Pl16BaseAddr = PcieCheckCap (RootComplex, PcieIndex, TRUE, PL16_CAP_ID);
  if (Pl16BaseAddr == 0) {
    DEBUG ((
      DEBUG_ERROR,
      "PCIE%d.%d: Cannot get PL16 capability address\n",
      RootComplex->ID,
      PcieIndex
      ));
    return;
  }

  // Generate the SPCIE capability address
  SpcieBaseAddr = PcieCheckCap (RootComplex, PcieIndex, TRUE, SPCIE_CAP_ID);
  if (SpcieBaseAddr == 0) {
    DEBUG ((
      DEBUG_ERROR,
      "PCIE%d.%d: Cannot get SPICE capability address\n",
      RootComplex->ID,
      PcieIndex
      ));
    return;
  }

  // Configure downstream Gen4 Tx preset
  if (RootComplex->PresetGen4[PcieIndex] == PRESET_INVALID) {
    Preset = 0x57; // Default Gen4 preset
  } else {
    Preset = RootComplex->PresetGen4[PcieIndex];
  }

  LinkWidth = RootComplex->Pcie[PcieIndex].MaxWidth;
  if (LinkWidth == 0x2) {
    Val = MmioRead32 (Pl16BaseAddr + PL16G_CAP_OFF_20H_REG_OFF);
    Val = DSP_16G_RXTX_PRESET0_SET (Val, Preset);
    Val = DSP_16G_RXTX_PRESET1_SET (Val, Preset);
    MmioWrite32 (Pl16BaseAddr + PL16G_CAP_OFF_20H_REG_OFF, Val);
  } else {
    for (Idx = 0; Idx < LinkWidth/4; Idx++) {
      Val = MmioRead32 (Pl16BaseAddr + PL16G_CAP_OFF_20H_REG_OFF + Idx*4);
      Val = DSP_16G_RXTX_PRESET0_SET (Val, Preset);
      Val = DSP_16G_RXTX_PRESET1_SET (Val, Preset);
      Val = DSP_16G_RXTX_PRESET2_SET (Val, Preset);
      Val = DSP_16G_RXTX_PRESET3_SET (Val, Preset);
      MmioWrite32 (Pl16BaseAddr + PL16G_CAP_OFF_20H_REG_OFF + Idx*4, Val);
    }
  }

  // Configure Gen3 preset
  for (Idx = 0; Idx < LinkWidth/2; Idx++) {
    Val = MmioRead32 (SpcieBaseAddr + CAP_OFF_0C + Idx*4);
    Val = DSP_TX_PRESET0_SET (Val, 0x7);
    Val = DSP_TX_PRESET1_SET (Val, 0x7);
    MmioWrite32 (SpcieBaseAddr + CAP_OFF_0C + Idx*4, Val);
  }
}

INT8
RasdpMitigation (
  IN AC01_ROOT_COMPLEX *RootComplex,
  IN UINT8             PcieIndex
  )
{
  PHYSICAL_ADDRESS       CfgAddr, DlinkBaseAddr, SnpsRamBase;
  PLATFORM_INFO_HOB      *PlatformHob;
  UINT16                 NextExtendedCapabilityOff;
  UINT32                 Val;
  UINT32                 VsecVal;
  VOID                   *Hob;

  SnpsRamBase = RootComplex->Pcie[PcieIndex].SnpsRamBase;
  CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

  Hob = GetFirstGuidHob (&gPlatformInfoHobGuid);
  PlatformHob = (PLATFORM_INFO_HOB *)GET_GUID_HOB_DATA (Hob);
  if ((PlatformHob->ScuProductId[0] & 0xff) == 0x01
      && AsciiStrCmp ((CONST CHAR8 *)PlatformHob->CpuVer, "A0") == 0
      && (RootComplex->Type == RootComplexTypeB || PcieIndex > 0)) {
    // Change the Read Margin dual ported RAMs
    Val = 0x10; // Margin to 0x0 (most conservative setting)
    MmioWrite32 (SnpsRamBase + TPSRAM_RMR, Val);

    // Generate the DLINK capability address
    DlinkBaseAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);
    NextExtendedCapabilityOff = 0x100; // This is the 1st extended capability offset
    do {
      Val = MmioRead32 (DlinkBaseAddr + NextExtendedCapabilityOff);
      if (Val == 0xFFFFFFFF) {
        DlinkBaseAddr = 0x0;
        break;
      }
      if ((Val & 0xFFFF) == DLINK_VENDOR_CAP_ID) {
        VsecVal = MmioRead32 (DlinkBaseAddr + NextExtendedCapabilityOff + 0x4);
        if (VsecVal == DLINK_VSEC) {
          DlinkBaseAddr = DlinkBaseAddr + NextExtendedCapabilityOff;
          break;
        }
      }
      NextExtendedCapabilityOff = (Val >> 20);
    } while (NextExtendedCapabilityOff != 0);

    // Disable the scaled credit mode
    if (DlinkBaseAddr != 0x0) {
      Val = 1;
      MmioWrite32 (DlinkBaseAddr + DATA_LINK_FEATURE_CAP_OFF, Val);
      Val = MmioRead32 (DlinkBaseAddr + DATA_LINK_FEATURE_CAP_OFF);
      if (Val != 1) {
        DEBUG ((DEBUG_ERROR, "- Pcie[%d] - Unable to disable scaled credit\n", PcieIndex));
        return -1;
      }
    } else {
      DEBUG ((DEBUG_ERROR, "- Pcie[%d] - Unable to locate data link feature cap offset\n", PcieIndex));
      return -1;
    }

    // Reduce Posted Credits to 1 packet header and data credit for all
    // impacted controllers.  Also zero credit scale values for both
    // data and packet headers.
    Val=0x40201020;
    MmioWrite32 (CfgAddr + PORT_LOCIG_VC0_P_RX_Q_CTRL_OFF, Val);
  }

  return 0;
}

VOID
ProgramHostBridge (
  AC01_ROOT_COMPLEX *RootComplex
  )
{
  UINT32 Val;

  // Program Root Complex Bifurcation
  if (RootComplex->Active) {
    if (RootComplex->Type == RootComplexTypeA) {
      if (!EFI_ERROR (MailboxMsgRegisterRead (RootComplex->Socket, RootComplex->HostBridgeBase + HBRCAPDMR, &Val))) {
        Val = RCAPCIDEVMAP_SET (Val, RootComplex->DevMapLow);
        MailboxMsgRegisterWrite (RootComplex->Socket, RootComplex->HostBridgeBase + HBRCAPDMR, Val);
      }
    } else {
      if (!EFI_ERROR (MailboxMsgRegisterRead (RootComplex->Socket, RootComplex->HostBridgeBase + HBRCBPDMR, &Val))) {
        Val = RCBPCIDEVMAPLO_SET (Val, RootComplex->DevMapLow);
        Val = RCBPCIDEVMAPHI_SET (Val, RootComplex->DevMapHigh);
        MailboxMsgRegisterWrite (RootComplex->Socket, RootComplex->HostBridgeBase + HBRCBPDMR, Val);
      }
    }
  }

  // Program VendorID and DeviceId
  if (!EFI_ERROR (MailboxMsgRegisterRead (RootComplex->Socket, RootComplex->HostBridgeBase  + HBPDVIDR, &Val))) {
    Val = PCIVENDID_SET (Val, AMPERE_PCIE_VENDORID);
    if (RootComplexTypeA == RootComplex->Type) {
      Val = PCIDEVID_SET (Val, AC01_HOST_BRIDGE_DEVICEID_RCA);
    } else {
      Val = PCIDEVID_SET (Val, AC01_HOST_BRIDGE_DEVICEID_RCB);
    }
    MailboxMsgRegisterWrite (RootComplex->Socket, RootComplex->HostBridgeBase  + HBPDVIDR, Val);
  }
}

VOID
ProgramLinkCapabilities (
  IN AC01_ROOT_COMPLEX *RootComplex,
  IN UINT8             PcieIndex
  )
{
  PHYSICAL_ADDRESS       CfgAddr;
  UINT32                 Val;

  CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

  Val = MmioRead32 (CfgAddr + PORT_LINK_CTRL_OFF);
  switch (RootComplex->Pcie[PcieIndex].MaxWidth) {
  case LINK_WIDTH_X2:
    Val = LINK_CAPABLE_SET (Val, LINK_CAPABLE_X2);
    break;

  case LINK_WIDTH_X4:
    Val = LINK_CAPABLE_SET (Val, LINK_CAPABLE_X4);
    break;

  case LINK_WIDTH_X8:
    Val = LINK_CAPABLE_SET (Val, LINK_CAPABLE_X8);
    break;

  case LINK_WIDTH_X16:
  default:
    Val = LINK_CAPABLE_SET (Val, LINK_CAPABLE_X16);
    break;
  }
  MmioWrite32 (CfgAddr + PORT_LINK_CTRL_OFF, Val);
  Val = MmioRead32 (CfgAddr + GEN2_CTRL_OFF);
  switch (RootComplex->Pcie[PcieIndex].MaxWidth) {
  case LINK_WIDTH_X2:
    Val = NUM_OF_LANES_SET (Val, NUM_OF_LANES_X2);
    break;

  case LINK_WIDTH_X4:
    Val = NUM_OF_LANES_SET (Val, NUM_OF_LANES_X4);
    break;

  case LINK_WIDTH_X8:
    Val = NUM_OF_LANES_SET (Val, NUM_OF_LANES_X8);
    break;

  case LINK_WIDTH_X16:
  default:
    Val = NUM_OF_LANES_SET (Val, NUM_OF_LANES_X16);
    break;
  }
  MmioWrite32 (CfgAddr + GEN2_CTRL_OFF, Val);
  Val = MmioRead32 (CfgAddr + LINK_CAPABILITIES_REG);
  switch (RootComplex->Pcie[PcieIndex].MaxWidth) {
  case LINK_WIDTH_X2:
    Val = CAP_MAX_LINK_WIDTH_SET (Val, CAP_MAX_LINK_WIDTH_X2);
    break;

  case LINK_WIDTH_X4:
    Val = CAP_MAX_LINK_WIDTH_SET (Val, CAP_MAX_LINK_WIDTH_X4);
    break;

  case LINK_WIDTH_X8:
    Val = CAP_MAX_LINK_WIDTH_SET (Val, CAP_MAX_LINK_WIDTH_X8);
    break;

  case LINK_WIDTH_X16:
  default:
    Val = CAP_MAX_LINK_WIDTH_SET (Val, CAP_MAX_LINK_WIDTH_X16);
    break;
  }

  switch (RootComplex->Pcie[PcieIndex].MaxGen) {
  case LINK_SPEED_GEN1:
    Val = CAP_MAX_LINK_SPEED_SET (Val, MAX_LINK_SPEED_25);
    break;

  case LINK_SPEED_GEN2:
    Val = CAP_MAX_LINK_SPEED_SET (Val, MAX_LINK_SPEED_50);
    break;

  case LINK_SPEED_GEN3:
    Val = CAP_MAX_LINK_SPEED_SET (Val, MAX_LINK_SPEED_80);
    break;

  default:
    Val = CAP_MAX_LINK_SPEED_SET (Val, MAX_LINK_SPEED_160);
    break;
  }
  /* Enable ASPM Capability */
  Val = CAP_ACTIVE_STATE_LINK_PM_SUPPORT_SET (Val, L0S_L1_SUPPORTED);
  MmioWrite32 (CfgAddr + LINK_CAPABILITIES_REG, Val);

  Val = MmioRead32 (CfgAddr + LINK_CONTROL2_LINK_STATUS2_REG);
  switch (RootComplex->Pcie[PcieIndex].MaxGen) {
  case LINK_SPEED_GEN1:
    Val = CAP_TARGET_LINK_SPEED_SET (Val, MAX_LINK_SPEED_25);
    break;

  case LINK_SPEED_GEN2:
    Val = CAP_TARGET_LINK_SPEED_SET (Val, MAX_LINK_SPEED_50);
    break;

  case LINK_SPEED_GEN3:
    Val = CAP_TARGET_LINK_SPEED_SET (Val, MAX_LINK_SPEED_80);
    break;

  default:
    Val = CAP_TARGET_LINK_SPEED_SET (Val, MAX_LINK_SPEED_160);
    break;
  }
  MmioWrite32 (CfgAddr + LINK_CONTROL2_LINK_STATUS2_REG, Val);
}

VOID
MaskCompletionTimeOut (
  IN AC01_ROOT_COMPLEX  *RootComplex,
  IN UINT8              PcieIndex,
  IN UINT32             TimeOut,
  IN BOOLEAN            IsMask
  )
{
  PHYSICAL_ADDRESS       CfgAddr;
  UINT32                 Val;

  CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

  Val = MmioRead32 (CfgAddr + AMBA_LINK_TIMEOUT_OFF);
  Val = LINK_TIMEOUT_PERIOD_DEFAULT_SET (Val, TimeOut);
  MmioWrite32 (CfgAddr + AMBA_LINK_TIMEOUT_OFF, Val);
  Val = MmioRead32 (CfgAddr + UNCORR_ERR_MASK_OFF);
  Val = CMPLT_TIMEOUT_ERR_MASK_SET (Val, IsMask ? 1 : 0);
  MmioWrite32 (CfgAddr + UNCORR_ERR_MASK_OFF, Val);
}

BOOLEAN
PollMemReady (
  PHYSICAL_ADDRESS       CsrBase
  )
{
  UINT32                 TimeOut;
  UINT32                 Val;

  // Poll till mem is ready
  TimeOut = MEMRDY_TIMEOUT;
  do {
    Val = MmioRead32 (CsrBase + MEMRDYR);
    if (Val & 1) {
      break;
    }

    TimeOut--;
    MicroSecondDelay (1);
  } while (TimeOut > 0);

  if (TimeOut <= 0) {
    return FALSE;
  }

  return TRUE;
}

BOOLEAN
PollPipeReady (
  PHYSICAL_ADDRESS       CsrBase
  )
{
  UINT32                 TimeOut;
  UINT32                 Val;

  // Poll till PIPE clock is stable
  TimeOut = PIPE_CLOCK_TIMEOUT;
  do {
    Val = MmioRead32 (CsrBase + LINKSTAT);
    if (!(Val & PHY_STATUS_MASK)) {
      break;
    }

    TimeOut--;
    MicroSecondDelay (1);
  } while (TimeOut > 0);

  if (TimeOut <= 0) {
    return FALSE;
  }

  return TRUE;
}

/**
  Setup and initialize the AC01 PCIe Root Complex and underneath PCIe controllers

  @param RootComplex           Pointer to Root Complex structure
  @param ReInit                Re-init status
  @param ReInitPcieIndex       PCIe index

  @retval RETURN_SUCCESS       The Root Complex has been initialized successfully.
  @retval RETURN_DEVICE_ERROR  PHY, Memory or PIPE is not ready.
**/
RETURN_STATUS
Ac01PcieCoreSetupRC (
  IN AC01_ROOT_COMPLEX *RootComplex,
  IN BOOLEAN           ReInit,
  IN UINT8             ReInitPcieIndex
  )
{
  PHYSICAL_ADDRESS     CsrBase, CfgAddr;
  RETURN_STATUS        Status;
  UINT32               Val;
  UINT8                PcieIndex;

  DEBUG ((DEBUG_INFO, "Initializing Socket%d RootComplex%d\n", RootComplex->Socket, RootComplex->ID));

  ProgramHostBridge (RootComplex);

  if (!ReInit) {
    // Initialize PHY
    Status = PciePhyInit (RootComplex->SerdesBase);
    if (RETURN_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to initialize the PCIe PHY\n", __FUNCTION__));
      return RETURN_DEVICE_ERROR;
    }
  }

  // Setup each controller
  for (PcieIndex = 0; PcieIndex < RootComplex->MaxPcieController; PcieIndex++) {

    if (ReInit) {
      PcieIndex = ReInitPcieIndex;
    }

    if (!RootComplex->Pcie[PcieIndex].Active) {
      continue;
    }

    DEBUG ((DEBUG_INFO, "Initializing Controller %d\n", PcieIndex));

    CsrBase = RootComplex->Pcie[PcieIndex].CsrBase;
    CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

    // Put Controller into reset if not in reset already
    Val = MmioRead32 (CsrBase +  RESET);
    if (!(Val & RESET_MASK)) {
      Val = DWCPCIE_SET (Val, 1);
      MmioWrite32 (CsrBase + RESET, Val);

      // Delay 50ms to ensure controller finish its reset
      MicroSecondDelay (50000);
    }

    // Clear memory shutdown
    Val = MmioRead32 (CsrBase + RAMSDR);
    Val = SD_SET (Val, 0);
    MmioWrite32 (CsrBase + RAMSDR, Val);

    if (!PollMemReady (CsrBase)) {
      DEBUG ((DEBUG_ERROR, "- Pcie[%d] - Mem not ready\n", PcieIndex));
      return RETURN_DEVICE_ERROR;
    }

    // Hold link training
    Val = MmioRead32 (CsrBase + LINKCTRL);
    Val = LTSSMENB_SET (Val, 0);
    MmioWrite32 (CsrBase + LINKCTRL, Val);

    // Enable subsystem clock and release reset
    Val = MmioRead32 (CsrBase + CLOCK);
    Val = AXIPIPE_SET (Val, 1);
    MmioWrite32 (CsrBase + CLOCK, Val);
    Val = MmioRead32 (CsrBase +  RESET);
    Val = DWCPCIE_SET (Val, 0);
    MmioWrite32 (CsrBase + RESET, Val);

    //
    // Controller does not provide any indicator for reset released.
    // Must wait at least 1us as per EAS.
    //
    MicroSecondDelay (1);

    if (!PollPipeReady (CsrBase)) {
      DEBUG ((DEBUG_ERROR, "- Pcie[%d] - PIPE clock is not stable\n", PcieIndex));
      return RETURN_DEVICE_ERROR;
    }

    // Start PERST pulse
    BoardPcieAssertPerst (RootComplex, PcieIndex, TRUE);

    // Allow programming to config space
    Val = MmioRead32 (CfgAddr + MISC_CONTROL_1_OFF);
    Val = DBI_RO_WR_EN_SET (Val, 1);
    MmioWrite32 (CfgAddr + MISC_CONTROL_1_OFF, Val);

    // In order to detect the NVMe disk for booting without disk,
    // need to set Hot-Plug Slot Capable during port initialization.
    // It will help PCI Linux driver to initialize its slot iomem resource,
    // the one is used for detecting the disk when it is inserted.
    Val = MmioRead32 (CfgAddr + SLOT_CAPABILITIES_REG);
    Val = SLOT_HPC_SET (Val, 1);
    // Program the power limit
    Val = SLOT_CAP_SLOT_POWER_LIMIT_VALUE_SET (Val, SLOT_POWER_LIMIT);
    MmioWrite32 (CfgAddr + SLOT_CAPABILITIES_REG, Val);

    // Apply RASDP error mitigation for all x8, x4, and x2 controllers
    // This includes all RootComplexTypeB root ports, and every RootComplexTypeA root port controller
    // except for index 0 (i.e. x16 controllers are exempted from this WA)
    RasdpMitigation (RootComplex, PcieIndex);

    // Program DTI for ATS support
    Val = MmioRead32 (CfgAddr + DTIM_CTRL0_OFF);
    Val = DTIM_CTRL0_ROOT_PORT_ID_SET (Val, 0);
    MmioWrite32 (CfgAddr + DTIM_CTRL0_OFF, Val);

    //
    // Program number of lanes used
    // - Reprogram LINK_CAPABLE of PORT_LINK_CTRL_OFF
    // - Reprogram NUM_OF_LANES of GEN2_CTRL_OFF
    // - Reprogram CAP_MAX_LINK_WIDTH of LINK_CAPABILITIES_REG
    //
    ProgramLinkCapabilities (RootComplex, PcieIndex);

    // Set Zero byte request handling
    Val = MmioRead32 (CfgAddr + FILTER_MASK_2_OFF);
    Val = CX_FLT_MASK_VENMSG0_DROP_SET (Val, 0);
    Val = CX_FLT_MASK_VENMSG1_DROP_SET (Val, 0);
    Val = CX_FLT_MASK_DABORT_4UCPL_SET (Val, 0);
    MmioWrite32 (CfgAddr + FILTER_MASK_2_OFF, Val);
    Val = MmioRead32 (CfgAddr + AMBA_ORDERING_CTRL_OFF);
    Val = AX_MSTR_ZEROLREAD_FW_SET (Val, 0);
    MmioWrite32 (CfgAddr + AMBA_ORDERING_CTRL_OFF, Val);

    //
    // Set Completion with CRS handling for CFG Request
    // Set Completion with CA/UR handling non-CFG Request
    //
    Val = MmioRead32 (CfgAddr + AMBA_ERROR_RESPONSE_DEFAULT_OFF);
    Val = AMBA_ERROR_RESPONSE_CRS_SET (Val, 0x2);
    MmioWrite32 (CfgAddr + AMBA_ERROR_RESPONSE_DEFAULT_OFF, Val);

    // Set Legacy PCIE interrupt map to INTA
    Val = MmioRead32 (CfgAddr + BRIDGE_CTRL_INT_PIN_INT_LINE_REG);
    Val = INT_PIN_SET (Val, 1);
    MmioWrite32 (CfgAddr + BRIDGE_CTRL_INT_PIN_INT_LINE_REG, Val);
    Val = MmioRead32 (CsrBase + IRQSEL);
    Val = INTPIN_SET (Val, 1);
    MmioWrite32 (CsrBase + IRQSEL, Val);

    if (RootComplex->Pcie[PcieIndex].MaxGen != LINK_SPEED_GEN1) {
      ConfigureEqualization (RootComplex, PcieIndex);
      if (RootComplex->Pcie[PcieIndex].MaxGen == LINK_SPEED_GEN3) {
        ConfigurePresetGen3 (RootComplex, PcieIndex);
      } else if (RootComplex->Pcie[PcieIndex].MaxGen == LINK_SPEED_GEN4) {
        ConfigurePresetGen4 (RootComplex, PcieIndex);
      }
    }

    // Mask Completion Timeout
    MaskCompletionTimeOut (RootComplex, PcieIndex, 1, TRUE);

    // AER surprise link down error should be masked due to hotplug is enabled
    // This event must be handled by Hotplug handler, instead of error handler
    Val = MmioRead32 (CfgAddr + UNCORR_ERR_MASK_OFF);
    Val = SDES_ERR_MASK_SET (Val, 1);
    MmioWrite32 (CfgAddr + UNCORR_ERR_MASK_OFF, Val);

    // Program Class Code
    Val = MmioRead32 (CfgAddr + TYPE1_CLASS_CODE_REV_ID_REG);
    Val = REVISION_ID_SET (Val, 4);
    Val = SUBCLASS_CODE_SET (Val, 4);
    Val = BASE_CLASS_CODE_SET (Val, 6);
    MmioWrite32 (CfgAddr + TYPE1_CLASS_CODE_REV_ID_REG, Val);

    // Program VendorID and DeviceID
    Val = MmioRead32 (CfgAddr + TYPE1_DEV_ID_VEND_ID_REG);
    Val = VENDOR_ID_SET (Val, AMPERE_PCIE_VENDORID);
    if (RootComplexTypeA == RootComplex->Type) {
      Val = DEVICE_ID_SET (Val, AC01_PCIE_BRIDGE_DEVICEID_RCA + PcieIndex);
    } else {
      Val = DEVICE_ID_SET (Val, AC01_PCIE_BRIDGE_DEVICEID_RCB + PcieIndex);
    }
    MmioWrite32 (CfgAddr + TYPE1_DEV_ID_VEND_ID_REG, Val);

    // Enable common clock for downstream
    Val = MmioRead32 (CfgAddr + LINK_CONTROL_LINK_STATUS_REG);
    Val = CAP_SLOT_CLK_CONFIG_SET (Val, 1);
    Val = CAP_COMMON_CLK_SET (Val, 1);
    MmioWrite32 (CfgAddr + LINK_CONTROL_LINK_STATUS_REG, Val);

    // Match aux_clk to system
    Val = MmioRead32 (CfgAddr + AUX_CLK_FREQ_OFF);
    Val = AUX_CLK_FREQ_SET (Val, AUX_CLK_500MHZ);
    MmioWrite32 (CfgAddr + AUX_CLK_FREQ_OFF, Val);

    // Assert PERST low to reset endpoint
    BoardPcieAssertPerst (RootComplex, PcieIndex, FALSE);

    // Start link training
    Val = MmioRead32 (CsrBase + LINKCTRL);
    Val = LTSSMENB_SET (Val, 1);
    MmioWrite32 (CsrBase + LINKCTRL, Val);

    // Complete the PERST pulse
    BoardPcieAssertPerst (RootComplex, PcieIndex, TRUE);

    // Lock programming of config space
    Val = MmioRead32 (CfgAddr + MISC_CONTROL_1_OFF);
    Val = DBI_RO_WR_EN_SET (Val, 0);
    MmioWrite32 (CfgAddr + MISC_CONTROL_1_OFF, Val);

    if (ReInit) {
      return RETURN_SUCCESS;
    }
  }

  return RETURN_SUCCESS;
}

BOOLEAN
PcieLinkUpCheck (
  IN AC01_PCIE_CONTROLLER *Pcie
  )
{
  PHYSICAL_ADDRESS       CsrBase;
  UINT32                 BlockEvent;
  UINT32                 LinkStat;

  CsrBase = Pcie->CsrBase;

  // Check if card present
  // smlh_ltssm_state[13:8] = 0
  // phy_status[2] = 0
  // smlh_link_up[1] = 0
  // rdlh_link_up[0] = 0
  LinkStat = MmioRead32 (CsrBase + LINKSTAT);
  LinkStat = LinkStat & (SMLH_LTSSM_STATE_MASK | PHY_STATUS_MASK_BIT |
                         SMLH_LINK_UP_MASK_BIT | RDLH_LINK_UP_MASK_BIT);
  if (LinkStat == 0x0000) {
    return FALSE;
  }

  BlockEvent = MmioRead32 (CsrBase +  BLOCKEVENTSTAT);
  LinkStat = MmioRead32 (CsrBase +  LINKSTAT);

  if (((BlockEvent & LINKUP_MASK) != 0)
      && (SMLH_LTSSM_STATE_GET(LinkStat) == LTSSM_STATE_L0))
  {
    DEBUG ((DEBUG_INFO, "%a Linkup\n", __FUNCTION__));
    return TRUE;
  }

  return FALSE;
}

/**
  Callback function when the Host Bridge enumeration end.

  @param RootComplex          Pointer to the Root Complex structure
**/
VOID
Ac01PcieCoreEndEnumeration (
  IN AC01_ROOT_COMPLEX *RootComplex
  )
{
  PHYSICAL_ADDRESS                CfgAddr;
  PHYSICAL_ADDRESS                Reg;
  UINT32                          Val;
  UINTN                           Index;

  if (RootComplex == NULL || !RootComplex->Active) {
    return;
  }

  // Clear uncorrectable error during enumuration phase. Mainly completion timeout.
  for (Index = 0; Index < RootComplex->MaxPcieController; Index++) {
    if (!RootComplex->Pcie[Index].Active) {
      continue;
    }
    if (!PcieLinkUpCheck(&RootComplex->Pcie[Index])) {
      // If link down/disabled after enumeration, disable completed time out
      CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[Index].DevNum << 15);
      Val = MmioRead32 (CfgAddr + UNCORR_ERR_MASK_OFF);
      Val = CMPLT_TIMEOUT_ERR_MASK_SET (Val, 1);
      MmioWrite32 (CfgAddr + UNCORR_ERR_MASK_OFF, Val);
    }
    // Clear all errors
    Reg = RootComplex->MmcfgBase + ((Index + 1) << 15) + UNCORR_ERR_STATUS_OFF;
    Val = MmioRead32 (Reg);
    if (Val != 0) {
      // Clear error by writting
      MmioWrite32 (Reg, Val);
    }
  }
}

/**
  Comparing current link status with the max capabilities of the link

  @param RootComplex   Pointer to AC01_ROOT_COMPLEX structure
  @param PcieIndex     PCIe index
  @param EpMaxWidth    EP max link width
  @param EpMaxGen      EP max link speed

  @retval              -1: Link status do not match with link max capabilities
                        1: Link capabilites are invalid
                        0: Link status are correct
**/
INT32
Ac01PcieCoreLinkCheck (
  IN AC01_ROOT_COMPLEX  *RootComplex,
  IN UINT8              PcieIndex,
  IN UINT8              EpMaxWidth,
  IN UINT8              EpMaxGen
  )
{
  PHYSICAL_ADDRESS      CsrBase, CfgAddr;
  UINT32                Val, LinkStat;
  UINT32                MaxWidth, MaxGen;

  CsrBase = RootComplex->Pcie[PcieIndex].CsrBase;
  CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

  Val = MmioRead32 (CfgAddr + LINK_CAPABILITIES_REG);
  if ((CAP_MAX_LINK_WIDTH_GET (Val) == 0) ||
      (CAP_MAX_LINK_SPEED_GET (Val) == 0)) {
    DEBUG ((DEBUG_INFO, "\tPCIE%d.%d: Wrong RootComplex capabilities\n", RootComplex->ID, PcieIndex));
    return LINK_CHECK_WRONG_PARAMETER;
  }

  if ((EpMaxWidth == 0) || (EpMaxGen == 0)) {
    DEBUG ((DEBUG_INFO, "\tPCIE%d.%d: Wrong EP capabilities\n", RootComplex->ID, PcieIndex));
    return LINK_CHECK_FAILED;
  }

  // Compare RootComplex and EP capabilities
  if (CAP_MAX_LINK_WIDTH_GET (Val) > EpMaxWidth) {
    MaxWidth = EpMaxWidth;
  } else {
    MaxWidth = CAP_MAX_LINK_WIDTH_GET (Val);
  }

  // Compare RootComplex and EP capabilities
  if (CAP_MAX_LINK_SPEED_GET (Val) > EpMaxGen) {
    MaxGen = EpMaxGen;
  } else {
    MaxGen = CAP_MAX_LINK_SPEED_GET (Val);
  }

  LinkStat = MmioRead32 (CsrBase + LINKSTAT);
  Val = MmioRead32 (CfgAddr + LINK_CONTROL_LINK_STATUS_REG);
  DEBUG ((
    DEBUG_INFO,
    "PCIE%d.%d: Link MaxWidth %d MaxGen %d, LINKSTAT 0x%x",
    RootComplex->ID,
    PcieIndex,
    MaxWidth,
    MaxGen,
    LinkStat
    ));

  // Checking all conditions of the link
  // If one of them is not sastified, return link up fail
  if ((CAP_NEGO_LINK_WIDTH_GET (Val) != MaxWidth) ||
      (CAP_LINK_SPEED_GET (Val) != MaxGen) ||
      (RDLH_SMLH_LINKUP_STATUS_GET (LinkStat) != (SMLH_LINK_UP_MASK_BIT | RDLH_LINK_UP_MASK_BIT)))
  {
    DEBUG ((DEBUG_INFO, "\tLinkCheck FAILED\n"));
    return LINK_CHECK_FAILED;
  }

  DEBUG ((DEBUG_INFO, "\tLinkCheck SUCCESS\n"));
  return LINK_CHECK_SUCCESS;
}

INT32
PFACounterRead (
  IN AC01_ROOT_COMPLEX   *RootComplex,
  IN UINT8               PcieIndex,
  IN UINT64              RasDesVSecBase
  )
{
  INT32                  Ret = LINK_CHECK_SUCCESS;
  UINT32                 Val;
  UINT8                  ErrCode, ErrGrpNum;

  UINT32 ErrCtrlCfg[MAX_NUM_ERROR_CODE] = {
    0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, 0x009, 0x00A, // Per Lane
    0x105, 0x106, 0x107, 0x108, 0x109, 0x10A,
    0x200, 0x201, 0x202, 0x203, 0x204, 0x205, 0x206, 0x207,
    0x300, 0x301, 0x302, 0x303, 0x304, 0x305,
    0x400, 0x401,                                                                // Per Lane
    0x500, 0x501, 0x502, 0x503, 0x504, 0x505, 0x506, 0x507, 0x508, 0x509, 0x50A, 0x50B, 0x50C, 0x50D
  };

  for (ErrCode = 0; ErrCode < MAX_NUM_ERROR_CODE; ErrCode++) {
    ErrGrpNum = (ErrCtrlCfg[ErrCode] & 0xF00) >> 8;
    // Skipping per lane group
    // Checking common lane group because AER error are included in common group only
    if ((ErrGrpNum != 0) && (ErrGrpNum != 4)) {
      Val = MmioRead32 (RasDesVSecBase + EVENT_COUNTER_CONTROL_REG_OFF);
      if (RootComplex->Type == RootComplexTypeA) {
        // RootComplexTypeA - 4 PCIe controller per port, 1 controller in charge of 4 lanes
        Val = ECCR_LANE_SEL_SET (Val, PcieIndex*4);
      } else {
        // RootComplexTypeB - 8 PCIe controller per port, 1 controller in charge of 2 lanes
        Val = ECCR_LANE_SEL_SET (Val, PcieIndex*2);
      }
      Val = ECCR_GROUP_EVENT_SEL_SET (Val, ErrCtrlCfg[ErrCode]);
      MmioWrite32 (RasDesVSecBase + EVENT_COUNTER_CONTROL_REG_OFF, Val);

      // After setting Counter Control reg
      // This delay just to make sure Counter Data reg is update with new value
      MicroSecondDelay (1);
      Val = MmioRead32 (RasDesVSecBase + EVENT_COUNTER_DATA_REG_OFF);
      if (Val != 0) {
        Ret = LINK_CHECK_FAILED;
        DEBUG ((
          DEBUG_ERROR,
          "\tSocket%d RootComplex%d RP%d \t%s: %d \tGROUP:%d-EVENT:%d\n",
          RootComplex->Socket,
          RootComplex->ID,
          PcieIndex,
          Val,
          ((ErrCtrlCfg[ErrCode] & 0xF00) >> 8),  // Group
          (ErrCtrlCfg[ErrCode] & 0x0FF)          // Event
          ));
      }
    }
  }

  return Ret;
}

INT32
Ac01PFAEnableAll (
  IN AC01_ROOT_COMPLEX   *RootComplex,
  IN UINT8               PcieIndex,
  IN INTN                PFAMode
  )
{
  PHYSICAL_ADDRESS       CfgAddr;
  PHYSICAL_ADDRESS       RasDesVSecBase;
  INT32                  Ret = LINK_CHECK_SUCCESS;
  UINT32                 Val;

  CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

  // Allow programming to config space
  Val = MmioRead32 (CfgAddr + MISC_CONTROL_1_OFF);
  Val = DBI_RO_WR_EN_SET (Val, 1);
  MmioWrite32 (CfgAddr + MISC_CONTROL_1_OFF, Val);

  // Generate the RAS DES capability address
  // RAS_DES_CAP_ID = 0xB
  RasDesVSecBase = PcieCheckCap (RootComplex, PcieIndex, TRUE, RAS_DES_CAP_ID);
  if (RasDesVSecBase == 0) {
    DEBUG ((DEBUG_INFO, "PCIE%d.%d: Cannot get RAS DES capability address\n", RootComplex->ID, PcieIndex));
    return LINK_CHECK_WRONG_PARAMETER;
  }

  if (PFAMode == PFA_REG_ENABLE) {
    // PFA Counters Enable
    Val = MmioRead32 (RasDesVSecBase + EVENT_COUNTER_CONTROL_REG_OFF);
    Val = ECCR_EVENT_COUNTER_ENABLE_SET (Val, 0x7);
    Val = ECCR_EVENT_COUNTER_CLEAR_SET (Val, 0);
    MmioWrite32 (RasDesVSecBase + EVENT_COUNTER_CONTROL_REG_OFF, Val);
  } else if (PFAMode == PFA_REG_CLEAR) {
    // PFA Counters Clear
    Val = MmioRead32 (RasDesVSecBase + EVENT_COUNTER_CONTROL_REG_OFF);
    Val = ECCR_EVENT_COUNTER_ENABLE_SET (Val, 0);
    Val = ECCR_EVENT_COUNTER_CLEAR_SET (Val, 0x3);
    MmioWrite32 (RasDesVSecBase + EVENT_COUNTER_CONTROL_REG_OFF, Val);
  } else {
    // PFA Counters Read
    Ret = PFACounterRead (RootComplex, PcieIndex, RasDesVSecBase);
  }

  // Disable programming to config space
  Val = MmioRead32 (CfgAddr + MISC_CONTROL_1_OFF);
  Val = DBI_RO_WR_EN_SET (Val, 0);
  MmioWrite32 (CfgAddr + MISC_CONTROL_1_OFF, Val);

  return Ret;
}

UINT32
CheckEndpointCfg (
  IN AC01_ROOT_COMPLEX  *RootComplex,
  IN UINT8              PcieIndex
  )
{
  PHYSICAL_ADDRESS      EpCfgAddr;
  UINT32                TimeOut;
  UINT32                Val;

  EpCfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 20); /* Bus 1, dev 0, func 0 */

  // Loop read EpCfgAddr value until got valid value or
  // reach to timeout EP_LINKUP_TIMEOUT (or more depend on card)
  TimeOut = EP_LINKUP_TIMEOUT;
  do {
    Val = MmioRead32 (EpCfgAddr);
    if (Val != 0xFFFF0001 && Val != 0xFFFFFFFF) {
      break;
    }
    MicroSecondDelay (LINK_WAIT_INTERVAL_US);
    TimeOut -= LINK_WAIT_INTERVAL_US;
  } while (TimeOut > 0);

  return MmioRead32 (EpCfgAddr);
}

VOID
GetEndpointInfo (
  IN  AC01_ROOT_COMPLEX  *RootComplex,
  IN  UINT8              PcieIndex,
  OUT UINT8              *EpMaxWidth,
  OUT UINT8              *EpMaxGen
  )
{
  PHYSICAL_ADDRESS      PcieCapBaseAddr;
  UINT32                Val;

  Val = CheckEndpointCfg (RootComplex, PcieIndex);
  if (Val == 0xFFFFFFFF) {
    *EpMaxWidth = 0;   // Invalid Width
    *EpMaxGen = 0;     // Invalid Speed
    DEBUG ((DEBUG_ERROR, "PCIE%d.%d Cannot access EP config space!\n", RootComplex->ID, PcieIndex));
  } else {
    PcieCapBaseAddr = PcieCheckCap (RootComplex, PcieIndex, FALSE, CAP_ID);
    if (PcieCapBaseAddr == 0) {
      *EpMaxWidth = 0;   // Invalid Width
      *EpMaxGen = 0;     // Invalid Speed
      DEBUG ((
        DEBUG_ERROR,
        "PCIE%d.%d Cannot get PCIe capability extended address!\n",
        RootComplex->ID,
        PcieIndex
        ));
    } else {
      Val = MmioRead32 (PcieCapBaseAddr + LINK_CAPABILITIES_REG_OFF);
      *EpMaxWidth = (Val >> 4) & 0x3F;
      *EpMaxGen = Val & 0xF;
      DEBUG ((
        DEBUG_INFO,
        "PCIE%d.%d EP MaxWidth %d EP MaxGen %d \n", RootComplex->ID,
        PcieIndex,
        *EpMaxWidth,
        *EpMaxGen
        ));

      // From EP, enabling common clock for upstream
      Val = MmioRead32 (PcieCapBaseAddr + LINK_CONTROL_LINK_STATUS_OFF);
      Val = CAP_SLOT_CLK_CONFIG_SET (Val, 1);
      Val = CAP_COMMON_CLK_SET (Val, 1);
      MmioWrite32 (PcieCapBaseAddr + LINK_CONTROL_LINK_STATUS_OFF, Val);
    }
  }
}

/**
   Get link capabilities link width and speed of endpoint

   @param RootComplex[in]  Pointer to AC01_ROOT_COMPLEX structure
   @param PcieIndex[in]    PCIe controller index
   @param EpMaxWidth[out]  EP max link width
   @param EpMaxGen[out]    EP max link speed
**/
VOID
Ac01PcieCoreGetEndpointInfo (
  IN  AC01_ROOT_COMPLEX  *RootComplex,
  IN  UINT8              PcieIndex,
  OUT UINT8              *EpMaxWidth,
  OUT UINT8              *EpMaxGen
  )
{
  PHYSICAL_ADDRESS      RcCfgAddr;
  UINT32                Val, RestoreVal;

  RcCfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

  // Allow programming to config space
  Val = MmioRead32 (RcCfgAddr + MISC_CONTROL_1_OFF);
  Val = DBI_RO_WR_EN_SET (Val, 1);
  MmioWrite32 (RcCfgAddr + MISC_CONTROL_1_OFF, Val);

  Val = MmioRead32 (RcCfgAddr + SEC_LAT_TIMER_SUB_BUS_SEC_BUS_PRI_BUS_REG);
  RestoreVal = Val;
  Val = SUB_BUS_SET (Val, DEFAULT_SUB_BUS);             /* Set DEFAULT_SUB_BUS to Subordinate bus */
  Val = SEC_BUS_SET (Val, RootComplex->Pcie[PcieIndex].DevNum);  /* Set RootComplex->Pcie[PcieIndex].DevNum to Secondary bus */
  Val = PRIM_BUS_SET (Val, 0x0);                        /* Set 0x0 to Primary bus */
  MmioWrite32 (RcCfgAddr + SEC_LAT_TIMER_SUB_BUS_SEC_BUS_PRI_BUS_REG, Val);

  GetEndpointInfo (RootComplex, PcieIndex, EpMaxWidth, EpMaxGen);

  // Restore value in order to not affect enumeration process
  MmioWrite32 (RcCfgAddr + SEC_LAT_TIMER_SUB_BUS_SEC_BUS_PRI_BUS_REG, RestoreVal);

  // Disable programming to config space
  Val = MmioRead32 (RcCfgAddr + MISC_CONTROL_1_OFF);
  Val = DBI_RO_WR_EN_SET (Val, 0);
  MmioWrite32 (RcCfgAddr + MISC_CONTROL_1_OFF, Val);
}

VOID
PollLinkUp (
  IN AC01_ROOT_COMPLEX   *RootComplex,
  IN UINT8               PcieIndex
  )
{
  UINT32        TimeOut;

  // Poll until link up
  // This checking for linkup status and
  // give LTSSM state the time to transit from DECTECT state to L0 state
  // Total delay are 100ms, smaller number of delay cannot always make sure
  // the state transition is completed
  TimeOut = LTSSM_TRANSITION_TIMEOUT;
  do {
    if (PcieLinkUpCheck (&RootComplex->Pcie[PcieIndex])) {
      DEBUG ((
        DEBUG_INFO,
        "\tPCIE%d.%d LinkStat is correct after soft reset, transition time: %d\n",
        RootComplex->ID,
        PcieIndex,
        TimeOut
        ));
      RootComplex->Pcie[PcieIndex].LinkUp = TRUE;
      break;
    }

    MicroSecondDelay (100);
    TimeOut -= 100;
  } while (TimeOut > 0);

  if (TimeOut <= 0) {
    DEBUG ((DEBUG_ERROR, "\tPCIE%d.%d LinkStat TIMEOUT after re-init\n", RootComplex->ID, PcieIndex));
  } else {
    DEBUG ((DEBUG_INFO, "PCIE%d.%d Link re-initialization passed!\n", RootComplex->ID, PcieIndex));
  }
}

/**
  Check active PCIe controllers of RootComplex, retrain or soft reset if needed

  @param RootComplex[in]  Pointer to AC01_ROOT_COMPLEX structure
  @param PcieIndex[in]    PCIe controller index

  @retval                 -1: Link recovery had failed
                          1: Link width and speed are not correct
                          0: Link recovery succeed
**/
INT32
Ac01PcieCoreQoSLinkCheckRecovery (
  IN AC01_ROOT_COMPLEX   *RootComplex,
  IN UINT8               PcieIndex
  )
{
  INT32       NumberOfReset = MAX_REINIT; // Number of soft reset retry
  UINT8       EpMaxWidth, EpMaxGen;
  INT32       LinkStatusCheck, RasdesChecking;

  // PCIe controller is not active or Link is not up
  // Nothing to be done
  if ((!RootComplex->Pcie[PcieIndex].Active) || (!RootComplex->Pcie[PcieIndex].LinkUp)) {
    return LINK_CHECK_WRONG_PARAMETER;
  }

  do {

    if (RootComplex->Pcie[PcieIndex].LinkUp) {
      // Enable all of RASDES register to detect any training error
      Ac01PFAEnableAll (RootComplex, PcieIndex, PFA_REG_ENABLE);

      // Accessing Endpoint and checking current link capabilities
      Ac01PcieCoreGetEndpointInfo (RootComplex, PcieIndex, &EpMaxWidth, &EpMaxGen);
      LinkStatusCheck = Ac01PcieCoreLinkCheck (RootComplex, PcieIndex, EpMaxWidth, EpMaxGen);

      // Delay to allow the link to perform internal operation and generate
      // any error status update. This allows detection of any error observed
      // during initial link training. Possible evaluation time can be
      // between 100ms to 200ms.
      MicroSecondDelay (100000);

      // Check for error
      RasdesChecking = Ac01PFAEnableAll (RootComplex, PcieIndex, PFA_REG_READ);

      // Clear error counter
      Ac01PFAEnableAll (RootComplex, PcieIndex, PFA_REG_CLEAR);

      // If link check functions return passed, then breaking out
      // else go to soft reset
      if (LinkStatusCheck != LINK_CHECK_FAILED &&
          RasdesChecking != LINK_CHECK_FAILED &&
          PcieLinkUpCheck (&RootComplex->Pcie[PcieIndex]))
      {
        return LINK_CHECK_SUCCESS;
      }

      RootComplex->Pcie[PcieIndex].LinkUp = FALSE;
    }

    // Trigger controller soft reset
    DEBUG ((DEBUG_INFO, "PCIE%d.%d Start link re-initialization..\n", RootComplex->ID, PcieIndex));
    Ac01PcieCoreSetupRC (RootComplex, TRUE, PcieIndex);

    PollLinkUp (RootComplex, PcieIndex);

    NumberOfReset--;
  } while (NumberOfReset > 0);

  return LINK_CHECK_SUCCESS;
}

VOID
Ac01PcieCoreUpdateLink (
  IN  AC01_ROOT_COMPLEX *RootComplex,
  OUT BOOLEAN           *IsNextRoundNeeded,
  OUT INT8              *FailedPciePtr,
  OUT INT8              *FailedPcieCount
  )
{
  AC01_PCIE_CONTROLLER      *Pcie;
  PHYSICAL_ADDRESS          CfgAddr;
  UINT8                     PcieIndex;
  UINT32                    Index;
  UINT32                    Val;

  *IsNextRoundNeeded = FALSE;
  *FailedPcieCount   = 0;
  for (Index = 0; Index < MaxPcieControllerB; Index++) {
    FailedPciePtr[Index] = -1;
  }

  if (!RootComplex->Active) {
    return;
  }

  // Loop for all controllers
  for (PcieIndex = 0; PcieIndex < RootComplex->MaxPcieController; PcieIndex++) {
    Pcie = &RootComplex->Pcie[PcieIndex];
    CfgAddr = RootComplex->MmcfgBase + (RootComplex->Pcie[PcieIndex].DevNum << 15);

    if (Pcie->Active && !Pcie->LinkUp) {
      if (PcieLinkUpCheck (Pcie)) {
        Pcie->LinkUp = TRUE;
        Val = MmioRead32 (CfgAddr + LINK_CONTROL_LINK_STATUS_REG);
        DEBUG ((
          DEBUG_INFO,
          "%a Socket%d RootComplex%d RP%d NEGO_LINK_WIDTH: 0x%x LINK_SPEED: 0x%x\n",
          __FUNCTION__,
          RootComplex->Socket,
          RootComplex->ID,
          PcieIndex,
          CAP_NEGO_LINK_WIDTH_GET (Val),
          CAP_LINK_SPEED_GET (Val)
          ));

        // Doing link checking and recovery if needed
        Ac01PcieCoreQoSLinkCheckRecovery (RootComplex, PcieIndex);

        // Un-mask Completion Timeout
        MaskCompletionTimeOut (RootComplex, PcieIndex, 32, FALSE);

      } else {
        *IsNextRoundNeeded = FALSE;
        FailedPciePtr[*FailedPcieCount] = PcieIndex;
        *FailedPcieCount += 1;
      }
    }
  }
}

/**
  Verify the link status and retry to initialize the Root Complex if there's any issue.

  @param RootComplexList      Pointer to the Root Complex list
**/
VOID
Ac01PcieCorePostSetupRC (
  IN AC01_ROOT_COMPLEX *RootComplexList
  )
{
  UINT8   RCIndex, Idx;
  BOOLEAN IsNextRoundNeeded = FALSE, NextRoundNeeded;
  UINT64  PrevTick, CurrTick, ElapsedCycle;
  UINT64  TimerTicks64;
  UINT8   ReInit;
  INT8    FailedPciePtr[MaxPcieControllerB];
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
    CurrTick = ArmGenericTimerGetSystemCount ();
    if (CurrTick < PrevTick) {
      ElapsedCycle += (UINT64)(~0x0ULL) - PrevTick;
      PrevTick = 0;
    }
    ElapsedCycle += (CurrTick - PrevTick);
    PrevTick = CurrTick;
  } while (ElapsedCycle < TimerTicks64);

  for (RCIndex = 0; RCIndex < AC01_PCIE_MAX_ROOT_COMPLEX; RCIndex++) {
    Ac01PcieCoreUpdateLink (&RootComplexList[RCIndex], &IsNextRoundNeeded, FailedPciePtr, &FailedPcieCount);
    if (IsNextRoundNeeded) {
      NextRoundNeeded = 1;
    }
  }

  if (NextRoundNeeded && ReInit < MAX_REINIT) {
    //
    // Timer is up. Give another chance to re-program controller
    //
    ReInit++;
    for (RCIndex = 0; RCIndex < AC01_PCIE_MAX_ROOT_COMPLEX; RCIndex++) {
      Ac01PcieCoreUpdateLink (&RootComplexList[RCIndex], &IsNextRoundNeeded, FailedPciePtr, &FailedPcieCount);
      if (IsNextRoundNeeded) {
        for (Idx = 0; Idx < FailedPcieCount; Idx++) {
          if (FailedPciePtr[Idx] == -1) {
            continue;
          }

          //
          // Some controller still observes link-down. Re-init controller
          //
          Ac01PcieCoreSetupRC (&RootComplexList[RCIndex], TRUE, FailedPciePtr[Idx]);
        }
      }
    }

    goto _link_polling;
  }
}
