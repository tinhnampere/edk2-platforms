/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _CPULIB_H_
#define _CPULIB_H_

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

#endif /* _CPULIB_H_ */
