/** @file

  Pcie board specific driver to handle asserting PERST signal to Endpoint
  card and parsing NVPARAM board settings for bifurcation programming.

  PERST asserting is via group of GPIO pins to CPLD as Platform Specification.

  NVPARAM board settings is spec-ed within Firmware Interface Requirement.
  Bifuration devmap is programmed before at SCP following the rule

  Root Complex Type-A devmap settings (RP == Root Port)
  -----------------------------------------
  | RP0   | RP1  | RP2  | RP3  | Devmap   |
  | (x16) | (x4) | (x8) | (x4) | (output) |
  -------------------------------------------
  |  Y    |  N   |  N   |  N   | 0        |
  |  Y    |  N   |  Y   |  N   | 1        |
  |  Y    |  N   |  Y   |  Y   | 2        |
  |  Y    |  Y   |  Y   |  Y   | 3        |
  -----------------------------------------

  Root Complex Type-B LO (aka RCBxA) devmap settings (RP == Root Port)
  ----------------------------------------
  | RP0  | RP1  | RP2  | RP3  | Devmap   |
  | (x8) | (x2) | (x4) | (x3) | (output) |
  ----------------------------------------
  |  Y   |  N   |  N   |  N   | 0        |
  |  Y   |  N   |  Y   |  N   | 1        |
  |  Y   |  N   |  Y   |  Y   | 2        |
  |  Y   |  Y   |  Y   |  Y   | 3        |
  ----------------------------------------

  Root Complex Type-B LO (aka RCBxB) devmap settings (RP == Root Port)
  ----------------------------------------
  | RP4  | RP5  | RP6  | RP7  | Devmap   |
  | (x8) | (x2) | (x4) | (x3) | (output) |
  ----------------------------------------
  |  Y   |  N   |  N   |  N   | 0        |
  |  Y   |  N   |  Y   |  N   | 1        |
  |  Y   |  N   |  Y   |  Y   | 2        |
  |  Y   |  Y   |  Y   |  Y   | 3        |
  ----------------------------------------

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Guid/PlatformInfoHobGuid.h>
#include <Library/AmpereCpuLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/GpioLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/NVParamLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <NVParamDef.h>
#include <Platform/Ac01.h>
#include <PlatformInfoHob.h>

#include "BoardPcie.h"
#include "BoardPcieVfr.h"

#ifndef BIT
#define BIT(nr)                         (1 << (nr))
#endif

#define RCA_MAX_PERST_GROUPVAL          62
#define RCB_MAX_PERST_GROUPVAL          46

extern CHAR16   gPcieVarstoreName[];
extern EFI_GUID gPcieFormSetGuid;

VOID
EFIAPI
BoardPcieLoadPreset (
  IN AC01_RC *RC
  )
{
  UINT32 Nv;
  INTN   NvParam;
  INTN   Ret;
  INTN   Index;

  // Load default value
  for (Index = 0; Index < MAX_PCIE_B; Index++) {
    RC->PresetGen3[Index] = PRESET_INVALID;
    RC->PresetGen4[Index] = PRESET_INVALID;
  }

  // Load override value
  if (RC->Socket == 0) {
    if (RC->Type == RCA) {
      if (RC->ID < MAX_RCA) {
        NvParam = NV_SI_RO_BOARD_S0_RCA0_TXRX_G3PRESET + RC->ID * NV_PARAM_ENTRYSIZE;
      } else {
        NvParam = NV_SI_RO_BOARD_S0_RCA4_TXRX_G3PRESET + (RC->ID - MAX_RCA) * NV_PARAM_ENTRYSIZE;
      }
    } else {
      //
      // There're two NVParam entries per RCB
      //
      NvParam = NV_SI_RO_BOARD_S0_RCB0A_TXRX_G3PRESET + (RC->ID - MAX_RCA) * (NV_PARAM_ENTRYSIZE * 2);
    }
  } else if (RC->Type == RCA) {
    if (RC->ID < MAX_RCA) {
      NvParam = NV_SI_RO_BOARD_S1_RCA2_TXRX_G3PRESET + (RC->ID - 2) * NV_PARAM_ENTRYSIZE;
    } else {
      NvParam = NV_SI_RO_BOARD_S1_RCA4_TXRX_G3PRESET + (RC->ID - MAX_RCA) * NV_PARAM_ENTRYSIZE;
    }
  } else {
    //
    // There're two NVParam entries per RCB
    //
    NvParam = NV_SI_RO_BOARD_S1_RCB0A_TXRX_G3PRESET + (RC->ID - MAX_RCA) * (NV_PARAM_ENTRYSIZE * 2);
  }

  Ret = NVParamGet ((NVPARAM)NvParam, NV_PERM_ALL, &Nv);
  if (Ret == EFI_SUCCESS) {
    for (Index = 0; Index < MAX_PCIE_A; Index++) {
      RC->PresetGen3[Index] = (Nv >> (Index * BITS_PER_BYTE)) & BYTE_MASK;
    }
  }

  if (RC->Type == RCB) {
    NvParam += NV_PARAM_ENTRYSIZE;
    Ret = NVParamGet ((NVPARAM)NvParam, NV_PERM_ALL, &Nv);
    if (Ret == EFI_SUCCESS) {
      for (Index = MAX_PCIE_A; Index < MAX_PCIE; Index++) {
        RC->PresetGen3[Index] = (Nv >> ((Index - MAX_PCIE_A) * BITS_PER_BYTE)) & BYTE_MASK;
      }
    }
  }

  if (RC->Socket == 0) {
    if (RC->Type == RCA) {
      if (RC->ID < MAX_RCA) {
        NvParam = NV_SI_RO_BOARD_S0_RCA0_TXRX_G4PRESET + RC->ID * NV_PARAM_ENTRYSIZE;
      } else {
        NvParam = NV_SI_RO_BOARD_S0_RCA4_TXRX_G4PRESET + (RC->ID - MAX_RCA) * NV_PARAM_ENTRYSIZE;
      }
    } else {
      //
      // There're two NVParam entries per RCB
      //
      NvParam = NV_SI_RO_BOARD_S0_RCB0A_TXRX_G4PRESET + (RC->ID - MAX_RCA) * (NV_PARAM_ENTRYSIZE * 2);
    }
  } else if (RC->Type == RCA) {
    if (RC->ID < MAX_RCA) {
      NvParam = NV_SI_RO_BOARD_S1_RCA2_TXRX_G4PRESET + (RC->ID - 2) * NV_PARAM_ENTRYSIZE;
    } else {
      NvParam = NV_SI_RO_BOARD_S1_RCA4_TXRX_G4PRESET + (RC->ID - MAX_RCA) * NV_PARAM_ENTRYSIZE;
    }
  } else {
    //
    // There're two NVParam entries per RCB
    //
    NvParam = NV_SI_RO_BOARD_S1_RCB0A_TXRX_G4PRESET + (RC->ID - MAX_RCA) * (NV_PARAM_ENTRYSIZE * 2);
  }

  Ret = NVParamGet ((NVPARAM)NvParam, NV_PERM_ALL, &Nv);
  if (Ret == EFI_SUCCESS) {
    for (Index = 0; Index < MAX_PCIE_A; Index++) {
      RC->PresetGen4[Index] = (Nv >> (Index * BITS_PER_BYTE)) & BYTE_MASK;
    }
  }

  if (RC->Type == RCB) {
    NvParam += NV_PARAM_ENTRYSIZE;
    Ret = NVParamGet ((NVPARAM)NvParam, NV_PERM_ALL, &Nv);
    if (Ret == EFI_SUCCESS) {
      for (Index = MAX_PCIE_A; Index < MAX_PCIE; Index++) {
        RC->PresetGen4[Index] = (Nv >> ((Index - MAX_PCIE_A) * BITS_PER_BYTE)) & BYTE_MASK;
      }
    }
  }
}

/**
  Parse platform Root Complex information.

  @param[out]  RC                   Root Complex instance to store the platform information.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
EFIAPI
BoardPcieParseRCParams (
  OUT AC01_RC *RC
  )
{
  UINT32             Efuse;
  PLATFORM_INFO_HOB  *PlatformHob;
  UINT8              PlatRCId;
  EFI_STATUS         Status;
  VOID               *Hob;
  UINTN              BufferSize;
  VARSTORE_DATA      VarStoreConfig = {
    .RCStatus = {TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
                 TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE},
    .RCBifurcationLow = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    .RCBifurcationHigh = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    .SmmuPmu = 0
  };

  DEBUG ((DEBUG_INFO, "%a - Socket%d RC%d\n", __FUNCTION__, RC->Socket, RC->ID));

  PlatRCId = RC->Socket * AC01_MAX_RCS_PER_SOCKET + RC->ID;
  // Get RC activation status
  BufferSize = sizeof (VARSTORE_DATA);
  Status = gRT->GetVariable (
                  gPcieVarstoreName,
                  &gPcieFormSetGuid,
                  NULL,
                  &BufferSize,
                  &VarStoreConfig
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a - Failed to read PCIE variable data from config store.\n", __FUNCTION__));
  }

  RC->Active = VarStoreConfig.RCStatus[PlatRCId];
  RC->DevMapLow = VarStoreConfig.RCBifurcationLow[PlatRCId];
  RC->DevMapHigh = VarStoreConfig.RCBifurcationHigh[PlatRCId];

  DEBUG ((
    DEBUG_INFO,
    "%a - Socket%d RC%d is %s\n",
    __FUNCTION__,
    RC->Socket,
    RC->ID,
    (RC->Active) ? "ACTIVE" : "INACTIVE"
    ));

  if (!IsSlaveSocketActive () && RC->Socket == 1) {
    RC->Active = FALSE;
  }

  if (RC->Active) {
    //
    // Consolidate with E-fuse
    //
    Efuse = 0;
    Hob = GetFirstGuidHob (&gPlatformHobGuid);
    if (Hob != NULL) {
      PlatformHob = (PLATFORM_INFO_HOB *)GET_GUID_HOB_DATA (Hob);
      Efuse = PlatformHob->RcDisableMask[0] | (PlatformHob->RcDisableMask[1] << AC01_MAX_RCS_PER_SOCKET);
      DEBUG ((
        DEBUG_INFO,
        "RcDisableMask[0]: 0x%x [1]: 0x%x\n",
        PlatformHob->RcDisableMask[0],
        PlatformHob->RcDisableMask[1]
        ));

      // Update errata flags for Ampere Altra
      if ((PlatformHob->ScuProductId[0] & 0xff) == 0x01) {
        if (PlatformHob->AHBCId[0] == 0x20100
            || PlatformHob->AHBCId[0] == 0x21100
            || (IsSlaveSocketActive ()
                && (PlatformHob->AHBCId[1] == 0x20100
                    || PlatformHob->AHBCId[1] == 0x21100)))
        {
          RC->Flags |= PCIE_ERRATA_SPEED1;
          DEBUG ((DEBUG_INFO, "RC[%d]: Flags 0x%x\n", RC->ID, RC->Flags));
        }
      }
    }

    RC->Active = (RC->Active && !(Efuse & BIT (PlatRCId))) ? TRUE : FALSE;
  }

  /* Load Gen3/Gen4 preset */
  BoardPcieLoadPreset (RC);
  BoardPcieGetLaneAllocation (RC);
  BoardPcieSetupDevmap (RC);
  BoardPcieGetSpeed (RC);

  return EFI_SUCCESS;
}

