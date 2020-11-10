/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#if !defined _MM_COMMUNICATION_LIB_H_
#define _MM_COMMUNICATION_LIB_H_

EFI_STATUS
EFIAPI
MmCommunicationCommunicate (
  IN OUT VOID                             *CommBuffer,
  IN OUT UINTN                            *CommSize OPTIONAL
  );

#endif /* _MM_COMMUNICATION_LIB_H_ */
