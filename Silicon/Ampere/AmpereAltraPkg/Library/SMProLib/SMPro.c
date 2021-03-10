/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/ArmLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TimerLib.h>
#include <Library/SMProInterface.h>
#include <Platform/Ac01.h>

/* Mailbox Door Bell */
#define DBMSG_REG_STRIDE                0x1000
#define DB_STATUS_ADDR                  0x00000020
#define DB_DIN_ADDR                     0x00000000
#define DB_DIN0_ADDR                    0x00000004
#define DB_DIN1_ADDR                    0x00000008
#define DB_AVAIL_MASK                   0x00010000
#define DB_OUT_ADDR                     0x00000010
#define DB_DOUT0_ADDR                   0x00000014
#define DB_DOUT1_ADDR                   0x00000018
#define DB_ACK_MASK                     0x00000001

/* RAS message */
#define IPP_RAS_MSG_HNDL_MASK           0x0F000000U
#define IPP_RAS_MSG_HNDL_SHIFT          24
#define IPP_RAS_MSG_CMD_MASK            0x00F00000U
#define IPP_RAS_MSG_CMD_SHIFT           20
#define IPP_RAS_MSG_HDLR                1
#define IPP_RAS_MSG                     0xB
#define IPP_RAS_MSG_HDLR                1
#define IPP_RAS_MSG_SETUP_CHECK         1 /* Setup RAS check polling */
#define IPP_RAS_MSG_START               2 /* Start RAS polling */
#define IPP_RAS_MSG_STOP                3 /* Stop RAS polling */

#define IPP_MSG_TYPE_SHIFT              28
#define IPP_MSG_CONTROL_BYTE_SHIFT      16
#define IPP_MSG_CONTROL_BYTE_MASK       0x00FF0000U

#define IPP_ENCODE_RAS_MSG(cmd, cb)     \
                (((UINT32) (IPP_RAS_MSG) << IPP_MSG_TYPE_SHIFT) | \
                ((IPP_RAS_MSG_HDLR << IPP_RAS_MSG_HNDL_SHIFT) & IPP_RAS_MSG_HNDL_MASK) | \
                (((cb) << IPP_MSG_CONTROL_BYTE_SHIFT) & IPP_MSG_CONTROL_BYTE_MASK) | \
                (((cmd) << IPP_RAS_MSG_CMD_SHIFT) & IPP_RAS_MSG_CMD_MASK))
#define IPP_DECODE_RAS_MSG_HNDL(data)   \
                (((data) & IPP_RAS_MSG_HNDL_MASK) >> IPP_RAS_MSG_HNDL_SHIFT)
#define IPP_DECODE_RAS_MSG_CMD(data)    \
                (((data) & IPP_RAS_MSG_CMD_MASK) >> IPP_RAS_MSG_CMD_SHIFT)
#define IPP_DECODE_RAS_MSG_CB(cb)       \
                (((cb) & IPP_MSG_CONTROL_BYTE_MASK) >> IPP_MSG_CONTROL_BYTE_SHIFT)

#define MB_POLL_INTERVALus              1000
#define MB_READ_DELAYus                 1000
#define MB_TIMEOUTus                    10000000

UINT64
SMProGetDBBase (
  UINT8  Socket,
  UINT64 Base
  )
{
  return Base + SOCKET_BASE_OFFSET * Socket;
}

/*
 * Read a message from doorbell interface
 */
