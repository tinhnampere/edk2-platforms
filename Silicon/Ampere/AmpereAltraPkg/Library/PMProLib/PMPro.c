/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PrintLib.h>
#include <Library/SMProInterface.h>
#include <Library/TimerLib.h>
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

#define DB_MSG_TYPE_SHIFT               28
#define DB_MSG_CONTROL_BYTE_SHIFT       16
#define DB_MSG_CONTROL_BYTE_MASK        0x00FF0000U

/* User message */
#define DB_USER_MSG                     0x6
#define DB_USER_MSG_HNDL_SHIFT          24
#define DB_USER_MSG_HNDL_MASK           0x0F000000
#define DB_MSG_CTRL_BYTE_SHIFT          16
#define DB_MSG_CTRL_BYTE_MASK           0x00FF0000
#define DB_USER_MSG_P0_SHIFT            8
#define DB_USER_MSG_P0_MASK             0x0000FF00
#define DB_USER_MSG_P1_MASK             0x000000FF
#define DB_ENCODE_USER_MSG(hndl, cb, p0, p1)    \
          ((DB_USER_MSG << DB_MSG_TYPE_SHIFT) | \
          (((hndl) << DB_USER_MSG_HNDL_SHIFT) & DB_USER_MSG_HNDL_MASK) | \
          (((cb) << DB_MSG_CTRL_BYTE_SHIFT) & DB_MSG_CTRL_BYTE_MASK) |   \
          (((p0) << DB_USER_MSG_P0_SHIFT) & DB_USER_MSG_P0_MASK) |       \
          ((p1) & DB_USER_MSG_P1_MASK))
#define DB_CONFIG_SET_HDLR              2
#define DB_TURBO_CMD                    20
#define DB_TURBO_ENABLE_SUBCMD          0

#define MB_POLL_INTERVALus              1000
#define MB_TIMEOUTus                    10000000

STATIC
UINT64
PMproGetDBBase (
  UINT8  Socket,
  UINT64 Base
  )
{
  return Base + SOCKET_BASE_OFFSET * Socket;
}

EFI_STATUS
EFIAPI
PMProDBWr (
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
    if (--TimeoutCnt == 0) {
      return EFI_TIMEOUT;
    }
  }

  /* Clear iPP ack */
  MmioWrite32 (MsgReg + IntStatOffset, DB_ACK_MASK);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PMProTurboEnable (
  UINT8 Socket,
  UINT8 Enable
  )
{
  UINT32 Msg;

  Msg = DB_ENCODE_USER_MSG (
          DB_CONFIG_SET_HDLR,
          0,
          DB_TURBO_CMD,
          DB_TURBO_ENABLE_SUBCMD
          );

  return PMProDBWr (
           PMPRO_DB,
           Msg,
           Enable,
           0,
           PMproGetDBBase (Socket, PMPRO_DB_BASE_REG)
           );
}
