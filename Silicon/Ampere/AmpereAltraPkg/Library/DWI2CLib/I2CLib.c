/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Uefi.h>

#include <Guid/PlatformInfoHobGuid.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/HobLib.h>
#include <Library/I2CLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <PlatformInfoHob.h>

#undef I2C_DBG
#undef I2C_PRINT

#ifdef I2C_DBG
#define DBG(arg...) DEBUG ((DEBUG_ERROR, "DW_I2C(DBG): "));DEBUG ((DEBUG_ERROR,## arg))
#else
#define DBG(arg...)
#endif

#define ERROR(arg...) DEBUG ((DEBUG_ERROR, "DW_I2C(ERROR): "));DEBUG ((DEBUG_ERROR,## arg))

#ifdef I2C_PRINT
#define PRINT(arg...) DEBUG ((DEBUG_INFO, "DW_I2C(INFO): "));DEBUG ((DEBUG_INFO,## arg))
#else
#define PRINT(arg...)
#endif

/* Runtime needs to be 64K alignment */
#define RUNTIME_ADDRESS_MASK           (~(SIZE_64KB - 1))
#define RUNTIME_ADDRESS_LENGTH         SIZE_64KB

/* Bus specific values */
typedef struct DW_I2C_CONTEXT {
  UINTN  Base;
  UINT32 BusSpeed;
  UINT32 RxFifo;
  UINT32 TxFifo;
  UINT32 PollingTime;
  UINT32 Enabled;
} DW_I2C_CONTEXT_T;

/* I2C SCL counter macros */
enum {
  I2C_SS = 0,
  I2C_FS,
  I2C_PS,
  I2C_HS_400PF,
  I2C_HS_100PF,
};
#define I2C_HS I2C_HS_400PF

enum {
  I2C_SCL_HIGH = 0,
  I2C_SCL_LOW,
  I2C_SCL_TF,
};

enum {
  I2C_SPKLEN = 0,
  I2C_OFFSET,
};

#define SS_SCL_HCNT  250
#define SS_SCL_LCNT  250
#define FS_SCL_HCNT  62
#define FS_SCL_LCNT  63

#define I2CSync() { asm volatile ("dmb ish" : : : "memory"); }

STATIC UINT32 I2CSclMin[][3] = {  /* in nano seconds */
  /* High, Low, tf */
  [I2C_SS] =   {4000, 4700, 300},  /* SS (Standard Speed) */
  [I2C_FS] =   { 600, 1300, 300},  /* FS (Fast Speed) */
  [I2C_PS] =   { 260, 500, 120},   /* PS (Fast Plus Speed) */
  [I2C_HS_400PF] = { 160,  320, 300},  /* HS (High Speed) 400pf */
  [I2C_HS_100PF] = {  60,  120, 300},  /* HS (High Speed) 100pf */
};

STATIC UINT32 I2CSclParam[][2] = {
  /* Spklen, offset */
  [I2C_SS] =   {10, 300},  /* SS (Standard Speed) */
  [I2C_FS] =   {10, 0},    /* FS (Fast Speed) */
  [I2C_PS] =   {10, 0},    /* PS (Fast Plus Speed) */
  [I2C_HS_400PF] = {0, 0}, /* HS (High Speed) 400pf */
  [I2C_HS_100PF] = {0, 0}, /* HS (High Speed) 100pf */
};

STATIC BOOLEAN            I2CRuntimeEnableArray[MAX_PLATFORM_I2C_BUS_NUM] = {FALSE};
STATIC UINTN              I2CBaseArray[MAX_PLATFORM_I2C_BUS_NUM] = {PLATFORM_I2C_REGISTER_BASE};
STATIC DW_I2C_CONTEXT_T   I2CBusList[MAX_PLATFORM_I2C_BUS_NUM];
STATIC UINTN              I2CClock = 0;
STATIC EFI_EVENT          mVirtualAddressChangeEvent = NULL;

#ifndef BIT
#define BIT(nr)                         (1 << (nr))
#endif

#define DW_SIGNATURE                    0x44570000  /* 'D' 'W' */

/*
 * Registers
 */
