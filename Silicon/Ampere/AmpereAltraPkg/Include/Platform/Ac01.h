/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PLATFORM_AC01_H_
#define __PLATFORM_AC01_H_

/* Number of supported sockets in the platform */
#define PLATFORM_CPU_MAX_SOCKET             2

/* Maximum number of CPMs in the chip. */
#define PLATFORM_CPU_MAX_CPM                (FixedPcdGet32 (PcdClusterCount))

/* Number of cores per CPM. */
#define PLATFORM_CPU_NUM_CORES_PER_CPM      (FixedPcdGet32(PcdCoreCount) / PLATFORM_CPU_MAX_CPM)

/* Socket bit offset of core UID. */
#define PLATFORM_SOCKET_UID_BIT_OFFSET      16

/* CPM bit offset of core UID. */
#define PLATFORM_CPM_UID_BIT_OFFSET         8

/* Maximum number of system locality supported. */
#define PLATFORM_MAX_NUM_ACPI_SYSTEM_LOCALITIES    2

/* Default turbo frequency. */
#define TURBO_DEFAULT_FREQ             3350000000

/* Maximum number of cores supported. */
#define PLATFORM_CPU_MAX_NUM_CORES  (PLATFORM_CPU_MAX_SOCKET * PLATFORM_CPU_MAX_CPM * PLATFORM_CPU_NUM_CORES_PER_CPM)

/* Maximum number of memory region */
#define PLATFORM_DRAM_INFO_MAX_REGION  16

/* Maximum number of DDR slots supported */
#define PLATFORM_DIMM_INFO_MAX_SLOT    32

/* Maximum number of memory supported. */
#define PLATFORM_MAX_MEMORY_REGION     4

/* Maximum number of GIC ITS supported. */
#define PLATFORM_MAX_NUM_GIC_ITS       1

/* The first SPI interrupt number of the Slave socket */
#define PLATFORM_SLAVE_SOCKET_SPI_INTERRUPT_START  352

/* **PORTING NEEDED** Total number of Super IO serial ports present. */
#define TOTAL_SIO_SERIAL_PORTS         1

#define SerialIo_SUPPORT               0

/* The base register of AHBC */
#define AHBC_REGISTER_BASE             0x1f10c000

/* The base address of UART0 Register */
#define UART0_REGISTER_BASE            0x12600000

/* The base address of UART1 Register */
#define UART1_REGISTER_BASE            0x12610000

/* PCC Configuration */
#define SMPRO_MAX_DB                   8
#define SMPRO_DB0_IRQ_NUM              40

#define PMPRO_MAX_DB                   8
#define PMPRO_DB0_IRQ_NUM              56

/* Non-secure Doorbell Mailbox to use between ARMv8 and SMpro */
//#define SMPRO_NS_MAILBOX_INDEX  1

/* Non-secure Doorbell Mailbox to use between ARMv8 and SMpro */
#define SMPRO_NS_RNG_MAILBOX_INDEX     6

#define PCC_MAX_SUBSPACES_PER_SOCKET   (SMPRO_MAX_DB + PMPRO_MAX_DB)
#define PCC_SUBSPACE_MASK              0xEFFFEFFF

#define DB_OUT                         0x00000010
#define DB_OUT0                        0x00000014
#define DB_OUT1                        0x00000018
#define DB_STATUS                      0x00000020
#define DB_STATUSMASK                  0x00000024
#define DB_AVAIL_MASK                  0x00010000
#define DBx_BASE_OFFSET                0x00001000

/* Doorbell to use between ARMv8 and SMpro */
#define SMPRO_DB                       0
#define PMPRO_DB                       1
#define SMPRO_DB_BASE_REG              (FixedPcdGet64 (PcdSmproDbBaseReg))
#define PMPRO_DB_BASE_REG              (FixedPcdGet64 (PcdPmproDbBaseReg))
#define SMPRO_EFUSE_SHADOW0            (FixedPcdGet64 (PcdSmproEfuseShadow0))
#define SMPRO_NS_MAILBOX_INDEX         (FixedPcdGet32 (PcdSmproNsMailboxIndex))
#define SMPRO_I2C_BMC_BUS_ADDR         (FixedPcdGet32 (PcdSmproI2cBmcBusAddr))

#define SOCKET_BASE_OFFSET             0x400000000000
#define SMPRO_DBx_REG(socket, db, reg) \
        ((socket) * SOCKET_BASE_OFFSET + SMPRO_DB_BASE_REG + DBx_BASE_OFFSET * (db) + reg)

#define PMPRO_DBx_REG(socket, db, reg) \
        ((socket) * SOCKET_BASE_OFFSET + PMPRO_DB_BASE_REG + DBx_BASE_OFFSET * (db) + reg)

#define PCC_MAX_SUBSPACES              ((SMPRO_MAX_DB + PMPRO_MAX_DB) * PLATFORM_CPU_MAX_SOCKET)
#define PCC_SUBSPACE_SHARED_MEM_SIZE   0x4000

#define PCC_NOMINAL_LATENCY                  10000   /* 10 ms */
#define PCC_CPPC_NOMINAL_LATENCY             1000    /* 1 ms */
#define PCC_MAX_PERIOD_ACCESS                0       /* unlimited */
#define PCC_MIN_REQ_TURNAROUND_TIME          0       /* unlimited */
#define PCC_CMD_POLL_UDELAY                  10      /* us */
#define PCC_CPPC_MIN_REQ_TURNAROUND_TIME     110      /* 110 us */

