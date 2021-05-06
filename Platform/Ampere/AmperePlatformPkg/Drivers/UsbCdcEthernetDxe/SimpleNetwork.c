/** @file
*
*  Copyright (c) 2021, Ampere Computing. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include "UsbCdcEthernet.h"


/**
  Reads the current interrupt status and recycled transmit buffer status from
  a network interface.

  @param  This            The protocol instance pointer.
  @param  InterruptStatus A pointer to the bit mask of the currently active interrupts
                          If this is NULL, the interrupt status will not be read from
                          the device. If this is not NULL, the interrupt status will
                          be read from the device. When the  interrupt status is read,
                          it will also be cleared. Clearing the transmit  interrupt
                          does not empty the recycled transmit buffer array.
  @param  TxBuf           Recycled transmit buffer address. The network interface will
                          not transmit if its internal recycled transmit buffer array
                          is full. Reading the transmit buffer does not clear the
                          transmit interrupt. If this is NULL, then the transmit buffer
                          status will not be read. If there are no transmit buffers to
                          recycle and TxBuf is not NULL, * TxBuf will be set to NULL.

  @retval EFI_SUCCESS           The status of the network interface was retrieved.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpGetStatus (
  IN  EFI_SIMPLE_NETWORK_PROTOCOL *This,
  OUT UINT32                      *InterruptStatus OPTIONAL,
  OUT VOID                        **TxBuf OPTIONAL
  )
{
  EFI_SIMPLE_NETWORK_MODE         *Mode;
  USB_CDC_ETHERNET_PRIVATE_DATA   *PrivateData;

  if (This == NULL || This->Mode == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Mode = This->Mode;

  //
  // Check that driver was started and initialised
  //
  if (Mode->State == EfiSimpleNetworkStopped) {
    return EFI_NOT_STARTED;
  }
  if (Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_DEVICE_ERROR;
  }

  PrivateData = USB_CDC_ETHERNET_PRIVATE_DATA_FROM_THIS_SNP (This);
  ASSERT (PrivateData != NULL);

  if (TxBuf != NULL && PrivateData->TxBuffer != NULL) {
    *TxBuf = PrivateData->TxBuffer;
    PrivateData->TxBuffer = NULL;
  }

  if (InterruptStatus != NULL) {
    *InterruptStatus = 0;
  }

  return EFI_SUCCESS;
}

/**
  Resets a network adapter and re-initializes it with the parameters that were
  provided in the previous call to Initialize().

  @param  This                 The protocol instance pointer.
  @param  ExtendedVerification Indicates that the driver may perform a more
                               exhaustive verification operation of the device
                               during reset.

  @retval EFI_SUCCESS           The network interface was reset.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpReset (
  IN EFI_SIMPLE_NETWORK_PROTOCOL *This,
  IN BOOLEAN                     ExtendedVerification
  )
{
  EFI_SIMPLE_NETWORK_MODE *Mode;

  if (This == NULL || This->Mode == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Mode = This->Mode;

  //
  // Check that driver was started and initialised
  //
  if (Mode->State == EfiSimpleNetworkStopped) {
    return EFI_NOT_STARTED;
  }
  if (Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Performs read and write operations on the NVRAM device attached to a
  network interface.

  @param  This       The protocol instance pointer.
  @param  ReadWrite  TRUE for read operations, FALSE for write operations.
  @param  Offset     Byte offset in the NVRAM device at which to start the read or
                     write operation. This must be a multiple of NvRamAccessSize and
                     less than NvRamSize.
  @param  BufferSize The number of bytes to read or write from the NVRAM device.
                     This must also be a multiple of NvramAccessSize.
  @param  Buffer     A pointer to the data buffer.

  @retval EFI_SUCCESS           The NVRAM access was performed.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpNvData (
  IN EFI_SIMPLE_NETWORK_PROTOCOL *This,
  IN BOOLEAN                     ReadWrite,
  IN UINTN                       Offset,
  IN UINTN                       BufferSize,
  IN OUT VOID                    *Buffer
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Resets a network adapter and allocates the transmit and receive buffers
  required by the network interface; optionally, also requests allocation
  of additional transmit and receive buffers.

  @param  This              The protocol instance pointer.
  @param  ExtraRxBufferSize The size, in bytes, of the extra receive buffer space
                            that the driver should allocate for the network interface.
                            Some network interfaces will not be able to use the extra
                            buffer, and the caller will not know if it is actually
                            being used.
  @param  ExtraTxBufferSize The size, in bytes, of the extra transmit buffer space
                            that the driver should allocate for the network interface.
                            Some network interfaces will not be able to use the extra
                            buffer, and the caller will not know if it is actually
                            being used.

  @retval EFI_SUCCESS           The network interface was initialized.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory for the transmit and
                                receive buffers.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpInitialize (
  IN  EFI_SIMPLE_NETWORK_PROTOCOL  *This,
  IN  UINTN                        ExtraRxBufferSize OPTIONAL,
  IN  UINTN                        ExtraTxBufferSize OPTIONAL
  )
{
  EFI_SIMPLE_NETWORK_MODE *Mode;
  EFI_STATUS              Status;
  USB_CDC_ETHERNET_PRIVATE_DATA    *PrivateData;

  if (This == NULL || This->Mode == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData = USB_CDC_ETHERNET_PRIVATE_DATA_FROM_THIS_SNP (This);
  Mode = This->Mode;

  if (Mode->State == EfiSimpleNetworkStopped) {
    return EFI_NOT_STARTED;
  } else if (Mode->State != EfiSimpleNetworkStarted) {
    return EFI_DEVICE_ERROR;
  }

  Mode->State = EfiSimpleNetworkInitialized;

  Status = UsbCdcGetLinkStatus (PrivateData);
  if (EFI_ERROR (Status)) {
    Mode->MediaPresent = FALSE;
  } else {
    Mode->MediaPresent = PrivateData->LinkUp;
    DEBUG ((DEBUG_VERBOSE, "%a: Mode->MediaPresent = %d\n", __FUNCTION__, (UINT8)Mode->MediaPresent));
  }

  return Status;
}


/**
  Converts a multicast IP address to a multicast HW MAC address.

  @param  This The protocol instance pointer.
  @param  IPv6 Set to TRUE if the multicast IP address is IPv6 [RFC 2460]. Set
               to FALSE if the multicast IP address is IPv4 [RFC 791].
  @param  IP   The multicast IP address that is to be converted to a multicast
               HW MAC address.
  @param  MAC  The multicast HW MAC address that is to be generated from IP.

  @retval EFI_SUCCESS           The multicast IP address was mapped to the multicast
                                HW MAC address.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_BUFFER_TOO_SMALL  The Statistics buffer was too small. The current buffer
                                size needed to hold the statistics is returned in
                                StatisticsSize.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpMCastIPtoMAC (
  IN  EFI_SIMPLE_NETWORK_PROTOCOL *This,
  IN  BOOLEAN                     IPv6,
  IN  EFI_IP_ADDRESS              *IP,
  OUT EFI_MAC_ADDRESS             *MAC
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Receives a packet from a network interface.

  This function retrieves one packet from the receive queue of the network
  interface. If there are no packets on the receive queue, then EFI_NOT_READY
  will be returned. If there is a packet on the receive queue, and the size
  of the packet is smaller than BufferSize, then the contents of the packet
  will be placed in Buffer, and BufferSize will be updated with the actual
  size of the packet. In addition, if SrcAddr, DestAddr, and Protocol are
  not NULL, then these values will be extracted from the media header and
  returned. If BufferSize is smaller than the received packet, then the
  size of the receive packet will be placed in BufferSize and
  EFI_BUFFER_TOO_SMALL will be returned.

  @param  This       The protocol instance pointer.
  @param  HeaderSize The size, in bytes, of the media header received on the network
                     interface. If this parameter is NULL, then the media header size
                     will not be returned.
  @param  BufferSize On entry, the size, in bytes, of Buffer. On exit, the size, in
                     bytes, of the packet that was received on the network interface.
  @param  Buffer     A pointer to the data buffer to receive both the media header and
                     the data.
  @param  SrcAddr    The source HW MAC address. If this parameter is NULL, the
                     HW MAC source address will not be extracted from the media
                     header.
  @param  DestAddr   The destination HW MAC address. If this parameter is NULL,
                     the HW MAC destination address will not be extracted from the
                     media header.
  @param  Protocol   The media header type. If this parameter is NULL, then the
                     protocol will not be extracted from the media header. See
                     RFC 1700 section "Ether Types" for examples.

  @retval  EFI_SUCCESS           The received data was stored in Buffer, and BufferSize has
                                 been updated to the number of bytes received.
  @retval  EFI_NOT_STARTED       The network interface has not been started.
  @retval  EFI_NOT_READY         No packets have been received on the network interface.
  @retval  EFI_BUFFER_TOO_SMALL  The BufferSize parameter is too small.
  @retval  EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval  EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval  EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpReceive (
  IN EFI_SIMPLE_NETWORK_PROTOCOL          *This,
  OUT UINTN                               *HeaderSize OPTIONAL,
  IN OUT UINTN                            *BufferSize,
  OUT VOID                                *Buffer,
  OUT EFI_MAC_ADDRESS                     *SrcAddr    OPTIONAL,
  OUT EFI_MAC_ADDRESS                     *DestAddr   OPTIONAL,
  OUT UINT16                              *Protocol   OPTIONAL
  )
{
  ETHER_HEAD                      *Header;
  EFI_SIMPLE_NETWORK_MODE         *Mode;
  USB_CDC_ETHERNET_PRIVATE_DATA   *PrivateData;
  EFI_STATUS                      Status;
  UINT16                          PktLen;

  if (This == NULL
      || This->Mode == NULL
      || Buffer == NULL
      || BufferSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Mode = This->Mode;

  if (!Mode->MediaPresent) {
    //
    // Don't bother transmitting if there's no link.
    //
    return EFI_NOT_READY;
  }

  //
  // Check that driver was started and initialised
  //
  if (Mode->State == EfiSimpleNetworkStopped) {
    return EFI_NOT_STARTED;
  }
  if (Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_DEVICE_ERROR;
  }

  PrivateData = USB_CDC_ETHERNET_PRIVATE_DATA_FROM_THIS_SNP (This);
  ASSERT (PrivateData != NULL);

  //
  //  Attempt to receive an Ethernet packet
  //
  Status = UsbCdcBulkIn (PrivateData);

  if (PrivateData->BulkInLength == 0) {
    return EFI_NOT_READY;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a %d No packet received!\n", __FUNCTION__, __LINE__));
    return Status;
  }

  PktLen = PrivateData->BulkInLength;

  if (PrivateData->BulkInLength > MAX_ETHERNET_PKT_SIZE) {
    return EFI_DEVICE_ERROR;
  }

  if (*BufferSize < PrivateData->BulkInLength) {
    return EFI_BUFFER_TOO_SMALL;
  }

  *BufferSize = PktLen;
  CopyMem (Buffer, PrivateData->BulkInBuffer, PktLen);
  Header = (ETHER_HEAD *)PrivateData->BulkInBuffer;

  if (HeaderSize != NULL) {
    *HeaderSize = PrivateData->SnpMode.MediaHeaderSize;
  }
  if (DestAddr != NULL) {
    CopyMem (DestAddr, &Header->DstMac, NET_ETHER_ADDR_LEN);
  }
  if (SrcAddr != NULL) {
    CopyMem (SrcAddr, &Header->SrcMac, NET_ETHER_ADDR_LEN);
  }
  if (Protocol != NULL) {
    *Protocol = NTOHS (Header->EtherType);
  }

  return Status;
}

/**
  Manages the multicast receive filters of a network interface.

  @param  This             The protocol instance pointer.
  @param  Enable           A bit mask of receive filters to enable on the network interface.
  @param  Disable          A bit mask of receive filters to disable on the network interface.
  @param  ResetMCastFilter Set to TRUE to reset the contents of the multicast receive
                           filters on the network interface to their default values.
  @param  McastFilterCnt   Number of multicast HW MAC addresses in the new
                           MCastFilter list. This value must be less than or equal to
                           the MCastFilterCnt field of EFI_SIMPLE_NETWORK_MODE. This
                           field is optional if ResetMCastFilter is TRUE.
  @param  MCastFilter      A pointer to a list of new multicast receive filter HW MAC
                           addresses. This list will replace any existing multicast
                           HW MAC address list. This field is optional if
                           ResetMCastFilter is TRUE.

  @retval EFI_SUCCESS           The multicast receive filter list was updated.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpReceiveFilters (
  IN EFI_SIMPLE_NETWORK_PROTOCOL *This,
  IN UINT32                      Enable,
  IN UINT32                      Disable,
  IN BOOLEAN                     ResetMCastFilter,
  IN UINTN                       MCastFilterCnt OPTIONAL,
  IN EFI_MAC_ADDRESS             *MCastFilter   OPTIONAL
  )
{
  EFI_SIMPLE_NETWORK_MODE *Mode;
  USB_CDC_ETHERNET_PRIVATE_DATA    *PrivateData;
  EFI_STATUS              Status;
  UINTN                   Index;
  UINT8                   Temp;
  UINT16                  CdcFilterMask;

  Mode = This->Mode;
  CdcFilterMask = USB_CDC_ECM_PACKET_TYPE_DIRECTED | USB_CDC_ECM_PACKET_TYPE_BROADCAST;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check that driver was started and initialised
  //
  if (Mode->State == EfiSimpleNetworkStopped) {
    return EFI_NOT_STARTED;
  }
  if (Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_DEVICE_ERROR;
  }

  //
  // Check if we are asked to enable or disable something that the SNP
  // does not even support!
  //
  if ((Enable & ~Mode->ReceiveFilterMask) != 0
      || (Disable & ~Mode->ReceiveFilterMask) != 0) {
    return EFI_INVALID_PARAMETER;
  }

  if (ResetMCastFilter) {
    DEBUG ((DEBUG_VERBOSE, "%a %d Reset Multicast\n", __FUNCTION__, __LINE__));
    if ((Mode->ReceiveFilterSetting & EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST) == 0
        && Enable == 0
        && Disable == 2) {
      return EFI_SUCCESS;
    }
    Mode->MCastFilterCount = 0;
    SetMem (
      &Mode->MCastFilter[0],
      sizeof (EFI_MAC_ADDRESS) * MAX_MCAST_FILTER_CNT,
      0
      );
  } else {
    if (MCastFilterCnt != 0) {
      EFI_MAC_ADDRESS * MulticastAddress;
      MulticastAddress = MCastFilter;

      if ((MCastFilterCnt > Mode->MaxMCastFilterCount) ||
          (MCastFilter == NULL)) {
        return EFI_INVALID_PARAMETER;
      }

      for (Index = 0; Index < MCastFilterCnt; Index++) {
          Temp = MulticastAddress->Addr[0];
          if ((Temp & 0x01) != 0x01) {
            return EFI_INVALID_PARAMETER;
          }
          MulticastAddress++;
      }

      Mode->MCastFilterCount = (UINT32)MCastFilterCnt;
      CopyMem (
        &Mode->MCastFilter[0],
        MCastFilter,
        MCastFilterCnt * sizeof (EFI_MAC_ADDRESS)
        );
    }
  }

  if ((Enable & ~Disable & EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS) != 0) {
    DEBUG ((DEBUG_VERBOSE, "%a %d Enable Promiscuous mode\n", __FUNCTION__, __LINE__));
    CdcFilterMask |= USB_CDC_ECM_PACKET_TYPE_PROMISCUOUS;
  }

  if ((Enable & ~Disable & EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST) != 0) {
    DEBUG ((DEBUG_VERBOSE, "%a %d Enable Broadcast mode\n", __FUNCTION__, __LINE__));
    CdcFilterMask |= USB_CDC_ECM_PACKET_TYPE_MULTICAST;
  }

  if (Enable == 0 && Disable == 0 && !ResetMCastFilter && MCastFilterCnt == 0) {
    return EFI_SUCCESS;
  }

  if ((Enable & EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST) != 0 && MCastFilterCnt == 0) {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData = USB_CDC_ETHERNET_PRIVATE_DATA_FROM_THIS_SNP (This);
  ASSERT (PrivateData != NULL);

  Status = UsbCdcUpdateFilterSetting (PrivateData, CdcFilterMask);

  Mode->ReceiveFilterSetting |= Enable;
  Mode->ReceiveFilterSetting &= ~Disable;

  return Status;
}

/**
  Changes the state of a network interface from "stopped" to "started".

  @param  This Protocol instance pointer.

  @retval EFI_SUCCESS           The network interface was started.
  @retval EFI_ALREADY_STARTED   The network interface is already in the started state.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpStart (
  IN EFI_SIMPLE_NETWORK_PROTOCOL *This
  )
{
  EFI_SIMPLE_NETWORK_MODE *Mode;

  if (This == NULL || This->Mode == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Mode = This->Mode;

  if (Mode->State == EfiSimpleNetworkStarted) {
    return EFI_ALREADY_STARTED;
  } else if (Mode->State != EfiSimpleNetworkStopped) {
    return EFI_DEVICE_ERROR;
  }

  Mode->State = EfiSimpleNetworkStarted;

  return EFI_SUCCESS;
}

/**
  Modifies or resets the current station address, if supported.

  @param  This  The protocol instance pointer.
  @param  Reset Flag used to reset the station address to the network interfaces
                permanent address.
  @param  New   The new station address to be used for the network interface.

  @retval EFI_SUCCESS           The network interfaces station address was updated.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpStationAddress (
  IN EFI_SIMPLE_NETWORK_PROTOCOL *This,
  IN BOOLEAN                     Reset,
  IN EFI_MAC_ADDRESS             *New OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Resets or collects the statistics on a network interface.

  @param  This            Protocol instance pointer.
  @param  Reset           Set to TRUE to reset the statistics for the network interface.
  @param  StatisticsSize  On input the size, in bytes, of StatisticsTable. On
                          output the size, in bytes, of the resulting table of
                          statistics.
  @param  StatisticsTable A pointer to the EFI_NETWORK_STATISTICS structure that
                          contains the statistics.

  @retval EFI_SUCCESS           The statistics were collected from the network interface.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_BUFFER_TOO_SMALL  The Statistics buffer was too small. The current buffer
                                size needed to hold the statistics is returned in
                                StatisticsSize.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpStatistics (
  IN EFI_SIMPLE_NETWORK_PROTOCOL          *This,
  IN BOOLEAN                              Reset,
  IN OUT UINTN                            *StatisticsSize   OPTIONAL,
  OUT EFI_NETWORK_STATISTICS              *StatisticsTable  OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}


/**
  Changes the state of a network interface from "started" to "stopped".

  @param  This Protocol instance pointer.

  @retval EFI_SUCCESS           The network interface was stopped.
  @retval EFI_ALREADY_STARTED   The network interface is already in the stopped state.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpStop (
  IN EFI_SIMPLE_NETWORK_PROTOCOL *This
  )
{
  EFI_SIMPLE_NETWORK_MODE *Mode;

  if (This == NULL || This->Mode == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Mode = This->Mode;

  if (Mode->State == EfiSimpleNetworkStopped) {
    return EFI_NOT_STARTED;
  } else if (Mode->State != EfiSimpleNetworkStarted) {
    return EFI_DEVICE_ERROR;
  }

  Mode->State = EfiSimpleNetworkStopped;

  return EFI_SUCCESS;
}


/**
  Resets a network adapter and leaves it in a state that is safe for
  another driver to initialize.

  @param  This Protocol instance pointer.

  @retval EFI_SUCCESS           The network interface was shutdown.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpShutdown (
  IN EFI_SIMPLE_NETWORK_PROTOCOL *This
  )
{
  EFI_SIMPLE_NETWORK_MODE *Mode;

  if (This == NULL || This->Mode == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Mode = This->Mode;

  if (Mode->State == EfiSimpleNetworkStopped) {
    return EFI_NOT_STARTED;
  }
  if (Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_DEVICE_ERROR;
  }

  Mode->State = EfiSimpleNetworkStarted;

  return EFI_SUCCESS;
}

/**
  Places a packet in the transmit queue of a network interface.

  @param  This       The protocol instance pointer.
  @param  HeaderSize The size, in bytes, of the media header to be filled in by
                     the Transmit() function. If HeaderSize is non-zero, then it
                     must be equal to This->Mode->MediaHeaderSize and the DestAddr
                     and Protocol parameters must not be NULL.
  @param  BufferSize The size, in bytes, of the entire packet (media header and
                     data) to be transmitted through the network interface.
  @param  Buffer     A pointer to the packet (media header followed by data) to be
                     transmitted. This parameter cannot be NULL. If HeaderSize is zero,
                     then the media header in Buffer must already be filled in by the
                     caller. If HeaderSize is non-zero, then the media header will be
                     filled in by the Transmit() function.
  @param  SrcAddr    The source HW MAC address. If HeaderSize is zero, then this parameter
                     is ignored. If HeaderSize is non-zero and SrcAddr is NULL, then
                     This->Mode->CurrentAddress is used for the source HW MAC address.
  @param  DestAddr   The destination HW MAC address. If HeaderSize is zero, then this
                     parameter is ignored.
  @param  Protocol   The type of header to build. If HeaderSize is zero, then this
                     parameter is ignored. See RFC 1700, section "Ether Types", for
                     examples.

  @retval EFI_SUCCESS           The packet was placed on the transmit queue.
  @retval EFI_NOT_STARTED       The network interface has not been started.
  @retval EFI_NOT_READY         The network interface is too busy to accept this transmit request.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize parameter is too small.
  @retval EFI_INVALID_PARAMETER One or more of the parameters has an unsupported value.
  @retval EFI_DEVICE_ERROR      The command could not be sent to the network interface.
  @retval EFI_UNSUPPORTED       This function is not supported by the network interface.

**/
EFI_STATUS
EFIAPI
SnpTransmit (
  IN EFI_SIMPLE_NETWORK_PROTOCOL *This,
  IN UINTN                       HeaderSize,
  IN UINTN                       BufferSize,
  IN VOID                        *Buffer,
  IN EFI_MAC_ADDRESS             *SrcAddr  OPTIONAL,
  IN EFI_MAC_ADDRESS             *DestAddr OPTIONAL,
  IN UINT16                      *Protocol OPTIONAL
  )
{
  ETHER_HEAD              *Header;
  EFI_SIMPLE_NETWORK_MODE *Mode;
  USB_CDC_ETHERNET_PRIVATE_DATA    *PrivateData;
  EFI_USB_IO_PROTOCOL     *UsbIo;
  EFI_STATUS              Status;
  UINTN                   TransferLength;
  UINT32                  TransferStatus;
  UINT16                  Type;
  UINTN                   EndpointAddress;
  UINT8                   TransferCount;
  UINTN                   TempLength;

  if (This == NULL
      || This->Mode == NULL
      || Buffer == NULL
      || BufferSize < This->Mode->MediaHeaderSize
      || BufferSize > MAX_ETHERNET_PKT_SIZE
      || (HeaderSize != 0 && (DestAddr == NULL || Protocol == NULL))
      || (HeaderSize != 0 && This->Mode->MediaHeaderSize != HeaderSize)) {
    return EFI_INVALID_PARAMETER;
  }

  Mode = This->Mode;
  PrivateData = USB_CDC_ETHERNET_PRIVATE_DATA_FROM_THIS_SNP (This);

  if (!Mode->MediaPresent) {
    //
    // Don't bother transmitting if there's no link.
    //
    return EFI_NOT_READY;
  }

  if (Mode->State == EfiSimpleNetworkStopped) {
    return EFI_NOT_STARTED;
  }
  if (Mode->State != EfiSimpleNetworkInitialized) {
    return EFI_DEVICE_ERROR;
  }

  //
  //  Copy the packet into the USB buffer
  //
  CopyMem (PrivateData->BulkOutBuffer, Buffer, BufferSize);

  //
  //  Transmit the packet
  //
  Header = (ETHER_HEAD *)PrivateData->BulkOutBuffer;
  if (HeaderSize != 0) {
    if (DestAddr != NULL) {
      CopyMem (&Header->DstMac, DestAddr, NET_ETHER_ADDR_LEN);
    }
    if (SrcAddr != NULL) {
      CopyMem (&Header->SrcMac, SrcAddr, NET_ETHER_ADDR_LEN);
    } else {
      CopyMem (&Header->SrcMac, &Mode->CurrentAddress.Addr[0], NET_ETHER_ADDR_LEN);
    }
    if (Protocol != NULL) {
      Type = *Protocol;
    } else {
      Type = BufferSize;
    }
    Header->EtherType = NTOHS (Type);
  }

  TempLength = BufferSize;
  TransferLength = 0;
  for (TransferCount = 0; TransferCount < (BufferSize / USB_CDC_ECM_DATA_PACKET_SIZE_MAX + 1); TransferCount++) {
    TempLength -= TransferLength;
    if (TempLength > USB_CDC_ECM_DATA_PACKET_SIZE_MAX) {
      TransferLength = USB_CDC_ECM_DATA_PACKET_SIZE_MAX;
    } else {
      TransferLength = TempLength;
    }

    UsbIo = PrivateData->UsbDataIo;
    EndpointAddress = PrivateData->UsbCdcDesc.UsbCdcOutEndpointDesc.EndpointAddress;
    Status = UsbIo->UsbBulkTransfer (
                      UsbIo,
                      EndpointAddress,
                      (PrivateData->BulkOutBuffer + USB_CDC_ECM_DATA_PACKET_SIZE_MAX * TransferCount),
                      &TransferLength,
                      USB_CDC_ECM_BULK_TRANSFER_TIMEOUT,
                      &TransferStatus);

    if (EFI_ERROR (Status) && EFI_ERROR (TransferStatus)) {
      return EFI_DEVICE_ERROR;
    }
  }

  if (!EFI_ERROR (Status) && !EFI_ERROR (TransferStatus)) {
    PrivateData->TxBuffer = Buffer;
  }

  return EFI_SUCCESS;
}