#define DW_IC_CON                       0x0
#define  DW_IC_CON_MASTER               BIT(0)
#define  DW_IC_CON_SPEED_STD            BIT(1)
#define  DW_IC_CON_SPEED_FAST           BIT(2)
#define  DW_IC_CON_10BITADDR_MASTER     BIT(4)
#define  DW_IC_CON_RESTART_EN           BIT(5)
#define  DW_IC_CON_SLAVE_DISABLE        BIT(6)
#define DW_IC_TAR                       0x4
#define  DW_IC_TAR_10BITS               BIT(12)
#define DW_IC_SAR                       0x8
#define DW_IC_DATA_CMD                  0x10
#define  DW_IC_DATA_CMD_RESTART         BIT(10)
#define  DW_IC_DATA_CMD_STOP            BIT(9)
#define  DW_IC_DATA_CMD_CMD             BIT(8)
#define  DW_IC_DATA_CMD_DAT_MASK        0xFF
#define DW_IC_SS_SCL_HCNT               0x14
#define DW_IC_SS_SCL_LCNT               0x18
#define DW_IC_FS_SCL_HCNT               0x1c
#define DW_IC_FS_SCL_LCNT               0x20
#define DW_IC_HS_SCL_HCNT               0x24
#define DW_IC_HS_SCL_LCNT               0x28
#define DW_IC_INTR_STAT                 0x2c
#define DW_IC_INTR_MASK                 0x30
#define  DW_IC_INTR_RX_UNDER            BIT(0)
#define  DW_IC_INTR_RX_OVER             BIT(1)
#define  DW_IC_INTR_RX_FULL             BIT(2)
#define  DW_IC_INTR_TX_EMPTY            BIT(4)
#define  DW_IC_INTR_TX_ABRT             BIT(6)
#define  DW_IC_INTR_ACTIVITY            BIT(8)
#define  DW_IC_INTR_STOP_DET            BIT(9)
#define  DW_IC_INTR_START_DET           BIT(10)
#define  DW_IC_ERR_CONDITION \
                (DW_IC_INTR_RX_UNDER | DW_IC_INTR_RX_OVER | DW_IC_INTR_TX_ABRT)
#define DW_IC_RAW_INTR_STAT             0x34
#define DW_IC_CLR_INTR                  0x40
#define DW_IC_CLR_RX_UNDER              0x44
#define DW_IC_CLR_RX_OVER               0x48
#define DW_IC_CLR_TX_ABRT               0x54
#define DW_IC_CLR_ACTIVITY              0x5c
#define DW_IC_CLR_STOP_DET              0x60
#define DW_IC_CLR_START_DET             0x64
#define DW_IC_ENABLE                    0x6c
#define DW_IC_STATUS                    0x70
#define  DW_IC_STATUS_ACTIVITY          BIT(0)
#define  DW_IC_STATUS_TFE               BIT(2)
#define  DW_IC_STATUS_RFNE              BIT(3)
#define  DW_IC_STATUS_MST_ACTIVITY      BIT(5)
#define DW_IC_TXFLR                     0x74
#define DW_IC_RXFLR                     0x78
#define DW_IC_SDA_HOLD                  0x7c
#define DW_IC_TX_ABRT_SOURCE            0x80
#define DW_IC_ENABLE_STATUS             0x9c
#define DW_IC_COMP_PARAM_1              0xf4
#define DW_IC_COMP_TYPE                 0xfc
#define SB_DW_IC_CON                    0xa8
#define SB_DW_IC_SCL_TMO_CNT            0xac
#define SB_DW_IC_RX_PEC                 0xb0
#define SB_DW_IC_ACK                    0xb4
#define SB_DW_IC_FLG                    0xb8
#define SB_DW_IC_FLG_CLR                0xbc
#define SB_DW_IC_INTR_STAT              0xc0
#define SB_DW_IC_INTR_STAT_MASK         0xc4
#define SB_DW_IC_DEBUG_SEL              0xec
#define SB_DW_IC_ACK_DEBUG              0xf0
#define DW_IC_FS_SPKLEN                 0xa0
#define DW_IC_HS_SPKLEN                 0xa4

#define DW_BUS_WAIT_SLEEP               1000 /* 1ms */
#define DW_BUS_WAIT_TIMEOUT_RETRY       20
#define DW_TRANSFER_DATA_TIMEOUT        10000000 /* Max 10s */
#define DW_STATUS_WAIT_RETRY            100

UINT32
Read32 (
  UINTN Addr
  )
{
  return MmioRead32 (Addr);
}

VOID
Write32 (
  UINTN Addr,
  UINT32 Val
  )
{
  MmioWrite32 (Addr, Val);
}

/**
 Initialize I2C Bus
 **/
