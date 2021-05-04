/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/DebugLib.h>
#include <Library/NVParamLib.h>
#include <Library/SystemFirmwareInterfaceLib.h>
#include <NVParamDef.h>

#include "BoardPcie.h"

/* Host bridge registers */
#define RCA_DEV_MAP_OFFSET    0x0
#define RCB_DEV_MAP_OFFSET    0x4

/* RCA_DEV_MAP_OFFSET */
#define RCA_DEV_MAP_SET(dst, src) \
  (((dst) & ~0x7) | (((UINT32)(src)) & 0x7))

/* RCB_DEV_MAP_OFFSET */
#define RCB_DEV_MAP_LOW_SET(dst, src) \
  (((dst) & ~0x7) | (((UINT32)(src)) & 0x7))
#define RCB_DEV_MAP_HIGH_SET(dst, src) \
  (((dst) & ~0x70) | (((UINT32)(src) << 4) & 0x70))

#define PCIE_GET_MAX_WIDTH(Pcie, Max) \
  !((Pcie).MaxWidth) ? (Max) : MIN((Pcie).MaxWidth, (Max))

BOOLEAN
IsEmptyRC (
  IN AC01_RC *RC
  )
{
  INTN Idx;

  for (Idx = PCIE_0; Idx < MAX_PCIE; Idx++) {
    if (RC->Pcie[Idx].Active) {
      return FALSE;
    }
  }

  return TRUE;
}

VOID
BoardPcieSetRCBifurcation (
  IN AC01_RC *RC,
  IN UINT8   RPStart,
  IN UINT8   DevMap
  )
{
  UINT8 MaxWidth;

  if (RPStart != PCIE_0 && RPStart != PCIE_4) {
    return;
  }

  if (RC->Type != RCB && RPStart == PCIE_4) {
    return;
  }

  if (RC->Type == RCA && RC->Pcie[RPStart].MaxWidth == LNKW_X16) {
    RC->Pcie[RPStart + 1].MaxWidth = LNKW_X4;
    RC->Pcie[RPStart + 2].MaxWidth = LNKW_X8;
    RC->Pcie[RPStart + 3].MaxWidth = LNKW_X4;
  }

  if (RC->Type == RCB && RC->Pcie[RPStart].MaxWidth == LNKW_X8) {
    RC->Pcie[RPStart + 1].MaxWidth = LNKW_X2;
    RC->Pcie[RPStart + 2].MaxWidth = LNKW_X4;
    RC->Pcie[RPStart + 3].MaxWidth = LNKW_X2;
  }

  switch (DevMap) {
  case DEV_MAP_2_CONTROLLERS:
    MaxWidth = (RC->Type == RCA) ? LNKW_X8 : LNKW_X4;
    RC->Pcie[RPStart].MaxWidth = PCIE_GET_MAX_WIDTH (RC->Pcie[RPStart], MaxWidth);
    RC->Pcie[RPStart + 1].Active = FALSE;
    RC->Pcie[RPStart + 2].MaxWidth = PCIE_GET_MAX_WIDTH (RC->Pcie[RPStart + 2], MaxWidth);
    RC->Pcie[RPStart + 2].Active = TRUE;
    RC->Pcie[RPStart + 3].Active = FALSE;
    break;

  case DEV_MAP_3_CONTROLLERS:
    MaxWidth = (RC->Type == RCA) ? LNKW_X8 : LNKW_X4;
    RC->Pcie[RPStart].MaxWidth = PCIE_GET_MAX_WIDTH (RC->Pcie[RPStart], MaxWidth);
    RC->Pcie[RPStart + 1].Active = FALSE;
    MaxWidth = (RC->Type == RCA) ? LNKW_X4 : LNKW_X2;
    RC->Pcie[RPStart + 2].MaxWidth = PCIE_GET_MAX_WIDTH (RC->Pcie[RPStart + 2], MaxWidth);
    RC->Pcie[RPStart + 2].Active = TRUE;
    RC->Pcie[RPStart + 3].MaxWidth = PCIE_GET_MAX_WIDTH (RC->Pcie[RPStart + 3], MaxWidth);
    RC->Pcie[RPStart + 3].Active = TRUE;
    break;

  case DEV_MAP_4_CONTROLLERS:
    MaxWidth = (RC->Type == RCA) ? LNKW_X4 : LNKW_X2;
    RC->Pcie[RPStart].MaxWidth = PCIE_GET_MAX_WIDTH (RC->Pcie[RPStart], MaxWidth);
    RC->Pcie[RPStart + 1].MaxWidth = PCIE_GET_MAX_WIDTH (RC->Pcie[RPStart + 1], MaxWidth);
    RC->Pcie[RPStart + 1].Active = TRUE;
    RC->Pcie[RPStart + 2].MaxWidth = PCIE_GET_MAX_WIDTH (RC->Pcie[RPStart + 2], MaxWidth);
    RC->Pcie[RPStart + 2].Active = TRUE;
    RC->Pcie[RPStart + 3].MaxWidth = PCIE_GET_MAX_WIDTH (RC->Pcie[RPStart + 3], MaxWidth);
    RC->Pcie[RPStart + 3].Active = TRUE;
    break;

  case DEV_MAP_1_CONTROLLER:
  default:
    MaxWidth = (RC->Type == RCA) ? LNKW_X16 : LNKW_X8;
    RC->Pcie[RPStart].MaxWidth = PCIE_GET_MAX_WIDTH (RC->Pcie[RPStart], MaxWidth);
    RC->Pcie[RPStart + 1].Active = FALSE;
    RC->Pcie[RPStart + 2].Active = FALSE;
    RC->Pcie[RPStart + 3].Active = FALSE;
    break;
  }
}

