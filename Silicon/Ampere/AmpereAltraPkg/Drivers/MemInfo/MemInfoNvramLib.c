 /**
 *
 * Copyright (c) 2020, Ampere Computing LLC
 *
 *  This program and the accompanying materials
 *  are licensed and made available under the terms and conditions of the BSD License
 *  which accompanies this distribution.  The full text of the license may be found at
 *  http://opensource.org/licenses/bsd-license.php
 *
 *  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 *
 **/

#include <Uefi.h>
#include <Library/NVParamLib.h>

#include "MemInfoScreen.h"
#include "NVParamDef.h"

#define DDR_NVPARAM_ERRCTRL_DE_FIELD_SHIFT    0
#define DDR_NVPARAM_ERRCTRL_DE_FIELD_MASK     0x1

#define DDR_NVPARAM_ERRCTRL_FI_FIELD_SHIFT    1
#define DDR_NVPARAM_ERRCTRL_FI_FIELD_MASK     0x2

/**
  This is function collects meminfo from NVParam

  @param  Data                  The buffer to return the contents.

  @retval EFI_SUCCESS           Get response data successfully.
  @retval Other value           Failed to get meminfo from NVParam
**/
EFI_STATUS
MemInfoNvparamGet (
  OUT    MEM_INFO_VARSTORE_DATA      *VarStoreConfig
  )
{
  UINT32                    Value;
  EFI_STATUS                Status;

  ASSERT (VarStoreConfig != NULL);

  Status = NVParamGet (
             NV_SI_DDR_SPEED,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC, 
             &Value
             );
  if (EFI_ERROR (Status)) {
    VarStoreConfig->DDRSpeedSel = 0; /* Default auto mode */
  } else {
    VarStoreConfig->DDRSpeedSel = Value;
  }

  Status = NVParamGet (
             NV_SI_DDR_ECC_MODE,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status)) {
    VarStoreConfig->EccMode = ECC_SECDED; /* Default enable */
  } else {
    VarStoreConfig->EccMode = Value;
  }

  Status = NVParamGet (
             NV_SI_DDR_ERRCTRL,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status)) {
    VarStoreConfig->ErrCtrl_DE = ERRCTLR_DE_ENABLE;
    VarStoreConfig->ErrCtrl_FI = ERRCTLR_FI_ENABLE;
  } else {
    VarStoreConfig->ErrCtrl_DE = (Value & DDR_NVPARAM_ERRCTRL_DE_FIELD_MASK) >> DDR_NVPARAM_ERRCTRL_DE_FIELD_SHIFT;
    VarStoreConfig->ErrCtrl_FI = (Value & DDR_NVPARAM_ERRCTRL_FI_FIELD_MASK) >> DDR_NVPARAM_ERRCTRL_FI_FIELD_SHIFT;
  }

  Status = NVParamGet (
             NV_SI_DDR_SLAVE_32BIT_MEM_EN,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status)) {
    VarStoreConfig->Slave32bit = 0; /* Default disabled */
  } else {
    VarStoreConfig->Slave32bit = Value;
  }

  Status = NVParamGet (
             NV_SI_DDR_SCRUB_EN,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status)) {
    VarStoreConfig->ScrubPatrol = DDR_DEFAULT_SCRUB_PATROL_DURATION;
  } else {
    VarStoreConfig->ScrubPatrol = Value;
  }

  Status = NVParamGet (
             NV_SI_DDR_WR_BACK_EN,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status)) {
    VarStoreConfig->DemandScrub = DDR_DEFAULT_DEMAND_SCRUB;
  } else {
    VarStoreConfig->DemandScrub = Value;
  }

  Status = NVParamGet (
             NV_SI_DDR_CRC_MODE,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status)) {
    VarStoreConfig->WriteCrc = DDR_DEFAULT_WRITE_CRC;
  } else {
    VarStoreConfig->WriteCrc = Value;
  }

  return EFI_SUCCESS;
}