VOID
I2CHWInit (
  UINT32 Bus
  )
{
  UINT32    Param;

  I2CBusList[Bus].Base = I2CBaseArray[Bus];
  Param = Read32 (I2CBusList[Bus].Base + DW_IC_COMP_PARAM_1);
  I2CBusList[Bus].PollingTime = (10 * 1000000) / I2CBusList[Bus].BusSpeed;
  I2CBusList[Bus].RxFifo = ((Param >> 8) & 0xff) + 1;
  I2CBusList[Bus].TxFifo = ((Param >> 16) & 0xff) + 1;
  I2CBusList[Bus].Enabled = 0;
  DBG ("Bus %d Rx_Buffer %d Tx_Buffer %d\n",
      Bus, I2CBusList[Bus].RxFifo, I2CBusList[Bus].TxFifo);
}

/**
 Enable or disable I2C Bus
 */
VOID
I2CEnable (
  UINT32 Bus,
  UINT32 Enable
  )
{
  UINT32 I2CStatusCnt = DW_STATUS_WAIT_RETRY;
  UINTN  Base         = I2CBusList[Bus].Base;

  I2CBusList[Bus].Enabled = Enable;

  Write32 (Base + DW_IC_ENABLE, Enable);
  do {
    if ((Read32 (Base + DW_IC_ENABLE_STATUS) & 0x01) == Enable) {
      break;
    }
    MicroSecondDelay (I2CBusList[Bus].PollingTime);
  } while (I2CStatusCnt-- != 0);

  if (I2CStatusCnt == 0) {
    ERROR ("Enable/disable timeout\n");
  }

  if ((Enable == 0) || (I2CStatusCnt == 0)) {
    /* Unset the target adddress */
    Write32 (Base + DW_IC_TAR, 0);
    I2CBusList[Bus].Enabled = 0;
  }
}

/**
 Setup Slave address
 **/
VOID
I2CSetSlaveAddr (
  UINT32 Bus,
  UINT32 SlaveAddr
  )
{
  UINTN  Base = I2CBusList[Bus].Base;
  UINT32 OldEnableStatus = I2CBusList[Bus].Enabled;

  I2CEnable (Bus, 0);
  Write32 (Base + DW_IC_TAR, SlaveAddr);
  if (OldEnableStatus != 0) {
    I2CEnable (Bus, 1);
  }
}

/**
 Check for errors on I2C Bus
 **/
UINT32
I2CCheckErrors (
  UINT32 Bus
  )
{
  UINTN  Base = I2CBusList[Bus].Base;
  UINT32 ErrorStatus;

  ErrorStatus = Read32 (Base + DW_IC_RAW_INTR_STAT) & DW_IC_ERR_CONDITION;
  if (ErrorStatus != 0) {
    ERROR ("Errors on i2c bus %d error status %08x\n", Bus, ErrorStatus);
  }

  if ((ErrorStatus & DW_IC_INTR_RX_UNDER) != 0) {
    Read32 (Base + DW_IC_CLR_RX_UNDER);
  }

  if ((ErrorStatus & DW_IC_INTR_RX_OVER) != 0) {
    Read32 (Base + DW_IC_CLR_RX_OVER);
  }

  if ((ErrorStatus & DW_IC_INTR_TX_ABRT) != 0) {
    DBG ("TX_ABORT at source %08x\n", Read32 (Base + DW_IC_TX_ABRT_SOURCE));
    Read32 (Base + DW_IC_CLR_TX_ABRT);
  }

  return ErrorStatus;
}

/**
 Waiting for bus to not be busy
 **/
BOOLEAN
I2CWaitBusNotBusy (
  UINT32 Bus
  )
{
  UINTN Base    = I2CBusList[Bus].Base;
  UINTN Timeout = DW_BUS_WAIT_TIMEOUT_RETRY;

  while ((Read32 (Base + DW_IC_STATUS) & DW_IC_STATUS_MST_ACTIVITY) != 0) {
    if (Timeout == 0) {
      DBG ("Timeout while waiting for bus ready\n");
      return FALSE;
    }
    Timeout--;
    /*
     * A delay isn't absolutely necessary.
     * But to ensure that we don't hammer the bus constantly,
     * delay for DW_BUS_WAIT_SLEEP as with other implementation.
     */
    MicroSecondDelay (DW_BUS_WAIT_SLEEP);
  }

  return TRUE;
}

/**
 Waiting for TX FIFO buffer available
 **/