VOID
BoardPcieGetLaneAllocation (
  IN AC01_RC *RC
  )
{
  INTN    RPIndex, Ret;
  UINT32  Nv, Width;
  NVPARAM NvParam;

  // Retrieve lane allocation and capabilities for each controller
  if (RC->Type == RCA) {
    NvParam = ((RC->Socket == 0) ? NV_SI_RO_BOARD_S0_RCA0_CFG : NV_SI_RO_BOARD_S1_RCA0_CFG) +
              RC->ID * NV_PARAM_ENTRYSIZE;
  } else {
    //
    // There're two NVParam entries per RCB
    //
    NvParam = ((RC->Socket == 0) ? NV_SI_RO_BOARD_S0_RCB0_LO_CFG : NV_SI_RO_BOARD_S1_RCB0_LO_CFG) +
              (RC->ID - MAX_RCA) * (NV_PARAM_ENTRYSIZE * 2);
  }

  Ret = NVParamGet (NvParam, NV_PERM_ALL, &Nv);
  Nv = (Ret != EFI_SUCCESS) ? 0 : Nv;

  for (RPIndex = 0; RPIndex < MAX_PCIE_A; RPIndex++) {
    Width = (Nv >> (RPIndex * BITS_PER_BYTE)) & BYTE_MASK;
    switch (Width) {
    case 1:
    case 2:
    case 3:
    case 4:
      RC->Pcie[RPIndex].MaxWidth = 1 << Width;
      RC->Pcie[RPIndex].MaxGen = SPEED_GEN3;
      RC->Pcie[RPIndex].Active = TRUE;
      break;

    case 0:
    default:
      RC->Pcie[RPIndex].MaxWidth = LNKW_NONE;
      RC->Pcie[RPIndex].MaxGen = SPEED_NONE;
      RC->Pcie[RPIndex].Active = FALSE;
      break;
    }
  }

  if (RC->Type == RCB) {
    NvParam += NV_PARAM_ENTRYSIZE;
    Ret = NVParamGet (NvParam, NV_PERM_ALL, &Nv);
    Nv = (Ret != EFI_SUCCESS) ? 0 : Nv;

    for (RPIndex = MAX_PCIE_A; RPIndex < MAX_PCIE; RPIndex++) {
      Width = (Nv >> ((RPIndex - MAX_PCIE_A) * BITS_PER_BYTE)) & BYTE_MASK;
      switch (Width) {
      case 1:
      case 2:
      case 3:
      case 4:
        RC->Pcie[RPIndex].MaxWidth = 1 << Width;
        RC->Pcie[RPIndex].MaxGen = SPEED_GEN3;
        RC->Pcie[RPIndex].Active = TRUE;
        break;

      case 0:
      default:
        RC->Pcie[RPIndex].MaxWidth = LNKW_NONE;
        RC->Pcie[RPIndex].MaxGen = SPEED_NONE;
        RC->Pcie[RPIndex].Active = FALSE;
        break;
      }
    }
  }

  // Do not proceed if no Root Port enabled
  if (IsEmptyRC (RC)) {
    RC->Active = FALSE;
  }
}