/**
  Set up the simple network protocol.

  @param[in]  PrivateData     Points to USB CDC Ethernet private data.

  @retval EFI_SUCCESS         Setup was successful

**/
EFI_STATUS
UsbCdcEthernetSnpSetup (
  IN USB_CDC_ETHERNET_PRIVATE_DATA *PrivateData
  )
{
  EFI_SIMPLE_NETWORK_PROTOCOL *Snp;
  EFI_SIMPLE_NETWORK_MODE     *SnpMode;
  EFI_STATUS                  Status;

  //
  // Initialize the simple network protocol
  //
  Snp = &PrivateData->Snp;
  Snp->Revision = EFI_SIMPLE_NETWORK_PROTOCOL_REVISION;
  Snp->Start = SnpStart;
  Snp->Stop = SnpStop;
  Snp->Initialize = SnpInitialize;
  Snp->Reset = SnpReset;
  Snp->Shutdown = SnpShutdown;
  Snp->ReceiveFilters = SnpReceiveFilters;
  Snp->StationAddress = SnpStationAddress;
  Snp->Statistics = SnpStatistics;
  Snp->MCastIpToMac = SnpMCastIPtoMAC;
  Snp->NvData = SnpNvData;
  Snp->GetStatus = SnpGetStatus;
  Snp->Transmit = SnpTransmit;
  Snp->Receive = SnpReceive;
  Snp->WaitForPacket = NULL;

  SnpMode = &PrivateData->SnpMode;
  Snp->Mode = SnpMode;
  ZeroMem (SnpMode, sizeof (EFI_SIMPLE_NETWORK_MODE));

  SnpMode->State = EfiSimpleNetworkStopped;
  SnpMode->HwAddressSize = NET_ETHER_ADDR_LEN;
  SnpMode->MediaHeaderSize = sizeof (ETHER_HEAD);
  SnpMode->MaxPacketSize = MAX_ETHERNET_PKT_SIZE;
  SnpMode->ReceiveFilterMask = EFI_SIMPLE_NETWORK_RECEIVE_UNICAST
                           | EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST
                           | EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS;
  SnpMode->ReceiveFilterSetting = EFI_SIMPLE_NETWORK_RECEIVE_UNICAST
                              | EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST;
  SnpMode->MaxMCastFilterCount = 0;
  SnpMode->MCastFilterCount = 0;
  SnpMode->NvRamSize = 0; // No NVRAM
  SnpMode->NvRamAccessSize = 0;
  SetMem (
    &SnpMode->BroadcastAddress,
    NET_ETHER_ADDR_LEN,
    0xFF
    );

  SnpMode->IfType = NET_IFTYPE_ETHERNET;
  SnpMode->MacAddressChangeable = TRUE;
  SnpMode->MultipleTxSupported = FALSE;
  SnpMode->MediaPresentSupported = TRUE;
  SnpMode->MediaPresent = FALSE;

  PrivateData->LinkUp = FALSE;
  PrivateData->TxBuffer = NULL;
  PrivateData->BulkInLength = 0;

  //
  //  Read the MAC address
  //
  Status = UsbCdcMacAddressGet (
             PrivateData,
             &SnpMode->PermanentAddress.Addr[0]
             );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  //
  //  Use the hardware address as the current address
  //
  CopyMem (
    &SnpMode->CurrentAddress,
    &SnpMode->PermanentAddress,
    NET_ETHER_ADDR_LEN
    );

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  MAX_ETHERNET_PKT_SIZE,
                  (VOID **)&PrivateData->BulkInBuffer
                  );

  if (EFI_ERROR (Status)) {
    gBS->FreePool (PrivateData->BulkInBuffer);
    return Status;
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  MAX_ETHERNET_PKT_SIZE,
                  (VOID **)&PrivateData->BulkOutBuffer
                  );

  if (EFI_ERROR (Status)) {
    gBS->FreePool (PrivateData->BulkOutBuffer);
    return Status;
  }

  return Status;
}
