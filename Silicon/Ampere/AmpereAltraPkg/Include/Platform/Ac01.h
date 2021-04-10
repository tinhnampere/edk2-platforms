/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_AC01_H_
#define PLATFORM_AC01_H_

/* Number of supported sockets in the platform */
#define PLATFORM_CPU_MAX_SOCKET             2

/* Maximum number of CPMs in the chip. */
#define PLATFORM_CPU_MAX_CPM                (FixedPcdGet32 (PcdClusterCount))

/* Number of cores per CPM. */
#define PLATFORM_CPU_NUM_CORES_PER_CPM      (FixedPcdGet32 (PcdCoreCount) / PLATFORM_CPU_MAX_CPM)

/* Socket bit offset of core UID. */
#define PLATFORM_SOCKET_UID_BIT_OFFSET      16

/* CPM bit offset of core UID. */
#define PLATFORM_CPM_UID_BIT_OFFSET         8

/* Maximum number of cores supported. */
#define PLATFORM_CPU_MAX_NUM_CORES  (PLATFORM_CPU_MAX_SOCKET * PLATFORM_CPU_MAX_CPM * PLATFORM_CPU_NUM_CORES_PER_CPM)

/* Maximum number of memory region */
#define PLATFORM_DRAM_INFO_MAX_REGION  16

/* Maximum number of DDR slots supported */
#define PLATFORM_DIMM_INFO_MAX_SLOT    32

/* Maximum number of memory supported. */
#define PLATFORM_MAX_MEMORY_REGION     4

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
// SMpro EFUSE Shadow register
//
#define SMPRO_EFUSE_SHADOW0         (FixedPcdGet64 (PcdSmproEfuseShadow0))

#define CFG2P_OFFSET                0x200
#define SLAVE_PRESENT_N             BIT1 // Slave socket present

#endif /* PLATFORM_AC01_H_ */