EFI_STATUS
I2CWaitTxData (
  UINT32 Bus
  )
{
  UINTN Base    = I2CBusList[Bus].Base;
  UINTN Timeout = DW_TRANSFER_DATA_TIMEOUT;

  while (Read32 (Base + DW_IC_TXFLR) == I2CBusList[Bus].TxFifo) {
    if (Timeout <= 0) {
      ERROR ("Timeout waiting for TX buffer available\n");
      return EFI_TIMEOUT;
    }
    MicroSecondDelay (I2CBusList[Bus].PollingTime);
    Timeout -= MicroSecondDelay (I2CBusList[Bus].PollingTime);
  }

  return EFI_SUCCESS;
}

/**
 Waiting for RX FIFO buffer available
 **/
EFI_STATUS
I2CWaitRxData (
  UINT32 Bus
  )
{
  UINTN Base    = I2CBusList[Bus].Base;
  UINTN Timeout = DW_TRANSFER_DATA_TIMEOUT;

  while ((Read32 (Base + DW_IC_STATUS) & DW_IC_STATUS_RFNE) == 0) {
      if (Timeout <= 0) {
          ERROR ("Timeout waiting for RX buffer available\n");
          return EFI_TIMEOUT;
      }

      if ((I2CCheckErrors (Bus) & DW_IC_INTR_TX_ABRT) != 0) {
          return EFI_ABORTED;
      }

      MicroSecondDelay (I2CBusList[Bus].PollingTime);
      Timeout -= MicroSecondDelay (I2CBusList[Bus].PollingTime);
  }

  return EFI_SUCCESS;
}

UINT32
I2CSclHcnt (
  UINT32 IcClk,
  UINT32 tSYMBOL,
  UINT32 Tf,
  UINT32 Spklen,
  INTN   Cond,
  INTN   Offset
  )
{
  /*
   * DesignWare I2C core doesn't seem to have solid strategy to meet
   * the tHD;STA timing spec. Configuring _HCNT. Based on tHIGH spec
   * will result in violation of the tHD;STA spec.
   */
  if (Cond != 0) {
    /*
     * Conditional expression:
     *
     *   IC_[FS]S_SCL_HCNT + (1+4+3) >= IC_CLK * tHIGH
     *
     * This is.Based on the DW manuals, and represents an ideal
     * configuration.  The resulting I2C bus speed will be
     * faster than any of the others.
     *
     * If your hardware is free from tHD;STA issue, try this one.
     */
    return (((IcClk * tSYMBOL + 500000) / 1000000) - 8 + Offset);
  }

  /*
   * Conditional expression:
   *
   * IC_[FS]S_SCL_HCNT + IC_[FH]S_SPKLEN + 6 >= IC_CLK *
   *                                            (tHD;STA + tf)
   *
   * This is just experimental rule; the tHD;STA period turned
   * out to be proportinal to (_HCNT _SPKLEN + 6).
   * With this setting, we could meet both tHIGH and tHD;STA
   * timing specs.
   *
   * If unsure, you'd better to take this alternative.
   *
   * The reason why we need to take into account "tf" here,
   * is the same as described in I2c_Dw_Scl_Lcnt().
   */
  return (((IcClk * (tSYMBOL + Tf) + 500000) / 1000000) -
            Spklen - 6 + Offset);
}

UINT32
I2CSclLcnt (
  UINT32 IcClk,
  UINT32 tLOW,
  UINT32 Tf,
  INTN   Offset
  )
{
  /*
   * Conditional expression:
   *
   *   IC_[FS]S_SCL_LCNT + 1 >= IC_CLK * (tLOW + tf)
   *
   * DW I2C core starts Counting the SCL CNTs for the LOW period
   * of the SCL clock (tLOW) as soon as it pulls the SCL line.
   * In order to meet the tLOW timing spec, we need to take INTNo
   * acCount the fall time of SCL signal (tf).  Default tf value
   * should be 0.3 us, for safety.
   */
  return (((IcClk * (tLOW + Tf) + 500000) / 1000000) - 1 + Offset);
}

/**
 Initialize the designware i2c scl Counts

 This functions configures scl clock Count for SS, FS, and HS.
 **/
