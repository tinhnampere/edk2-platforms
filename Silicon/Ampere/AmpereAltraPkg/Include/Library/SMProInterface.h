/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SMPRO_INTERFACE_H_
#define SMPRO_INTERFACE_H_

#define IPP_ENCODE_OK_MSG               (1 << 28)

/* SMPro common definition */
#define SMPRO_MSG_TYPE_SHIFT            28
#define SMPRO_DEBUG_MSG                 0
#define SMPRO_USER_MSG                  6

#define SMPRO_DBGMSG_TYPE_SHIFT         24
#define SMPRO_DBGMSG_TYPE_MASK          0x0F000000U

#define SMPRO_USER_TYPE_SHIFT           24
#define SMPRO_USER_TYPE_MASK            0x0F000000U

/* SMPro I2C message encode */
#define SMPRO_I2C_PROTOCOL              0
#define SMPRO_SMB_PROTOCOL              1

#define SMPRO_I2C_RD                    0
#define SMPRO_I2C_WR                    1

#define SMPRO_DBG_SUBTYPE_I2C1READ      4
#define SMPRO_I2C_DEV_SHIFT             23
#define SMPRO_I2C_DEV_MASK              0x00800000
#define SMPRO_I2C_DEVID_SHIFT           13
#define SMPRO_I2C_DEVID_MASK            0x007FE000
#define SMPRO_I2C_RW_SHIFT              12
#define SMPRO_I2C_RW_MASK               0x00001000
#define SMPRO_I2C_PROTO_SHIFT           11
#define SMPRO_I2C_PROTO_MASK            0x00000800
#define SMPRO_I2C_ADDRLEN_SHIFT         8
#define SMPRO_I2C_ADDRLEN_MASK          0x00000700
#define SMPRO_I2C_DATALEN_SHIFT         0
#define SMPRO_I2C_DATALEN_MASK          0x000000FF

#define SMPRO_I2C_ENCODE_MSG(dev, chip, op, proto, addrlen, datalen) \
                ((SMPRO_DEBUG_MSG << SMPRO_MSG_TYPE_SHIFT) | \
                ((SMPRO_DBG_SUBTYPE_I2C1READ << SMPRO_DBGMSG_TYPE_SHIFT) & \
                SMPRO_DBGMSG_TYPE_MASK) | \
                ((dev << SMPRO_I2C_DEV_SHIFT) & SMPRO_I2C_DEV_MASK) | \
                ((chip << SMPRO_I2C_DEVID_SHIFT) & SMPRO_I2C_DEVID_MASK) | \
                ((op << SMPRO_I2C_RW_SHIFT) & SMPRO_I2C_RW_MASK) | \
                ((proto << SMPRO_I2C_PROTO_SHIFT) & SMPRO_I2C_PROTO_MASK) | \
                ((addrlen << SMPRO_I2C_ADDRLEN_SHIFT) & SMPRO_I2C_ADDRLEN_MASK) | \
                ((datalen << SMPRO_I2C_DATALEN_SHIFT) & SMPRO_I2C_DATALEN_MASK))

#define SMPRO_I2C_ENCODE_FLAG_BUFADDR           0x80000000
#define SMPRO_I2C_ENCODE_UPPER_DATABUF(a)       \
                        ((UINT32)(((UINT64)(a) >> 12) & 0x3FF00000))
#define SMPRO_I2C_ENCODE_LOWER_DATABUF(a)       \
                        ((UINT32)((UINT64)(a) & 0xFFFFFFFF))
#define SMPRO_I2C_ENCODE_DATAADDR(a)            ((UINT64)(a) & 0xFFFF)

#define SMPRO_USER_MSG_P0_SHIFT           8
#define SMPRO_USER_MSG_P0_MASK            0x0000FF00U
#define SMPRO_USER_MSG_P1_SHIFT           0
#define SMPRO_USER_MSG_P1_MASK            0x000000FFU

/* SMPro Boot Process message encode */
#define SMPRO_USER_SUBTYPE_BOOTPROCESS    6
#define SMPRO_BOOT_PROCESS_ENCODE_MSG(msg1,msg2) \
                ((SMPRO_USER_MSG << SMPRO_MSG_TYPE_SHIFT) | \
                ((SMPRO_USER_SUBTYPE_BOOTPROCESS << SMPRO_USER_TYPE_SHIFT) & \
                SMPRO_USER_TYPE_MASK) | \
                (((msg1) << SMPRO_USER_MSG_P0_SHIFT) & SMPRO_USER_MSG_P0_MASK) | \
                (((msg2) << SMPRO_USER_MSG_P1_SHIFT) & SMPRO_USER_MSG_P1_MASK))

#define SMPRO_USER_SUBTYPE_RNG            7
#define SMPRO_RNG_ENCODE_MSG(msg1,msg2) \
                ((SMPRO_USER_MSG << SMPRO_MSG_TYPE_SHIFT) | \
                ((SMPRO_USER_SUBTYPE_RNG << SMPRO_USER_TYPE_SHIFT) & \
                SMPRO_USER_TYPE_MASK) | \
                (((msg1) << SMPRO_USER_MSG_P0_SHIFT) & SMPRO_USER_MSG_P0_MASK) | \
                (((msg2) << SMPRO_USER_MSG_P1_SHIFT) & SMPRO_USER_MSG_P1_MASK))

#define IPP_DBGMSG_TYPE_SHIFT           24
#define IPP_DBGMSG_TYPE_MASK            0x0F000000
#define IPP_DEBUG_MSG                   0x0
#define IPP_DBG_SUBTYPE_REGREAD         0x1
#define IPP_DBG_SUBTYPE_REGWRITE        0x2
#define IPP_DBGMSG_P0_MASK              0x0000FF00
#define IPP_DBGMSG_P0_SHIFT             8
#define IPP_DBGMSG_P1_MASK              0x000000FF
#define IPP_DBGMSG_P1_SHIFT             0
#define IPP_ENCODE_DEBUG_MSG(type, cb, p0, p1) \
                ((IPP_DEBUG_MSG << IPP_MSG_TYPE_SHIFT) | (((type) << IPP_DBGMSG_TYPE_SHIFT) & \
                IPP_DBGMSG_TYPE_MASK) | (((cb) << IPP_MSG_CONTROL_BYTE_SHIFT) & \
                IPP_MSG_CONTROL_BYTE_MASK) | (((p0) << IPP_DBGMSG_P0_SHIFT) & \
                IPP_DBGMSG_P0_MASK) | (((p1) << IPP_DBGMSG_P1_SHIFT) & IPP_DBGMSG_P1_MASK))

#endif /* SMPRO_INTERFACE_H_ */
