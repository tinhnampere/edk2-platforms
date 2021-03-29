/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PCIE_H_
#define PCIE_H_

#define PCIE_CORE_DEBUG
#define PCIE_CORE_CFG_DEBUG
#undef PCIE_CORE_MMIO_DEBUG

#ifdef PCIE_CORE_CFG_DEBUG
#define PCIE_DEBUG_CFG(arg...)               \
  if (DebugCodeEnabled()) {                  \
    DEBUG ((DEBUG_INFO,"PCICore (DBG): "));  \
    DEBUG ((DEBUG_INFO,## arg));             \
  }
#else
#define PCIE_DEBUG_CFG(arg...)
#endif

#ifdef PCIE_CORE_CSR_DEBUG
#define PCIE_CSR_DEBUG(arg...)               \
  if (DebugCodeEnabled()) {                  \
    DEBUG ((DEBUG_INFO,"PCICore (DBG): "));  \
    DEBUG((DEBUG_INFO,## arg))               \
  }
#else
#define PCIE_CSR_DEBUG(arg...)
#endif

#ifdef PCIE_CORE_PHY_DEBUG
#define PCIE_PHY_DEBUG(arg...)               \
  if (DebugCodeEnabled()) {                  \
    DEBUG ((DEBUG_INFO,"PCICore (DBG): "));  \
    DEBUG ((DEBUG_INFO,## arg))              \
  }
#else
#define PCIE_PHY_DEBUG(arg...)
#endif

#ifdef PCIE_CORE_MMIO_DEBUG
#define PCIE_DEBUG_MMIO(arg...)            \
  DEBUG ((DEBUG_INFO,"PCICore (DBG): "));  \
  DEBUG ((DEBUG_INFO,## arg))
#else
#define PCIE_DEBUG_MMIO(arg...)
#endif

#ifdef PCIE_CORE_DEBUG
#define PCIE_DEBUG(arg...)                   \
  if (DebugCodeEnabled()) {                  \
    DEBUG ((DEBUG_INFO,"PCICore (DBG): "));  \
    DEBUG ((DEBUG_INFO,## arg));             \
  }
#else
#define PCIE_DEBUG(arg...)
#endif

#define PCIE_WARN(arg...)                   \
  DEBUG ((DEBUG_WARN,"PCICore (WARN): "));  \
  DEBUG ((DEBUG_WARN,## arg))

#define PCIE_ERR(arg...)                      \
  DEBUG ((DEBUG_ERROR,"PCICore (ERROR): "));  \
  DEBUG ((DEBUG_ERROR,## arg))

#define RCS_PER_SOCKET          8

#define PCIE_ERRATA_SPEED1      0x0001 // Limited speed errata

#define PRESET_INVALID          0xFF

/* Max number for AC01 PCIE Root Complexes */
#define MAX_AC01_PCIE_ROOT_COMPLEX   16

/* Max number for AC01 PCIE Root Bridge under each Root Complex */
#define MAX_AC01_PCIE_ROOT_BRIDGE    1

/* The base address of {TCU, CSR, MMCONFIG} Registers */
#define AC01_PCIE_REGISTER_BASE    0x33FFE0000000, 0x37FFE0000000, 0x3BFFE0000000, 0x3FFFE0000000, 0x23FFE0000000, 0x27FFE0000000, 0x2BFFE0000000, 0x2FFFE0000000, 0x73FFE0000000, 0x77FFE0000000, 0x7BFFE0000000, 0x7FFFE0000000, 0x63FFE0000000, 0x67FFE0000000, 0x6BFFE0000000, 0x6FFFE0000000

/* The base address of MMIO Registers */
#define AC01_PCIE_MMIO_BASE        0x300000000000, 0x340000000000, 0x380000000000, 0x3C0000000000, 0x200000000000, 0x240000000000, 0x280000000000, 0x2C0000000000, 0x700000000000, 0x740000000000, 0x780000000000, 0x7C0000000000, 0x600000000000, 0x640000000000, 0x680000000000, 0x6C0000000000

/* The base address of MMIO32 Registers*/
#define AC01_PCIE_MMIO32_BASE      0x000020000000, 0x000028000000, 0x000030000000, 0x000038000000, 0x000001000000, 0x000008000000, 0x000010000000, 0x000018000000, 0x000060000000, 0x000068000000, 0x000070000000, 0x000078000000, 0x000040000000, 0x000048000000, 0x000050000000, 0x000058000000

/* The base address of MMIO32 Registers */
#define AC01_PCIE_MMIO32_BASE_1P   0x000040000000, 0x000050000000, 0x000060000000, 0x000070000000, 0x000001000000, 0x000010000000, 0x000020000000, 0x000030000000, 0, 0, 0, 0, 0, 0, 0, 0

/* A switch to enable PciBus Driver Debug messages over Serial Port. */
#define PCI_BUS_DEBUG_MESSAGES    1

/* DSDT RCA2 PCIe Meme32 Attribute */
#define AC01_PCIE_RCA2_QMEM    0x0000000000000000, 0x0000000060000000, 0x000000006FFFFFFF, 0x0000000000000000, 0x0000000010000000

/* DSDT RCA3 PCIe Meme32 Attribute */
#define AC01_PCIE_RCA3_QMEM    0x0000000000000000, 0x0000000070000000, 0x000000007FFFFFFF, 0x0000000000000000, 0x0000000010000000

/* DSDT RCB0 PCIe Meme32 Attribute */
#define AC01_PCIE_RCB0_QMEM    0x0000000000000000, 0x0000000001000000, 0x000000000FFFFFFF, 0x0000000000000000, 0x000000000F000000

/* DSDT RCB1 PCIe Meme32 Attribute */
#define AC01_PCIE_RCB1_QMEM    0x0000000000000000, 0x0000000010000000, 0x000000001FFFFFFF, 0x0000000000000000, 0x0000000010000000

/* DSDT RCB2 PCIe Meme32 Attribute*/
#define AC01_PCIE_RCB2_QMEM    0x0000000000000000, 0x0000000020000000, 0x000000002FFFFFFF, 0x0000000000000000, 0x0000000010000000

/* DSDT RCB3 PCIe Meme32 Attribute */
#define AC01_PCIE_RCB3_QMEM    0x0000000000000000, 0x0000000030000000, 0x000000003FFFFFFF, 0x0000000000000000, 0x0000000010000000

/* Ampere Pcie vendor ID */
#define AMPERE_PCIE_VENDORID  0x1DEF

/* Ampere Pcie device ID */
#define AMPERE_PCIE_DEVICEID  0xE00D

/* The start of TBU PMU IRQ array. */
#define SMMU_TBU_PMU_IRQ_START_ARRAY  224, 230, 236, 242, 160, 170, 180, 190, 544, 550, 556, 562, 480, 490, 500, 510

/* The start of TCU PMU IRQ array */
#define SMMU_TCU_PMU_IRQ_START_ARRAY  256, 257, 258, 259, 260, 261, 262, 263, 576, 577, 578, 579, 580, 581, 582, 583

/* Token Enabled to use PCIIO Mapped address for either DMA or PIO Data Transfer*/
#define USE_PCIIO_MAP_ADDRESS_FOR_DATA_TRANSFER   1

/* Pci Express Base Addrerss should come from CSP module */
#define PCIEX_BASE_ADDRESS    0x00000000

#define PCIEX_LENGTH          0x10000000
/* Pci Express Extended Config Space length should come from CSP module */

#define ISA_IRQ_MASK          0


enum PCIE_LINK_WIDTH {
  LNKW_NONE = 0,
  LNKW_X1 = 0x1,
  LNKW_X2 = 0x2,
  LNKW_X4 = 0x4,
  LNKW_X8 = 0x8,
  LNKW_X16 = 0x10,
};

enum PCIE_LINK_SPEED {
  SPEED_NONE = 0,
  SPEED_GEN1 = 0x1,
  SPEED_GEN2 = 0x2,
  SPEED_GEN3 = 0x4,
  SPEED_GEN4 = 0x8,
};

enum PCIE_CONTROLLER {
  PCIE_0 = 0,
  PCIE_1,
  PCIE_2,
  PCIE_3,
  PCIE_4,
  MAX_PCIE_A = PCIE_4,
  PCIE_5,
  PCIE_6,
  PCIE_7,
  MAX_PCIE,
  MAX_PCIE_B = MAX_PCIE
};

enum RC_TYPE {
  RCA,
  RCB
};

enum RC_BLOCK {
  RCA0 = 0,
  RCA1,
  RCA2,
  RCA3,
  MAX_RCA,
  RCB0 = MAX_RCA,
  RCB1,
  RCB2,
  RCB3,
  MAX_RCB,
  MAX_RC = MAX_RCB
};

typedef struct _AC01_PCIE {
  UINT64  CsrAddr;               // Pointer to CSR Address
  UINT64  SnpsRamAddr;           // Pointer to Synopsys SRAM address
  UINT8   MaxGen;                // Max speed Gen-1/-2/-3/-4
  UINT8   CurGen;                // Current speed Gen-1/-2/-3/-4
  UINT8   MaxWidth;              // Max lanes x2/x4/x8/x16
  UINT8   CurWidth;              // Current lanes x2/x4/x8/x16
  UINT8   ID;                    // ID of the controller within Root Complex
  UINT8   DevNum;                // Device number as part of Bus:Dev:Func
  BOOLEAN Active;                // Active? Used in bi-furcation mode
  BOOLEAN LinkUp;                // PHY and PCIE linkup
  BOOLEAN HotPlug;               // Hotplug support
} AC01_PCIE;

typedef struct _AC01_RC {
  UINT64    BaseAddr;
  UINT64    TcuAddr;
  UINT64    HBAddr;
  UINT64    MsgAddr;
  UINT64    SerdesAddr;
  UINT64    MmcfgAddr;
  UINT64    MmioAddr;
  UINT64    Mmio32Addr;
  UINT64    IoAddr;
  AC01_PCIE Pcie[MAX_PCIE_B];
  UINT8     MaxPcieController;
  UINT8     Type;
  UINT8     ID;
  UINT8     DevMapHi;               // Copy of High Devmap programmed to Host bridge
  UINT8     DevMapLo;               // Copy of Low Devmap programmed to Host bridge
  UINT8     DefaultDevMapHi;        // Default of High devmap based on board settings
  UINT8     DefaultDevMapLo;        // Default of Low devmap based on board settings
  UINT8     Socket;
  BOOLEAN   Active;
  UINT8     Logical;
  VOID      *RootBridge;            // Pointer to Stack PCI_ROOT_BRIDGE
  UINT32    Flags;                  // Flags
  UINT8     PresetGen3[MAX_PCIE_B]; // Preset for Gen3
  UINT8     PresetGen4[MAX_PCIE_B]; // Preset for Gen4
} AC01_RC;

#endif