VOID
I2CSclInit (
  UINT32 Bus,
  UINT32 I2CClkFreq,
  UINT32 I2CSpeed
  )
{
  UINT32    Hcnt, Lcnt;
  UINT16    IcCon;
  UINTN     Base            = I2CBusList[Bus].Base;
  UINT32    InputClockKhz   = I2CClkFreq / 1000;
  UINT32    SsClockKhz      = InputClockKhz;
  UINT32    FsClockKhz      = InputClockKhz;
  UINT32    HsClockKhz      = InputClockKhz;
  UINT32    PsClockKhz      = InputClockKhz;
  UINT32    I2CSpeedKhz     = I2CSpeed / 1000;

  DBG("Bus %d I2CClkFreq %d I2CSpeed %d\n", Bus, I2CClkFreq, I2CSpeed);
  IcCon = DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE | DW_IC_CON_RESTART_EN;

  if (I2CSpeedKhz <= 100) {
    IcCon |= DW_IC_CON_SPEED_STD;
    SsClockKhz = (SsClockKhz * 100) / I2CSpeedKhz;
    /* Standard speed mode */
    Hcnt = I2CSclHcnt (SsClockKhz,
      I2CSclMin[I2C_SS][I2C_SCL_HIGH],  /* tHD;STA = tHIGH = 4.0 us */
      I2CSclMin[I2C_SS][I2C_SCL_TF],    /* tf = 0.3 us */
      I2CSclParam[I2C_SS][I2C_SPKLEN],  /* spklen = 10 */
      0,                                /* 0: DW default, 1: Ideal */
      I2CSclParam[I2C_SS][I2C_OFFSET]); /* offset = 300 */
    Write32 (Base + DW_IC_FS_SPKLEN, I2CSclParam[I2C_SS][I2C_SPKLEN]);
    Lcnt = I2CSclLcnt (SsClockKhz,
      I2CSclMin[I2C_SS][I2C_SCL_LOW],   /* tLOW = 4.7 us */
      I2CSclMin[I2C_SS][I2C_SCL_TF],    /* tf = 0.3 us */
      0);                               /* No Offset */
    Write32 (Base + DW_IC_SS_SCL_HCNT, Hcnt);
    Write32 (Base + DW_IC_SS_SCL_LCNT, Lcnt);
  } else if (I2CSpeedKhz > 100 && I2CSpeedKhz <= 400) {
    IcCon |= DW_IC_CON_SPEED_FAST;
    FsClockKhz = (FsClockKhz * 400) / I2CSpeedKhz;
    /* Fast speed mode */
    Hcnt = I2CSclHcnt (FsClockKhz,
      I2CSclMin[I2C_FS][I2C_SCL_HIGH],  /* tHD;STA = tHIGH = 0.6 us */
      I2CSclMin[I2C_FS][I2C_SCL_TF],    /* tf = 0.3 us */
      I2CSclParam[I2C_FS][I2C_SPKLEN],  /* spklen = 0xA */
      0,                                /* 0: DW default, 1: Ideal */
     I2CSclParam[I2C_FS][I2C_OFFSET]);  /* No Offset */
    Write32 (Base + DW_IC_FS_SPKLEN, I2CSclParam[I2C_FS][I2C_SPKLEN]);
    Lcnt = I2CSclLcnt (FsClockKhz,
      I2CSclMin[I2C_FS][I2C_SCL_LOW],   /* tLOW = 1.3 us */
      I2CSclMin[I2C_FS][I2C_SCL_TF],    /* tf = 0.3 us */
      0);                               /* No Offset */
    Write32 (Base + DW_IC_FS_SCL_HCNT, Hcnt);
    Write32 (Base + DW_IC_FS_SCL_LCNT, Lcnt);
  } else if (I2CSpeedKhz > 400 && I2CSpeedKhz <= 1000) {
    IcCon |= DW_IC_CON_SPEED_FAST;
    PsClockKhz = (PsClockKhz * 1000) / I2CSpeedKhz;
    /* Fast speed plus mode */
    Hcnt = I2CSclHcnt (PsClockKhz,
      I2CSclMin[I2C_PS][I2C_SCL_HIGH],  /* tHD;STA = tHIGH = 0.26 us */
      I2CSclMin[I2C_PS][I2C_SCL_TF],    /* tf = 0.12 us */
      I2CSclParam[I2C_PS][I2C_SPKLEN],  /* spklen = 0xA */
      0,                                /* 0: DW default, 1: Ideal */
      I2CSclParam[I2C_PS][I2C_OFFSET]); /* No Offset */
    Lcnt = I2CSclLcnt (PsClockKhz,
      I2CSclMin[I2C_PS][I2C_SCL_LOW],   /* tLOW = 0.5 us */
      I2CSclMin[I2C_PS][I2C_SCL_TF],    /* tf = 0.12 us */
      0);  /* No Offset */
    Write32 (Base + DW_IC_FS_SCL_HCNT, Hcnt);
    Write32 (Base + DW_IC_FS_SCL_LCNT, Lcnt);
    Write32 (Base + DW_IC_FS_SPKLEN, I2CSclParam[I2C_PS][I2C_SPKLEN]);
  } else if (I2CSpeedKhz > 1000 && I2CSpeedKhz <= 3400) {
    IcCon |= (DW_IC_CON_SPEED_STD | DW_IC_CON_SPEED_FAST);
    HsClockKhz = (HsClockKhz * 3400) / I2CSpeedKhz;
    /* High speed mode */
    Hcnt = I2CSclHcnt (HsClockKhz,
      I2CSclMin[I2C_HS][I2C_SCL_HIGH],  /* tHD;STA = tHIGH = 0.06 us for 100pf 0.16 for 400pf */
      I2CSclMin[I2C_HS][I2C_SCL_TF],    /* tf = 0.3 us */
      I2CSclParam[I2C_HS][I2C_SPKLEN],  /* No spklen */
      0,                                /* 0: DW default, 1: Ideal */
      I2CSclParam[I2C_HS][I2C_OFFSET]); /* No Offset */
    Lcnt = I2CSclLcnt (HsClockKhz,
      I2CSclMin[I2C_HS][I2C_SCL_LOW],   /* tLOW = 0.12 us for 100pf 0.32 us for 400pf */
      I2CSclMin[I2C_HS][I2C_SCL_TF],    /* tf = 0.3 us */
      0);  /* No Offset */
    Write32 (Base + DW_IC_HS_SCL_HCNT, Hcnt);
    Write32 (Base + DW_IC_HS_SCL_LCNT, Lcnt);
  }
  Write32 (Base + DW_IC_CON, IcCon);
}

