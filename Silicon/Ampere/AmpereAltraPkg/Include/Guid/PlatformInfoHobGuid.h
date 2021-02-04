/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_INFO_HOB_GUID_H_
#define PLATFORM_INFO_HOB_GUID_H_

#define PLATFORM_INFO_HOB_GUID \
  { 0xa39d5143, 0x964a, 0x4ebe, { 0xb1, 0xa0, 0xcd, 0xd4, 0xa6, 0xf2, 0x18, 0x3a } }

extern EFI_GUID gPlatformHobGuid;

#define PLATFORM_INFO_HOB_GUID_V2 \
  { 0x7f73e372, 0x7183, 0x4022, { 0xb3, 0x76, 0x78, 0x30, 0x32, 0x6d, 0x79, 0xb4 } }

extern EFI_GUID gPlatformHobV2Guid;

#endif /* PLATFORM_INFO_HOB_GUID_H_ */
