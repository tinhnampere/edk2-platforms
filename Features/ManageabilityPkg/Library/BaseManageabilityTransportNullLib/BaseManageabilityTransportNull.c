/** @file
  Null instance of Manageability Transport Library

  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/ManageabilityTransportLib.h>

/**
  This function acquires to create a transport session to transmit manageability
  packet. A transport token is returned to caller for the follow up operations.

  @param [in]   ManageabilityProtocolSpec  The protocol spec the transport interface is acquired.
  @param [out]  TransportToken             The pointer to receive the transport token created by
                                           the target transport interface library.
  @retval       EFI_SUCCESS                Token is created successfully.
  @retval       EFI_OUT_OF_RESOURCES       Out of resource to create a new transport session.
  @retval       EFI_UNSUPPORTED            Protocol is not supported on this transport interface.
  @retval       Otherwise                  Other errors.

**/
EFI_STATUS
AcquireTransportSession (
  IN  EFI_GUID                       *ManageabilityProtocolSpec,
  OUT MANAGEABILITY_TRANSPORT_TOKEN  **TransportToken
  )
{
  return EFI_UNSUPPORTED;
}

/**
  This function returns the transport capabilities.

  @param [out]  TransportFeature        Pointer to receive transport capabilities.
                                        See the definitions of
                                        MANAGEABILITY_TRANSPORT_CAPABILITY.

**/
VOID
GetTransportCapability (
  OUT MANAGEABILITY_TRANSPORT_CAPABILITY  *TransportCapability
  )
{
  *TransportCapability = 0;
}

/**
  This function releases the manageability session.

  @param [in]  TransportToken   The transport token acquired through
                                AcquireTransportSession.
  @retval      EFI_SUCCESS      Token is released successfully.
               Otherwise        Other errors.

**/
EFI_STATUS
ReleaseTransportSession (
  IN MANAGEABILITY_TRANSPORT_TOKEN  *TransportToken
  )
{
  return EFI_SUCCESS;
}
