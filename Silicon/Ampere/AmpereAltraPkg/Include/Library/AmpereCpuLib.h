/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef AMPERE_CPU_LIB_H_
#define AMPERE_CPU_LIB_H_

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

/**
  Get the SubNUMA mode.

  @return   UINT8      The SubNUMA mode.

**/
UINT8
EFIAPI
CPUGetSubNumaMode (VOID);

/**
  Get the number of SubNUMA region.

  @return   UINT8      The number of SubNUMA region.

**/
UINT8
EFIAPI
CPUGetNumOfSubNuma (VOID);

/**
  Get the SubNUMA node of a CPM.

  @param    SocketId    Socket index.
  @param    Cpm         CPM index.
  @return   UINT8       The SubNUMA node of a CPM.

**/
UINT8
EFIAPI
CPUGetSubNumNode (
  UINT8 Socket,
  UINT32 Cpm
  );

/**
  Get the value of CLIDR register.

  @return   UINT64      The value of CLIDR register.

**/
UINT64
EFIAPI
AArch64ReadCLIDRReg ();

/**
  Get the value of CCSID register.

  @param    Level       Cache level.
  @return   UINT64      The value of CCSID register.

**/
UINT64
EFIAPI
AArch64ReadCCSIDRReg (
  UINT64 Level
  );

/**
  Get the associativity of cache.

  @param    Level       Cache level.
  @return   UINT32      Associativity of cache.

**/
UINT32
EFIAPI
CpuGetAssociativity (
  UINTN Level
  );

/**
  Get the cache size.

  @param    Level       Cache level.
  @return   UINT32      Cache size.

**/
UINT32
EFIAPI
CpuGetCacheSize (
  UINTN Level
  );

/**
  Get the number of supported socket.

  @return   UINT32      Number of supported socket.

**/
UINT32
EFIAPI
GetNumberSupportedSockets (VOID);

/**
  Get the number of active socket.

  @return   UINT32      Number of active socket.

**/
UINT32
EFIAPI
GetNumberActiveSockets (VOID);

/**
  Get the number of active CPM per socket.

  @param    SocketId    Socket index.
  @return   UINT32      Number of CPM.

**/
UINT32
EFIAPI
GetNumberActiveCPMsPerSocket (
  UINT32 SocketId
  );

/**
  Get the configured number of CPM per socket.

  @param    SocketId    Socket index.
  @return   UINT32      Configured number of CPM.

**/
UINT32
EFIAPI
GetConfiguredNumberCPMs (
  IN UINTN Socket
  );

/**
  Set the configured number of CPM per socket.

  @param    SocketId        Socket index.
  @param    Number          Number of CPM to be configured.
  @return   EFI_SUCCESS     Operation succeeded.
  @return   Others          An error has occurred.

**/
EFI_STATUS
EFIAPI
SetConfiguredNumberCPMs (
  UINTN Socket,
  UINTN Number
  );

/**
  Get the maximum number of core per socket. This number
  should be the same for all sockets.

  @return   UINT32      Maximum number of core.

**/
UINT32
EFIAPI
GetMaximumNumberOfCores (VOID);

/**
  Get the maximum number of CPM per socket. This number
  should be the same for all sockets.

  @return   UINT32      Maximum number of CPM.

**/
UINT32
EFIAPI
GetMaximumNumberCPMs (VOID);

/**
  Get the number of active cores of a sockets.

  @return   UINT32      Number of active core.

**/
UINT32
EFIAPI
GetNumberActiveCoresPerSocket (
  UINT32 SocketId
  );

/**
  Get the number of active cores of all socket.

  @return   UINT32      Number of active core.

**/
UINT32
EFIAPI
GetNumberActiveCores (VOID);

/**
  Check if the logical CPU is enabled or not.

  @param    CpuId       The logical Cpu ID. Started from 0.
  @return   BOOLEAN     TRUE if the Cpu enabled
                        FALSE if the Cpu disabled/

**/
BOOLEAN
EFIAPI
IsCpuEnabled (
  UINTN CpuId
  );


/**
  Check if the slave socket is present

  @return   BOOLEAN     TRUE if the Slave Cpu present
                        FALSE if the Slave Cpu present

**/
BOOLEAN
EFIAPI
PlatSlaveSocketPresent (VOID);

#endif /* AMPERE_CPU_LIB_H_ */