/**
 Initialize the designware i2c master hardware
 **/
EFI_STATUS
I2CInit (
  UINT32 Bus,
  UINTN  BusSpeed
  )
{
  UINTN Base;

  ASSERT (I2CClock != 0);

  I2CBusList[Bus].BusSpeed = BusSpeed;
  I2CHWInit (Bus);

  Base = I2CBusList[Bus].Base;

  /* Disable the adapter and interrupt */
  I2CEnable (Bus, 0);
  Write32 (Base + DW_IC_INTR_MASK, 0);

  /* Set standard and fast speed divider for high/low periods */
  I2CSclInit (Bus, I2CClock, BusSpeed);
  Write32 (Base + DW_IC_SDA_HOLD, 0x4b);

  return EFI_SUCCESS;
}

/**
 Wait the transaction finished
 **/
EFI_STATUS
I2CFinish (
  UINT32 Bus
  )
{
  UINTN Base    = I2CBusList[Bus].Base;
  UINTN Timeout = DW_TRANSFER_DATA_TIMEOUT;

  /* Wait for TX FIFO empty */
  do {
    if ((Read32 (Base + DW_IC_STATUS) & DW_IC_STATUS_TFE) != 0) {
      break;
    }
    MicroSecondDelay (I2CBusList[Bus].PollingTime);
    Timeout -= MicroSecondDelay (I2CBusList[Bus].PollingTime);
  } while (Timeout > 0);

  if (Timeout == 0) {
    ERROR ("Timeout waiting for TX FIFO empty\n");
    return EFI_TIMEOUT;
  }

  /* Wait for STOP signal detected on the bus */
  Timeout = DW_TRANSFER_DATA_TIMEOUT;
  do {
    if ((Read32 (Base + DW_IC_RAW_INTR_STAT) & DW_IC_INTR_STOP_DET) != 0) {
      Read32 (Base + DW_IC_CLR_STOP_DET);
      return EFI_SUCCESS;
    }
    MicroSecondDelay (I2CBusList[Bus].PollingTime);
    Timeout -= MicroSecondDelay (I2CBusList[Bus].PollingTime);
  } while (Timeout > 0);

  ERROR ("Timeout waiting for transaction finished\n");
  return EFI_TIMEOUT;
}

EFI_STATUS
InternalI2CWrite (
  UINT32 Bus,
  UINT8  *Buf,
  UINT32 *Length
  )
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINTN      WriteCount;
  UINTN      Base = I2CBusList[Bus].Base;

  DBG ("Write Bus %d Buf %p Length %d\n", Bus, Buf, *Length);
  I2CEnable (Bus, 1);

  WriteCount = 0;
  while ((*Length - WriteCount) != 0) {
    Status = I2CWaitTxData (Bus);
    if (EFI_ERROR (Status)) {
      Write32 (Base + DW_IC_DATA_CMD, DW_IC_DATA_CMD_STOP);
      I2CSync ();
      goto Exit;
    }

    if (WriteCount == *Length - 1) {
      Write32 (Base + DW_IC_DATA_CMD,
                (Buf[WriteCount] & DW_IC_DATA_CMD_DAT_MASK)
                | DW_IC_DATA_CMD_STOP);
    } else {
      Write32 (Base + DW_IC_DATA_CMD,
                Buf[WriteCount] & DW_IC_DATA_CMD_DAT_MASK);
    }
    I2CSync ();
    WriteCount++;
  }

