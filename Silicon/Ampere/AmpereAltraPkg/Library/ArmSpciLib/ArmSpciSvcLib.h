/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef ARM_SPCI_SVC_LIB_H_
#define ARM_SPCI_SVC_LIB_H_

#define SMCCC_VERSION_MAJOR_SHIFT       16
#define SMCCC_VERSION_MAJOR_MASK        0x7FFF
#define SMCCC_VERSION_MINOR_SHIFT       0
#define SMCCC_VERSION_MINOR_MASK        0xFFFF
#define MAKE_SMCCC_VERSION(_major, _minor) \
  ((((UINT32)(_major) & SMCCC_VERSION_MAJOR_MASK) << \
    SMCCC_VERSION_MAJOR_SHIFT) \
   | (((UINT32)(_minor) & SMCCC_VERSION_MINOR_MASK) << \
      SMCCC_VERSION_MINOR_SHIFT))

#define SMC_UNKNOWN                     -1

/*******************************************************************************
 * Bit definitions inside the function id as per the SMC calling convention
 ******************************************************************************/
#define FUNCID_TYPE_SHIFT               31
#define FUNCID_CC_SHIFT                 30
#define FUNCID_OEN_SHIFT                24
#define FUNCID_NUM_SHIFT                0

#define FUNCID_TYPE_MASK                0x1
#define FUNCID_CC_MASK                  0x1
#define FUNCID_OEN_MASK                 0x3f
#define FUNCID_NUM_MASK                 0xffff

#define FUNCID_TYPE_WIDTH               1
#define FUNCID_CC_WIDTH                 1
#define FUNCID_OEN_WIDTH                6
#define FUNCID_NUM_WIDTH                16

#define SMC_64                          1
#define SMC_32                          0
#define SMC_TYPE_FAST                   1
#define SMC_TYPE_STD                    0

/*******************************************************************************
 * Owning entity number definitions inside the function id as per the SMC
 * calling convention
 ******************************************************************************/
#define OEN_ARM_START                   0
#define OEN_ARM_END                     0
#define OEN_CPU_START                   1
#define OEN_CPU_END                     1
#define OEN_SIP_START                   2
#define OEN_SIP_END                     2
#define OEN_OEM_START                   3
#define OEN_OEM_END                     3
#define OEN_STD_START                   4       /* Standard Calls */
#define OEN_STD_END                     4
#define OEN_TAP_START                   48      /* Trusted Applications */
#define OEN_TAP_END                     49
#define OEN_TOS_START                   50      /* Trusted OS */
#define OEN_TOS_END                     63
#define OEN_LIMIT                       64

/* SPCI_VERSION helpers */
#define SPCI_VERSION_MAJOR              0
#define SPCI_VERSION_MAJOR_SHIFT        16
#define SPCI_VERSION_MAJOR_MASK         0x7FFF
#define SPCI_VERSION_MINOR              1
#define SPCI_VERSION_MINOR_SHIFT        0
#define SPCI_VERSION_MINOR_MASK         0xFFFF
#define SPCI_VERSION_FORM(major, minor) ((((major) & SPCI_VERSION_MAJOR_MASK)  \
                                          << SPCI_VERSION_MAJOR_SHIFT) | \
                                         ((minor) & SPCI_VERSION_MINOR_MASK))
#define SPCI_VERSION_COMPILED           SPCI_VERSION_FORM ( \
                                          SPCI_VERSION_MAJOR, \
                                          SPCI_VERSION_MINOR \
                                          )

/* Definitions to build the complete SMC ID */
#define SPCI_FID_MISC_FLAG              (0 << 27)
#define SPCI_FID_MISC_SHIFT             20
#define SPCI_FID_MISC_MASK              0x7F

#define SPCI_FID_TUN_FLAG               (1 << 27)
#define SPCI_FID_TUN_SHIFT              24
#define SPCI_FID_TUN_MASK               0x7

#define OEN_SPCI_START                  0x30
#define OEN_SPCI_END                    0x3F

#define SPCI_SMC(spci_fid)      ((OEN_SPCI_START << FUNCID_OEN_SHIFT) | \
                                 (1 << 31) | (spci_fid))
#define SPCI_MISC_32(misc_fid)  ((SMC_32 << FUNCID_CC_SHIFT) |  \
                                 SPCI_FID_MISC_FLAG |           \
                                 SPCI_SMC ((misc_fid) << SPCI_FID_MISC_SHIFT))
#define SPCI_MISC_64(misc_fid)  ((SMC_64 << FUNCID_CC_SHIFT) |  \
                                 SPCI_FID_MISC_FLAG |           \
                                 SPCI_SMC ((misc_fid) << SPCI_FID_MISC_SHIFT))
#define SPCI_TUN_32(tun_fid)    ((SMC_32 << FUNCID_CC_SHIFT) |  \
                                 SPCI_FID_TUN_FLAG |            \
                                 SPCI_SMC ((tun_fid) << SPCI_FID_TUN_SHIFT))
#define SPCI_TUN_64(tun_fid)    ((SMC_64 << FUNCID_CC_SHIFT) |  \
                                 SPCI_FID_TUN_FLAG |            \
                                 SPCI_SMC ((tun_fid) << SPCI_FID_TUN_SHIFT))

