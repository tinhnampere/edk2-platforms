/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _AMPERE_CPU_LIB_H_
#define _AMPERE_CPU_LIB_H_

/* Ctypen, bits[3(n - 1) + 2 : 3(n - 1)], for n = 1 to 7 */
#define CLIDR_CTYPE_SHIFT(Level)    (3 * (Level - 1))
#define CLIDR_CTYPE_MASK(Level)     (7 << CLIDR_CTYPE_SHIFT(Level))
#define CLIDR_CTYPE(Clidr, Level) \
  (((Clidr) & CLIDR_CTYPE_MASK(Level)) >> CLIDR_CTYPE_SHIFT(Level))

#define CCSIDR_NUMSETS_SHIFT        13
#define CCSIDR_NUMSETS_MASK         0xFFFE000
#define CCSIDR_NUMSETS(Ccsidr) \
  (((Ccsidr) & CCSIDR_NUMSETS_MASK) >> CCSIDR_NUMSETS_SHIFT)
#define CCSIDR_ASSOCIATIVITY_SHIFT  3
#define CCSIDR_ASSOCIATIVITY_MASK   0x1FF8
#define CCSIDR_ASSOCIATIVITY(Ccsidr) \
  (((Ccsidr) & CCSIDR_ASSOCIATIVITY_MASK) >> CCSIDR_ASSOCIATIVITY_SHIFT)
#define CCSIDR_LINE_SIZE_SHIFT      0
#define CCSIDR_LINE_SIZE_MASK       0x7
#define CCSIDR_LINE_SIZE(Ccsidr) \
  (((Ccsidr) & CCSIDR_LINE_SIZE_MASK) >> CCSIDR_LINE_SIZE_SHIFT)

#define SOC_EFUSE_SHADOWn(s,x)        (SMPRO_EFUSE_SHADOW0 + (s) * SOCKET_BASE_OFFSET + (x) * 4)

#define SUBNUMA_MODE_MONOLITHIC        0
#define SUBNUMA_MODE_HEMISPHERE        1
#define SUBNUMA_MODE_QUADRANT          2

UINT8 CPUGetSubNumaMode (VOID);

/*
 * Return the number of SubNUMA region
 */
UINT8 CPUGetNumOfSubNuma(VOID);

/*
 * Return the SubNUMA node of a CPM
 */
UINT8 CPUGetSubNumNode(UINT8 Socket, UINT32 Cpm);

/*
 * Return the value of CLIDR register
 */
UINT64 Aarch64ReadCLIDRReg ();

/*
 * Return the value of CCSID register
 */
UINT64 Aarch64ReadCCSIDRReg (UINT64 Level);

/*
 * Return the associativity of cache
 */
UINT32 CpuGetAssociativity (UINTN Level);

/*
 * Return the cache size.
 */
UINT32 CpuGetCacheSize (UINTN Level);

/**
  Get the number of socket supported.

  @return   UINT32      Number of supported socket.

**/
EFIAPI
UINT32
GetNumberSupportedSockets (VOID);

/**
  Get the number of active socket.

  @return   UINT32      Number of active socket.

**/
EFIAPI
UINT32
GetNumberActiveSockets (VOID);

/**
  Get the number of active CPM per socket.

  @param    UINT32      Socket index.
  @return   UINT32      Number of CPM.

**/
EFIAPI
UINT32
GetNumberActiveCPMsPerSocket (UINT32 SocketId);

/**
  Get the configured number of CPM per socket. This number
  should be the same for all sockets.

  @return   UINT32      Configurated number of CPM.

**/
EFIAPI
UINT32
GetConfiguratedNumberCPMs (IN UINTN Socket);

/**
  Set the configured number of CPM per socket. This number
  should be the same for all sockets.

  @param    Number      Number of CPM to be configurated.
  @return   EFI_STATUS  EFI_SUCCESS if successful.

**/
EFIAPI
EFI_STATUS
SetConfiguratedNumberCPMs (IN UINTN Socket, IN UINTN Number);

/**
  Get the maximum number of core per socket. This number
  should be the same for all sockets.

  @return   UINT32      Maximum number of core.

**/
EFIAPI
UINT32
GetMaximumNumberOfCores (VOID);

/**
  Get the maximum number of CPM per socket. This number
  should be the same for all sockets.

  @return   UINT32      Maximum number of CPM.

**/
EFIAPI
UINT32
GetMaximumNumberCPMs (VOID);

/**
  Get the number of active cores of a sockets.

  @return   UINT32      Number of active core.

**/
EFIAPI
UINT32
GetNumberActiveCoresPerSocket (UINT32 SocketId);

/**
  Get the number of active cores of all socket.

  @return   UINT32      Number of active core.

**/
EFIAPI
UINT32
GetNumberActiveCores (VOID);

/**
  Check if the logical CPU is enabled or not.

  @param    Cpu         The logical Cpu ID. Started from 0.
  @return   BOOLEAN     TRUE if the Cpu enabled
                        FALSE if the Cpu disabled/

**/
EFIAPI
BOOLEAN
IsCpuEnabled (IN UINTN Cpu);


/**
  Check if the slave socket is present

  @return   BOOLEAN     TRUE if the Slave Cpu present
                        FALSE if the Slave Cpu present

**/
EFIAPI
BOOLEAN
PlatSlaveSocketPresent (VOID);

#endif /* _AMPERE_CPU_LIB_H_ */