Exit:
  *Length = WriteCount;
  I2CFinish (Bus);
  I2CWaitBusNotBusy (Bus);
  I2CEnable (Bus, 0);

  return Status;
}

EFI_STATUS
InternalI2CRead (
  UINT32      Bus,
  UINT8       *BufCmd,
  IN UINT32   CmdLength,
  UINT8       *Buf,
  UINT32      *Length
  )
{
  UINTN         Base    = I2CBusList[Bus].Base;
  UINT32        CmdSend;
  UINT32        TxLimit, RxLimit;
  UINTN         Idx = 0;
  UINTN         Count = 0;
  UINTN         ReadCount = 0;
  UINTN         WriteCount = 0;
  EFI_STATUS    Status = EFI_SUCCESS;

  DBG ("Read Bus %d Buf %p Length:%d\n", Bus, Buf, *Length);
  I2CEnable (Bus, 1);

  /* Write command data */
  WriteCount = 0;
  while (CmdLength != 0) {
    TxLimit = I2CBusList[Bus].TxFifo - Read32 (Base + DW_IC_TXFLR);
    Count = CmdLength > TxLimit ? TxLimit : CmdLength;

    for (Idx = 0; Idx < Count ; Idx++ ) {
      CmdSend = BufCmd[WriteCount++] & DW_IC_DATA_CMD_DAT_MASK;
      Write32 (Base + DW_IC_DATA_CMD, CmdSend);
      I2CSync ();

      if (I2CCheckErrors (Bus) != 0) {
        Status = EFI_CRC_ERROR;
        goto Exit;
      }
      CmdLength--;
    }

    Status = I2CWaitTxData (Bus);
    if (EFI_ERROR (Status)) {
      Write32 (Base + DW_IC_DATA_CMD, DW_IC_DATA_CMD_STOP);
      I2CSync ();
      goto Exit;
    }
  }

  ReadCount = 0;
  WriteCount = 0;
  while ((*Length - ReadCount) != 0) {
    TxLimit = I2CBusList[Bus].TxFifo - Read32 (Base + DW_IC_TXFLR);
    RxLimit = I2CBusList[Bus].RxFifo - Read32 (Base + DW_IC_RXFLR);
    Count = *Length - ReadCount;
    Count = Count > RxLimit ? RxLimit : Count;
    Count = Count > TxLimit ? TxLimit : Count;

    for (Idx = 0; Idx < Count ; Idx++ ) {
      CmdSend = DW_IC_DATA_CMD_CMD;
      if (WriteCount == *Length - 1) {
        CmdSend |= DW_IC_DATA_CMD_STOP;
      }
      Write32 (Base + DW_IC_DATA_CMD, CmdSend);
      I2CSync ();
      WriteCount++;

      if (I2CCheckErrors (Bus) != 0) {
        DBG ("Sending reading command remaining length %d CRC error\n", *Length);
        Status = EFI_CRC_ERROR;
        goto Exit;
      }
    }

    for (Idx = 0; Idx < Count ; Idx++ ) {
      Status = I2CWaitRxData (Bus);
      if (EFI_ERROR (Status)) {
        DBG ("Reading remaining length %d failed to wait data\n", *Length);

        if (Status != EFI_ABORTED) {
          Write32 (Base + DW_IC_DATA_CMD, DW_IC_DATA_CMD_STOP);
          I2CSync ();
        }

        goto Exit;
      }

      Buf[ReadCount++] = Read32 (Base + DW_IC_DATA_CMD) & DW_IC_DATA_CMD_DAT_MASK;
      I2CSync ();

      if (I2CCheckErrors (Bus) != 0) {
       DBG ("Reading remaining length %d CRC error\n", *Length);
       Status = EFI_CRC_ERROR;
       goto Exit;
      }
    }
  }

Exit:
  *Length = ReadCount;
  I2CFinish (Bus);
  I2CWaitBusNotBusy (Bus);
  I2CEnable (Bus, 0);

  return Status;
}

