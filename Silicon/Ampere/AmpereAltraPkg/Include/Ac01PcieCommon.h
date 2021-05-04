/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef AC01_PCIE_COMMON_H_
#define AC01_PCIE_COMMON_H_

#define PRESET_INVALID   0xFF

//
// PCIe link width
//
enum {
  LNKW_NONE = 0,
  LNKW_X1 = 0x1,
  LNKW_X2 = 0x2,
  LNKW_X4 = 0x4,
  LNKW_X8 = 0x8,
  LNKW_X16 = 0x10,
};

//
// PCIe link speed
//
enum {
  SPEED_NONE = 0,
  SPEED_GEN1 = 0x1,
  SPEED_GEN2 = 0x2,
  SPEED_GEN3 = 0x4,
  SPEED_GEN4 = 0x8,
};

//
// PCIe controller number
//
enum {
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

//
// Root Complex type
//
enum {
  RCA,
  RCB
};

//
// Root Complex number
//
enum {
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

//
// Data structure to store the PCIe controller information
//
typedef struct {
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

//
// Data structure to store the Root Complex information
//
typedef struct {
  UINT64    BaseAddr;
  UINT64    TcuAddr;
  UINT64    HBAddr;
  UINT64    MsgAddr;
  UINT64    SerdesAddr;
  UINT64    MmcfgAddr;
  UINT64    MmioAddr;
  UINT64    MmioSize;
  UINT64    Mmio32Addr;
  UINT64    Mmio32Size;
  UINT64    IoAddr;
  AC01_PCIE Pcie[MAX_PCIE_B];
  UINT8     MaxPcieController;
  UINT8     Type;
  UINT8     ID;
  UINT8     DevMapHigh:3;           // Copy of High Devmap programmed to Host bridge
  UINT8     DevMapLow:3;            // Copy of Low Devmap programmed to Host bridge
  UINT8     DefaultDevMapHigh:3;    // Default of High devmap based on board settings
  UINT8     DefaultDevMapLow:3;     // Default of Low devmap based on board settings
  UINT8     Socket;
  BOOLEAN   Active;
  UINT8     Logical;
  VOID      *RootBridge;            // Pointer to Stack PCI_ROOT_BRIDGE
  UINT32    Flags;                  // Flags
  UINT8     PresetGen3[MAX_PCIE_B]; // Preset for Gen3
  UINT8     PresetGen4[MAX_PCIE_B]; // Preset for Gen4
} AC01_RC;

#endif
