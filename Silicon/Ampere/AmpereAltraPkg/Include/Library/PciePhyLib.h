/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PCIE_PHY_LIB_H__
#define __PCIE_PHY_LIB_H__

#define PHY_TX_PARAM_SIZE               2
#define PHY_RX_PARAM_SIZE               2

#define HOST_SECURE_ACCESS(addr)    (unsigned long long)(addr | 0x40000000000000)
#define SIZE_OF_SRAM_CODE_UPDATE    0x100
#define STARTING_SRAM_ADDRESS       0x130000
#define BUG_67112_WORKAROUND        1
#define MULTWRITE_ENABLE            1

/*
 * PCIe PHY error code
 */
typedef enum {
  PHY_SRAM_UPDATE_FAIL = -1,
  PHY_INIT_PASS = 0,
  PHY_ROM_ECC_FAIL,
  PHY_SRAM_ECC_FAIL,
  PHY_CALIB_FAIL,
  PHY_CALIB_TIMEOUT,
  PHY_PLL_FAIL
} PHY_STATUS;

typedef enum {
  PHY_DBG_ERROR = 0x0001,
  PHY_DBG_INFO = 0x0002,
  PHY_DBG_WARN = 0x0004,
  PHY_DBG_VERBOSE = 0x0008
} PHY_DBG_FLAGS;

typedef struct {
  unsigned char is_cal_by_sram;
  unsigned int pll_settings;
  unsigned long tune_tx_param[PHY_RX_PARAM_SIZE];
  unsigned long tune_rx_param[PHY_TX_PARAM_SIZE];
} phy_setting_t;

/**
 * struct serdes_plat_resource - Serdes Platform Operations
 * @puts_cb: Prints string to serial console
 * @putint_cb: Prints 32-bit unsigned integer to serial console
 * @puthex_cb: Prints 32-bit unsigned hex to serial console
 * @puthex64_cb: Prints 64-bit unsigned hex to serial console
 * @debug_print_cb: Prints formated string to serial console
 * @mmio_rd_cb: Reads 32-bit unsigned integer
 * @mmio_wr_cb: Writes 32-bit unsigned integer
 */
typedef struct {
  void (*puts_cb)(const char *msg);
  void (*putint_cb)(unsigned int val);
  void (*puthex_cb)(unsigned int val);
  void (*puthex64_cb)(unsigned long val);
  int (*debug_print_cb)(const char *fmt, ...);
  void (*mmio_rd_cb)(unsigned long int addr, unsigned int *val);
  void (*mmio_wr_cb)(unsigned long int addr, unsigned int val);
  void (*uDelay_cb)(unsigned int val);
} phy_plat_resource_t;

typedef struct {
  unsigned long sds_addr;        // PHY base address
  unsigned long pcie_ctrl_info;  // PCIe controller related information passed here
                  // BIT0-1: SoC revision
                  //   0: Ampere Altra
                  //   1: Ampere Mystique
                  //   2: Ampere Siryn
                  // BIT2  : SocketID (0: Socket0, 1: Socket1)
                  // BIT3  : Reserved
                  // BIT4-6: Root Complex context (RCA0/1/2/3 or RCB0/4/5/6)
                  // BIT7  : Reserved
                  // BIT8-9: PHY Numbers within RCA/RCB [0 to 3 each controls 4 lane ]
                  // 0 : x16, 1: x8 , 2:x4, 3: 0x2
                  // BIT10-11 : Gen
                  // 0 : Gen1, 1: Gen2, 2: Gen3 ; 3: Gen4 + ESM
  phy_setting_t phy_setting;               // PHY input setting
  phy_plat_resource_t *phy_plat_resource;  // Debug and misc function pointers
  PHY_DBG_FLAGS debug;
} phy_context_t;

/*
 * Description
 *
 * Input:
 *  ctx    - Serdes context pointer
 *
 * Return:
 *  PHY_STATUS - Return status
 */
PHY_STATUS serdes_sram_update(phy_context_t *ctx);

/*
 * Description
 */
PHY_STATUS serdes_init_clkrst(phy_context_t *ctx);

/*
 * Description
 */
PHY_STATUS serdes_init_calib(phy_context_t *ctx);

#endif