EFI_STATUS
EFIAPI
I2CWrite (
  IN     UINT32 Bus,
  IN     UINT32 SlaveAddr,
  IN OUT UINT8  *Buf,
  IN OUT UINT32 *WriteLength
  )
{
  if (Bus >= MAX_PLATFORM_I2C_BUS_NUM) {
    return EFI_INVALID_PARAMETER;
  }

  I2CSetSlaveAddr (Bus, SlaveAddr);

  return InternalI2CWrite (Bus, Buf, WriteLength);
}

EFI_STATUS
EFIAPI
I2CRead (
  IN     UINT32 Bus,
  IN     UINT32 SlaveAddr,
  IN     UINT8  *BufCmd,
  IN     UINT32 CmdLength,
  IN OUT UINT8  *Buf,
  IN OUT UINT32 *ReadLength
  )
{
  if (Bus >= MAX_PLATFORM_I2C_BUS_NUM) {
    return EFI_INVALID_PARAMETER;
  }

  I2CSetSlaveAddr (Bus, SlaveAddr);

  return InternalI2CRead (Bus, BufCmd, CmdLength, Buf, ReadLength);
}

EFI_STATUS
EFIAPI
I2CProbe (
  IN UINT32 Bus,
  IN UINTN BusSpeed
  )
{
  if (Bus >= MAX_PLATFORM_I2C_BUS_NUM) {
    return EFI_INVALID_PARAMETER;
  }

  return I2CInit (Bus, BusSpeed);
}

/**
 * Notification function of EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE.
 *
 * This is a notification function registered on EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE event.
 * It convers pointer to new virtual address.
 *
 * @param  Event        Event whose notification function is being invoked.
 * @param  Context      Pointer to the notification function's context.
 */
VOID
EFIAPI
I2cVirtualAddressChangeEvent (
  IN EFI_EVENT                            Event,
  IN VOID                                 *Context
  )
{
  UINTN Count;

  EfiConvertPointer (0x0, (VOID**) &I2CBusList);
  EfiConvertPointer (0x0, (VOID**) &I2CBaseArray);
  EfiConvertPointer (0x0, (VOID**) &I2CClock);
  for (Count = 0; Count < MAX_PLATFORM_I2C_BUS_NUM; Count++) {
    if (!I2CRuntimeEnableArray[Count]) {
      continue;
    }
    EfiConvertPointer (0x0, (VOID**) &I2CBaseArray[Count]);
    EfiConvertPointer (0x0, (VOID**) &I2CBusList[Count].Base);
  }
}

/**
 Setup a bus that to be used in runtime service.

 @Bus:      Bus ID.
 @return:   0 for success.
            Otherwise, error code.
 **/
EFI_STATUS
EFIAPI
I2CSetupRuntime (
  IN UINT32 Bus
  )
{
  EFI_STATUS                        Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR   Descriptor;

  if (mVirtualAddressChangeEvent == NULL) {
    /*
     * Register for the virtual address change event
     */
    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    TPL_NOTIFY,
                    I2cVirtualAddressChangeEvent,
                    NULL,
                    &gEfiEventVirtualAddressChangeGuid,
                    &mVirtualAddressChangeEvent
                    );
    ASSERT_EFI_ERROR (Status);
  }

  Status = gDS->GetMemorySpaceDescriptor (
                  I2CBaseArray[Bus] & RUNTIME_ADDRESS_MASK,
                  &Descriptor
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gDS->SetMemorySpaceAttributes (
                  I2CBaseArray[Bus] & RUNTIME_ADDRESS_MASK,
                  RUNTIME_ADDRESS_LENGTH,
                  Descriptor.Attributes | EFI_MEMORY_RUNTIME
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  I2CRuntimeEnableArray[Bus] = TRUE;

  return Status;
}

EFI_STATUS
EFIAPI
I2CLibConstructor (
  VOID
  )
{
  VOID                *Hob;
  PlatformInfoHob     *PlatformHob;
  PlatformInfoHob_V2  *PlatformHob_V2;

  /* Get I2C Clock from the Platform HOB */
  Hob = GetFirstGuidHob (&gPlatformHobGuid);
  if (Hob == NULL) {
    Hob = GetFirstGuidHob (&gPlatformHobV2Guid);
    if (Hob == NULL) {
      return EFI_NOT_FOUND;
    }
    PlatformHob_V2 = (PlatformInfoHob_V2 *) GET_GUID_HOB_DATA (Hob);
    I2CClock = PlatformHob_V2->AhbClk;
  } else {
    PlatformHob = (PlatformInfoHob *) GET_GUID_HOB_DATA (Hob);
    I2CClock = PlatformHob->ApbClk;
  }
  ASSERT (I2CClock != 0);

  return EFI_SUCCESS;
}