VOID
BoardPcieSetupDevmap (
  IN AC01_RC *RC
  )
{
  UINT32 Val;

  if (RC->Pcie[PCIE_0].Active
      && RC->Pcie[PCIE_1].Active
      && RC->Pcie[PCIE_2].Active
      && RC->Pcie[PCIE_3].Active)
  {
    RC->DefaultDevMapLow = DEV_MAP_4_CONTROLLERS;
  } else if (RC->Pcie[PCIE_0].Active
             && RC->Pcie[PCIE_2].Active
             && RC->Pcie[PCIE_3].Active)
  {
    RC->DefaultDevMapLow = DEV_MAP_3_CONTROLLERS;
  } else if (RC->Pcie[PCIE_0].Active
             && RC->Pcie[PCIE_2].Active)
  {
    RC->DefaultDevMapLow = DEV_MAP_2_CONTROLLERS;
  } else {
    RC->DefaultDevMapLow = DEV_MAP_1_CONTROLLER;
  }

  if (RC->Pcie[PCIE_4].Active
      && RC->Pcie[PCIE_5].Active
      && RC->Pcie[PCIE_6].Active
      && RC->Pcie[PCIE_7].Active)
  {
    RC->DefaultDevMapHigh = DEV_MAP_4_CONTROLLERS;
  } else if (RC->Pcie[PCIE_4].Active
             && RC->Pcie[PCIE_6].Active
             && RC->Pcie[PCIE_7].Active)
  {
    RC->DefaultDevMapHigh = DEV_MAP_3_CONTROLLERS;
  } else if (RC->Pcie[PCIE_4].Active
             && RC->Pcie[PCIE_6].Active)
  {
    RC->DefaultDevMapHigh = DEV_MAP_2_CONTROLLERS;
  } else {
    RC->DefaultDevMapHigh = DEV_MAP_1_CONTROLLER;
  }

  if (RC->DevMapLow == DEV_MAP_1_CONTROLLER) {
    RC->DevMapLow = RC->DefaultDevMapLow;
  }

  if (RC->Type == RCB && RC->DevMapHigh == DEV_MAP_1_CONTROLLER) {
    RC->DevMapHigh = RC->DefaultDevMapHigh;
  }

  BoardPcieSetRCBifurcation (RC, PCIE_0, RC->DevMapLow);
  if (RC->Type == RCB) {
    BoardPcieSetRCBifurcation (RC, PCIE_4, RC->DevMapHigh);
  }

  if (RC->Active) {
    if (RC->Type == RCA) {
      if (!EFI_ERROR (MailboxMsgRegisterRead (RC->Socket, RC->HBAddr + RCA_DEV_MAP_OFFSET, &Val))) {
        Val = RCA_DEV_MAP_SET (Val, RC->DevMapLow);
        MailboxMsgRegisterWrite (RC->Socket, RC->HBAddr + RCA_DEV_MAP_OFFSET, Val);
      }
    } else {
      if (!EFI_ERROR (MailboxMsgRegisterRead (RC->Socket, RC->HBAddr + RCB_DEV_MAP_OFFSET, &Val))) {
        Val = RCB_DEV_MAP_LOW_SET (Val, RC->DevMapLow);
        Val = RCB_DEV_MAP_HIGH_SET (Val, RC->DevMapHigh);
        MailboxMsgRegisterWrite (RC->Socket, RC->HBAddr + RCB_DEV_MAP_OFFSET, Val);
      }
    }
  }
}

VOID
BoardPcieGetSpeed (
  IN AC01_RC *RC
  )
{
  UINT8 MaxGenTbl[MAX_PCIE_A] = { SPEED_GEN4, SPEED_GEN4, SPEED_GEN4, SPEED_GEN4 };         // Bifurcation 0: RCA x16 / RCB x8
  UINT8 MaxGenTblX8X4X4[MAX_PCIE_A] = { SPEED_GEN4, SPEED_GEN4, SPEED_GEN1, SPEED_GEN1 };   // Bifurcation 2: x8 x4 x4 (PCIE_ERRATA_SPEED1)
  UINT8 MaxGenTblX4X4X4X4[MAX_PCIE_A] = { SPEED_GEN1, SPEED_GEN1, SPEED_GEN1, SPEED_GEN1 }; // Bifurcation 3: x4 x4 x4 x4 (PCIE_ERRATA_SPEED1)
  UINT8 MaxGenTblRCB[MAX_PCIE_A] = { SPEED_GEN1, SPEED_GEN1, SPEED_GEN1, SPEED_GEN1 };      // RCB PCIE_ERRATA_SPEED1
  INTN  RPIdx;
  UINT8 *MaxGen;

  ASSERT (MAX_PCIE_A == 4);
  ASSERT (MAX_PCIE == 8);

  //
  // Due to hardware errata, for A0/A1*
  //  RCB is limited to Gen1 speed.
  //  RCA x16, x8 port supports up to Gen4,
  //  RCA x4 port only supports Gen1.
  //
  MaxGen = MaxGenTbl;
  if (RC->Type == RCB) {
    if (RC->Flags & PCIE_ERRATA_SPEED1) {
      MaxGen = MaxGenTblRCB;
    }
  } else {
    switch (RC->DevMapLow) {
    case DEV_MAP_3_CONTROLLERS: /* x8 x4 x4 */
      if (RC->Flags & PCIE_ERRATA_SPEED1) {
        MaxGen = MaxGenTblX8X4X4;
      }
      break;

    case DEV_MAP_4_CONTROLLERS: /* X4 x4 x4 x4 */
      if (RC->Flags & PCIE_ERRATA_SPEED1) {
        MaxGen = MaxGenTblX4X4X4X4;
      }
      break;

    case DEV_MAP_2_CONTROLLERS: /* x8 x8 */
    default:
      break;
    }
  }

  for (RPIdx = 0; RPIdx < MAX_PCIE_A; RPIdx++) {
    RC->Pcie[RPIdx].MaxGen = RC->Pcie[RPIdx].Active ? MaxGen[RPIdx] : SPEED_NONE;
  }

  if (RC->Type == RCB) {
    for (RPIdx = MAX_PCIE_A; RPIdx < MAX_PCIE; RPIdx++) {
      RC->Pcie[RPIdx].MaxGen = RC->Pcie[RPIdx].Active ?
                               MaxGen[RPIdx - MAX_PCIE_A] : SPEED_NONE;
    }
  }
}