/**
  This is function stores meminfo to corresponding NVParam

  @param  VarStoreConfig         The contents for the variable.

  @retval EFI_SUCCESS            Set data successfully.
  @retval Other value            Failed to set meminfo to NVParam

**/
EFI_STATUS
MemInfoNvparamSet(
  IN  MEM_INFO_VARSTORE_DATA      *VarStoreConfig
  )
{
  EFI_STATUS                Status;
  UINT32                    Value, TmpValue;

  ASSERT (VarStoreConfig != NULL);

  /* Set DDR speed */
  Status = NVParamGet (
             NV_SI_DDR_SPEED,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status) || Value != VarStoreConfig->DDRSpeedSel) {
    Status = NVParamSet (
               NV_SI_DDR_SPEED,
               NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
               NV_PERM_BIOS | NV_PERM_MANU,
               VarStoreConfig->DDRSpeedSel
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  /* Set ECC mode */
  Status = NVParamGet (
             NV_SI_DDR_ECC_MODE,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status) || Value != VarStoreConfig->EccMode) {
    Status = NVParamSet (
               NV_SI_DDR_ECC_MODE,
               NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
               NV_PERM_BIOS | NV_PERM_MANU,
               VarStoreConfig->EccMode
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  /* Set ErrCtrl */
  TmpValue = (VarStoreConfig->ErrCtrl_DE << DDR_NVPARAM_ERRCTRL_DE_FIELD_SHIFT) |
                (VarStoreConfig->ErrCtrl_FI << DDR_NVPARAM_ERRCTRL_FI_FIELD_SHIFT);
  Status = NVParamGet (
             NV_SI_DDR_ERRCTRL,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status) || Value != TmpValue ) {
    Status = NVParamSet (
               NV_SI_DDR_ERRCTRL,
               NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
               NV_PERM_BIOS | NV_PERM_MANU,
               TmpValue
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  /* Set slave's 32bit region */
  TmpValue = VarStoreConfig->Slave32bit;
  Status = NVParamGet (
             NV_SI_DDR_SLAVE_32BIT_MEM_EN,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status) || Value != TmpValue ) {
    if (TmpValue == 0) {
      /* Default is disabled so just clear nvparam */
      Status = NVParamClr (
                 NV_SI_DDR_SLAVE_32BIT_MEM_EN,
                 NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC
                 );
    } else {
      Status = NVParamSet (
                 NV_SI_DDR_SLAVE_32BIT_MEM_EN,
                 NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
                 NV_PERM_BIOS | NV_PERM_MANU,
                 TmpValue
                 );
    }
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  /* Set Scrub patrol */
  TmpValue = VarStoreConfig->ScrubPatrol;
  Status = NVParamGet (
             NV_SI_DDR_SCRUB_EN,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status) || Value != TmpValue ) {
    if (TmpValue == DDR_DEFAULT_SCRUB_PATROL_DURATION) {
      Status = NVParamClr (
                 NV_SI_DDR_SCRUB_EN,
                 NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC
                 );
    } else {
      Status = NVParamSet (
                 NV_SI_DDR_SCRUB_EN,
                 NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
                 NV_PERM_BIOS | NV_PERM_MANU,
                 TmpValue
                 );
    }
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  /* Demand Scrub */
  TmpValue = VarStoreConfig->DemandScrub;
  Status = NVParamGet (
             NV_SI_DDR_WR_BACK_EN,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status) || Value != TmpValue ) {
    if (TmpValue == DDR_DEFAULT_DEMAND_SCRUB) {
      Status = NVParamClr (
                 NV_SI_DDR_WR_BACK_EN,
                 NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC
                 );
    } else {
      Status = NVParamSet (
                 NV_SI_DDR_WR_BACK_EN,
                 NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU |NV_PERM_BMC,
                 NV_PERM_BIOS | NV_PERM_MANU,
                 TmpValue
                 );
    }
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  /* Write CRC */
  TmpValue = VarStoreConfig->WriteCrc;
  Status = NVParamGet (
             NV_SI_DDR_CRC_MODE,
             NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
             &Value
             );
  if (EFI_ERROR (Status) || Value != TmpValue ) {
    if (TmpValue == DDR_DEFAULT_WRITE_CRC) {
      Status = NVParamClr (
                 NV_SI_DDR_CRC_MODE, 
                 NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC
                 );
    } else {
      Status = NVParamSet (
                 NV_SI_DDR_CRC_MODE,
                 NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
                 NV_PERM_BIOS | NV_PERM_MANU,
                 TmpValue
                 );
    }
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}