/* SPCI miscellaneous functions */
#define SPCI_FID_VERSION                        0x0
#define SPCI_FID_SERVICE_HANDLE_OPEN            0x2
#define SPCI_FID_SERVICE_HANDLE_CLOSE           0x3
#define SPCI_FID_SERVICE_MEM_REGISTER           0x4
#define SPCI_FID_SERVICE_MEM_UNREGISTER         0x5
#define SPCI_FID_SERVICE_MEM_PUBLISH            0x6
#define SPCI_FID_SERVICE_REQUEST_BLOCKING       0x7
#define SPCI_FID_SERVICE_REQUEST_START          0x8
#define SPCI_FID_SERVICE_GET_RESPONSE           0x9
#define SPCI_FID_SERVICE_RESET_CLIENT_STATE     0xA

/* SPCI tunneling functions */
#define SPCI_FID_SERVICE_TUN_REQUEST_START      0x0
#define SPCI_FID_SERVICE_REQUEST_RESUME         0x1
#define SPCI_FID_SERVICE_TUN_REQUEST_BLOCKING   0x2

#define SPCI_SERVICE_HANDLE_OPEN_NOTIFY_BIT     1

/* Complete SMC IDs and associated values */
#define SPCI_VERSION                            SPCI_MISC_32 (SPCI_FID_VERSION)

#define SPCI_SERVICE_HANDLE_OPEN                \
                            SPCI_MISC_32 (SPCI_FID_SERVICE_HANDLE_OPEN)

#define SPCI_SERVICE_HANDLE_CLOSE               \
                            SPCI_MISC_32 (SPCI_FID_SERVICE_HANDLE_CLOSE)

#define SPCI_SERVICE_MEM_REGISTER_AARCH32       \
                            SPCI_MISC_32 (SPCI_FID_SERVICE_MEM_REGISTER)
#define SPCI_SERVICE_MEM_REGISTER_AARCH64       \
                            SPCI_MISC_64 (SPCI_FID_SERVICE_MEM_REGISTER)

#define SPCI_SERVICE_MEM_UNREGISTER_AARCH32     \
                            SPCI_MISC_32 (SPCI_FID_SERVICE_MEM_UNREGISTER)
#define SPCI_SERVICE_MEM_UNREGISTER_AARCH64     \
                            SPCI_MISC_64 (SPCI_FID_SERVICE_MEM_UNREGISTER)

#define SPCI_SERVICE_MEM_PUBLISH_AARCH32        \
                            SPCI_MISC_32 (SPCI_FID_SERVICE_MEM_PUBLISH)
#define SPCI_SERVICE_MEM_PUBLISH_AARCH64        \
                            SPCI_MISC_64 (SPCI_FID_SERVICE_MEM_PUBLISH)

#define SPCI_SERVICE_REQUEST_BLOCKING_AARCH32   \
                            SPCI_MISC_32 (SPCI_FID_SERVICE_REQUEST_BLOCKING)
#define SPCI_SERVICE_REQUEST_BLOCKING_AARCH64   \
                            SPCI_MISC_64 (SPCI_FID_SERVICE_REQUEST_BLOCKING)

#define SPCI_SERVICE_REQUEST_START_AARCH32       \
                            SPCI_MISC_32 (SPCI_FID_SERVICE_REQUEST_START)
#define SPCI_SERVICE_REQUEST_START_AARCH64      \
                            SPCI_MISC_64 (SPCI_FID_SERVICE_REQUEST_START)

#define SPCI_SERVICE_GET_RESPONSE_AARCH32       \
                            SPCI_MISC_32 (SPCI_FID_SERVICE_GET_RESPONSE)
#define SPCI_SERVICE_GET_RESPONSE_AARCH64       \
                            SPCI_MISC_64 (SPCI_FID_SERVICE_GET_RESPONSE)

#define SPCI_SERVICE_RESET_CLIENT_STATE_AARCH32 \
                            SPCI_MISC_32 (SPCI_FID_SERVICE_RESET_CLIENT_STATE)
#define SPCI_SERVICE_RESET_CLIENT_STATE_AARCH64 \
                            SPCI_MISC_64 (SPCI_FID_SERVICE_RESET_CLIENT_STATE)

#define SPCI_SERVICE_TUN_REQUEST_START_AARCH32  \
                            SPCI_TUN_32 (SPCI_FID_SERVICE_TUN_REQUEST_START)
#define SPCI_SERVICE_TUN_REQUEST_START_AARCH64  \
                            SPCI_TUN_64 (SPCI_FID_SERVICE_TUN_REQUEST_START)

#define SPCI_SERVICE_REQUEST_RESUME_AARCH32     \
                            SPCI_TUN_32 (SPCI_FID_SERVICE_REQUEST_RESUME)
#define SPCI_SERVICE_REQUEST_RESUME_AARCH64     \
                            SPCI_TUN_64 (SPCI_FID_SERVICE_REQUEST_RESUME)

#define SPCI_SERVICE_TUN_REQUEST_BLOCKING_AARCH32  \
                            SPCI_TUN_32 (SPCI_FID_SERVICE_TUN_REQUEST_BLOCKING)
#define SPCI_SERVICE_TUN_REQUEST_BLOCKING_AARCH64  \
                            SPCI_TUN_64 (SPCI_FID_SERVICE_TUN_REQUEST_BLOCKING)

#endif