EFI_STATUS
EFIAPI
SMProDBRd (
  UINT8  Db,
  UINT32 *Data,
  UINT32 *Param,
  UINT32 *Param1,
  UINT64 MsgReg
  )
{
  INTN   TimeoutCnt = MB_TIMEOUTus / MB_POLL_INTERVALus;
  UINT32 DBBaseOffset = Db * DBMSG_REG_STRIDE;

  /* Wait for message since we don't operate in interrupt mode */
  while ((MmioRead32 ((MsgReg + DBBaseOffset + DB_STATUS_ADDR)) & DB_AVAIL_MASK) == 0) {
    MicroSecondDelay (MB_POLL_INTERVALus);
    if (--TimeoutCnt == 0) {
      return EFI_TIMEOUT;
    }
  }

  /* Read iPP message */
  if (Param != NULL) {
    *Param = MmioRead32 (MsgReg + DBBaseOffset + DB_DIN0_ADDR);
  }
  if (Param1 != NULL) {
    *Param1 = MmioRead32 (MsgReg + DBBaseOffset + DB_DIN1_ADDR);
  }
  *Data = MmioRead32 (MsgReg + DBBaseOffset + DB_DIN_ADDR);

  /* Send acknowledgment */
  MmioWrite32 (MsgReg + DBBaseOffset + DB_STATUS_ADDR, DB_AVAIL_MASK);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SMProDBWr (
  UINT8  Db,
  UINT32 Data,
  UINT32 Param,
  UINT32 Param1,
  UINT64 MsgReg
  )
{
  INTN   TimeoutCnt = MB_TIMEOUTus / MB_POLL_INTERVALus;
  UINT32 IntStatOffset;
  UINT32 PCodeOffset;
  UINT32 ScratchOffset;
  UINT32 Scratch1Offset;

  ScratchOffset = (Db * DBMSG_REG_STRIDE) + DB_DOUT0_ADDR;
  Scratch1Offset = (Db *DBMSG_REG_STRIDE) + DB_DOUT1_ADDR;
  PCodeOffset = (Db * DBMSG_REG_STRIDE) + DB_OUT_ADDR;
  IntStatOffset = (Db * DBMSG_REG_STRIDE) + DB_STATUS_ADDR;

  /* Clear previous pending ack if any */
  if ((MmioRead32 ((MsgReg + IntStatOffset)) & DB_ACK_MASK) != 0) {
    MmioWrite32 ((MsgReg + IntStatOffset), DB_ACK_MASK);
  }

  /* Send message */
  MmioWrite32 ((MsgReg + ScratchOffset), Param);
  MmioWrite32 ((MsgReg + Scratch1Offset), Param1);
  MmioWrite32 ((MsgReg + PCodeOffset), Data);

  /* Wait for ack */
  while ((MmioRead32 (MsgReg + IntStatOffset) & DB_ACK_MASK) == 0) {
    MicroSecondDelay (MB_POLL_INTERVALus);
    if (--TimeoutCnt == 0)
      return EFI_TIMEOUT;
  }

  /* Clear iPP ack */
  MmioWrite32 (MsgReg + IntStatOffset, DB_ACK_MASK);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SMProRegRd (
  UINT8  Socket,
  UINT64 Addr,
  UINT32 *Value
  )
{
  UINT32      Data0 = (UINT32) Addr;
  UINT32      Upper = (UINT32) (Addr >> 32);
  UINT32      Data1 = 0;
  UINT32      Msg;
  EFI_STATUS  Status;

  Msg = IPP_ENCODE_DEBUG_MSG (
          IPP_DBG_SUBTYPE_REGREAD,
          0,
          (Upper >> 8) & 0xFF,
          Upper & 0xFF
          );
  Status = SMProDBWr (
             SMPRO_DB,
             Msg,
             Data0,
             Data1,
             SMProGetDBBase (Socket, SMPRO_DB_BASE_REG)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SMProDBRd (
             SMPRO_DB,
             &Msg,
             &Data0,
             &Data1,
             SMProGetDBBase (Socket, SMPRO_DB_BASE_REG)
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((Msg & IPP_DBGMSG_P0_MASK) == 0) {
    return EFI_UNSUPPORTED;
  }

  if (Value != NULL) {
    *Value = Data0;
  }

  return Status;
}

EFI_STATUS
EFIAPI
SMProRegWr (
  UINT8  Socket,
  UINT64 Addr,
  UINT32 Value
  )
{
  UINT32 Data0 = (UINT32) Addr;
  UINT32 Upper = (UINT32) (Addr >> 32);
  UINT32 Data1 = Value;
  UINT32 Msg;

  Msg = IPP_ENCODE_DEBUG_MSG (
          IPP_DBG_SUBTYPE_REGWRITE,
          0,
          (Upper >> 8) & 0xFF,
          Upper & 0xFF
          );

  return SMProDBWr (
           SMPRO_DB,
           Msg,
           Data0,
           Data1,
           SMProGetDBBase (Socket, SMPRO_DB_BASE_REG)
           );
}
