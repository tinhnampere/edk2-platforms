/** @file

    IPMI Manageability PPI internal header file.

  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MANAGEABILITY_IPMI_PPI_INTERNAL_H_
#define MANAGEABILITY_IPMI_PPI_INTERNAL_H_

#include <Library/ManageabilityTransportLib.h>
#include <Ppi/IpmiPpi.h>

#define MANAGEABILITY_IPMI_PPI_INTERNAL_SIGNATURE  SIGNATURE_32 ('I', 'P', 'P', 'I')

#define MANAGEABILITY_IPMI_PPI_INTERNAL_FROM_LINK(a)  CR (a, PEI_IPMI_PPI_INTERNAL, PeiIpmiPpi, MANAGEABILITY_IPMI_PPI_INTERNAL_SIGNATURE)

typedef struct {
  UINT32                         Signature;
  MANAGEABILITY_TRANSPORT_TOKEN  *TransportToken;
  PEI_IPMI_PPI                   PeiIpmiPpi;
} PEI_IPMI_PPI_INTERNAL;

#endif // MANAGEABILITY_IPMI_PPI_INTERNAL_H_