VOID
EFIAPI
BoardPcieReleaseAllPerst (
  IN UINT8 SocketId
  )
{
  UINT32 GpioIndex, GpioPin;

  // Write 1 to all GPIO[16..21] to release all PERST
  GpioPin = GPIO_DWAPB_PINS_PER_SOCKET * SocketId + 16;
  for (GpioIndex = 0; GpioIndex < 6; GpioIndex++) {
    GpioModeConfig (GpioPin + GpioIndex, GPIO_CONFIG_OUT_HI);
  }
}

/**
  Assert PERST of input PCIe controller

  @param[in]  RC                    Root Complex instance.
  @param[in]  PcieIndex             PCIe controller index of input Root Complex.
  @param[in]  Bifurcation           Bifurcation mode of input Root Complex.
  @param[in]  IsPullToHigh          Target status for the PERST.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
EFIAPI
BoardPcieAssertPerst (
  IN AC01_RC *RC,
  IN UINT32  PcieIndex,
  IN UINT8   Bifurcation,
  IN BOOLEAN IsPullToHigh
  )
{
  UINT32 GpioGroupVal, Val, GpioIndex, GpioPin;

  if (!IsPullToHigh) { // Pull PERST to Low
    if (RC->Type == RCA) { // RCA: RC->ID: 0->3 ; PcieIndex: 0->3
      GpioGroupVal = RCA_MAX_PERST_GROUPVAL - RC->ID * MAX_PCIE_A - PcieIndex;
    } else { // RCB: RC->ID: 4->7 ; PcieIndex: 0->7
      GpioGroupVal = RCB_MAX_PERST_GROUPVAL - (RC->ID - MAX_RCA) * MAX_PCIE_B - PcieIndex;
    }

    // Update the value of GPIO[16..21]. Corresponding PERST line will be decoded by CPLD.
    GpioPin = GPIO_DWAPB_PINS_PER_SOCKET * RC->Socket + 16;
    for (GpioIndex = 0; GpioIndex < 6; GpioIndex++) {
      Val = (GpioGroupVal & 0x3F) & (1 << GpioIndex);
      if (Val == 0) {
        GpioModeConfig (GpioPin + GpioIndex, GPIO_CONFIG_OUT_LOW);
      } else {
        GpioModeConfig (GpioPin + GpioIndex, GPIO_CONFIG_OUT_HI);
      }
    }

    // Keep reset as low as 100 ms as specification
    MicroSecondDelay (100 * 1000);
  } else { // Pull PERST to High
    BoardPcieReleaseAllPerst (RC->Socket);
  }

  return EFI_SUCCESS;
}

/**
  Override the segment number for a root complex with a board specific number.

  @param[in]  RC                    Root Complex instance with properties.
  @param[out] SegmentNumber         Return segment number.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
EFIAPI
BoardPcieGetRCSegmentNumber (
  IN  AC01_RC *RC,
  OUT UINTN   *SegmentNumber
  )
{
  if (RC->Socket == 0) {
    if (RC->Type == RCA) {
      switch (RC->ID) {
      case RCA0:
        *SegmentNumber = 12;
        break;

      case RCA1:
        *SegmentNumber = 13;
        break;

      case RCA2:
        *SegmentNumber = 1;
        break;

      case RCA3:
        *SegmentNumber = 0;
        break;

      default:
        *SegmentNumber = 16;
      }
    } else { /* Socket0, CCIX: RCA0 and RCA1 */
      *SegmentNumber = RC->ID - 2;
    }
  } else { /* Socket1, CCIX: RCA0 and RCA1 */
    if (RC->ID == RCA0 || RC->ID == RCA1) {
      *SegmentNumber = 16;
    } else {
      *SegmentNumber = 4 + RC->ID;
    }
  }

  return EFI_SUCCESS;
}

/**
  Check if SMM PMU enabled in board screen.

  @param[out]  IsSmmuPmuEnabled     TRUE if the SMMU PMU enabled, otherwise FALSE.

  @retval EFI_SUCCESS               The operation is successful.
  @retval Others                    An error occurred.
**/
EFI_STATUS
EFIAPI
BoardPcieCheckSmmuPmuEnabled (
  OUT BOOLEAN *IsSmmuPmuEnabled
  )
{
  VARSTORE_DATA      VarStoreConfig;
  UINTN              BufferSize;
  EFI_STATUS         Status;

  *IsSmmuPmuEnabled = FALSE;

  // Get Buffer Storage data from EFI variable
  BufferSize = sizeof (VARSTORE_DATA);
  Status = gRT->GetVariable (
                  gPcieVarstoreName,
                  &gPcieFormSetGuid,
                  NULL,
                  &BufferSize,
                  &VarStoreConfig
                  );
  if (!EFI_ERROR (Status)) {
    *IsSmmuPmuEnabled = VarStoreConfig.SmmuPmu;
  }

  return EFI_SUCCESS;
}