#define PCC_SIGNATURE_MASK                   0x50424300
#define PCC_CPPC_SUBSPACE                    2      /* Doorbell 2 of PMPro */
#define PCC_MSG                              0x53000040
#define PCC_CPPC_MSG                         0x00000100
#define PCC_CPPC_URG_MSG                     0x00800000
#define PCC_256_ALIGN_ADDR                   0x00000040
#define PCC_MSG_SIZE                         12      /* Num of Bytes */
#define PCP_MSG_UPPER_ADDR_MASK              0xF


/* The Array of Soc Gpio Base Address */
#define GPIO_DWAPB_BASE_ADDR      0x1000026f0000,0x1000026e0000,0x1000027b0000,0x1000026d0000,0x5000026f0000,0x5000026e0000,0x5000027b0000,0x5000026d0000

/* The Array of Soc Gpi Base Address */
#define GPI_DWAPB_BASE_ADDR      0x1000026d0000,0x5000026d0000

/* Number of Pins Per Each Contoller */
#define GPIO_DWAPB_PINS_PER_CONTROLLER    8

/* Number of Pins Each Socket */
#define GPIO_DWAPB_PINS_PER_SOCKET        32

/* The maximum number of I2C bus */
#define MAX_PLATFORM_I2C_BUS_NUM          2

/* The base address of DW I2C */
#define PLATFORM_I2C_REGISTER_BASE        0x1000026B0000ULL, 0x100002750000ULL

/* Offset of failsafe testing feature */
#define NV_UEFI_FAILURE_FAILSAFE_OFFSET   0x1F8

/* Maximum number of memory controller supports NVDIMM-N per socket */
#define PLATFORM_NVDIMM_MCU_MAX_PER_SK    2

/* Maximum number of NVDIMM-N per memory controller */
#define PLATFORM_NVDIMM_NUM_MAX_PER_MCU   1

/* Maximum number of NVDIMM region per socket */
#define PLATFORM_NVDIMM_REGION_MAX_PER_SK 2

/* Socket 0 base address of NVDIMM non-hashed region 0 */
#define PLATFORM_NVDIMM_SK0_NHASHED_REGION0     0x0B0000000000ULL

/* Socket 0 base address of NVDIMM non-hashed region 1 */
#define PLATFORM_NVDIMM_SK0_NHASHED_REGION1     0x0F0000000000ULL

/* Socket 1 base address of NVDIMM non-hashed region 0 */
#define PLATFORM_NVDIMM_SK1_NHASHED_REGION0     0x430000000000ULL

/* Socket 1 base address of NVDIMM non-hashed region 1 */
#define PLATFORM_NVDIMM_SK1_NHASHED_REGION1     0x470000000000ULL

/* DIMM ID of NVDIMM-N device 1 */
#define PLATFORM_NVDIMM_NVD1_DIMM_ID            6

/* DIMM ID of NVDIMM-N device 2 */
#define PLATFORM_NVDIMM_NVD2_DIMM_ID            14

/* DIMM ID of NVDIMM-N device 3 */
#define PLATFORM_NVDIMM_NVD3_DIMM_ID            22

/* DIMM ID of NVDIMM-N device 4 */
#define PLATFORM_NVDIMM_NVD4_DIMM_ID            30

/* NFIT device handle of NVDIMM-N device 1 */
#define PLATFORM_NVDIMM_NVD1_DEVICE_HANDLE      0x0330

/* NFIT device handle of NVDIMM-N device 2 */
#define PLATFORM_NVDIMM_NVD2_DEVICE_HANDLE      0x0770

/* NFIT device handle of NVDIMM-N device 3 */
#define PLATFORM_NVDIMM_NVD3_DEVICE_HANDLE      0x1330

/* NFIT device handle of NVDIMM-N device 4 */
#define PLATFORM_NVDIMM_NVD4_DEVICE_HANDLE      0x1770

/* Interleave ways of non-hashed NVDIMM-N */
#define PLATFORM_NVDIMM_NHASHED_INTERLEAVE_WAYS 1

/* Interleave ways of hashed NVDIMM-N */
#define PLATFORM_NVDIMM_HASHED_INTERLEAVE_WAYS  2

/* Region offset of hashed NVDIMM-N */
#define PLATFORM_NVDIMM_HASHED_REGION_OFFSET    512

/* The base address of master socket GIC redistributor registers */
#define GICR_MASTER_BASE_REG    0x100100140000

/* The base address of GIC distributor registers */
#define GICD_BASE_REG           0x100100000000

/* The base address of slave socket GIC redistributor registers */
#define GICR_SLAVE_BASE_REG     0x500100140000

/* The base address of slave socket GIC distributor registers */
#define GICD_SLAVE_BASE_REG     0x500100000000

/* Socket 0 first RC */
#define SOCKET0_FIRST_RC        2

/* Socket 0 last RC */
#define SOCKET0_LAST_RC         7

/* Socket 1 first RC */
#define SOCKET1_FIRST_RC        10

/* Socket 1 last RC */
#define SOCKET1_LAST_RC         15

//
// Offset from SMPRO_EFUSE_SHADOW0
//
#define CFG2P_OFFSET                0x200

//
// Slave Socket Present_N in CFG2P_OFFSET
//
#define SLAVE_PRESENT_N             0x2

#endif /* __PLATFORM_AC01_H_ */
