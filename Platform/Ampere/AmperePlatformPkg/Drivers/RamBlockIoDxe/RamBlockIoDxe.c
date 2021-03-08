/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/DiskIo.h>

typedef struct _RAMDISK_BLOCKIO_INSTANCE  RAMDISK_BLOCKIO_INSTANCE;

const EFI_GUID gRamBlockIoGuid = { 0xc84d8a80, 0xcc28, 0x4cea, { 0x9b, 0xc5, 0x1f, 0x9a, 0xb5, 0x00, 0xa0, 0x77 } };

/* Start address of the Ram Block Io */
#define RAM_BLOCKIO_START_ADDRESS   0x80000000000

/* Size of the Ram Block Io */
#define RAM_BLOCKIO_SIZE            0x32000000

/* The size of a block. */
#define RAM_BLOCKIO_BLOCKSIZE       0x200


typedef struct {
  VENDOR_DEVICE_PATH                  Vendor;
  EFI_DEVICE_PATH_PROTOCOL            End;
} RAMDISK_BLOCKIO_DEVICE_PATH;

#define RAMDISK_BLOCKIO_SIGNATURE     SIGNATURE_32('r', 'b', 'i', 'o')
#define INSTANCE_FROM_BLKIO_THIS(a)   CR(a, RAMDISK_BLOCKIO_INSTANCE, BlockIoProtocol, RAMDISK_BLOCKIO_SIGNATURE)

struct _RAMDISK_BLOCKIO_INSTANCE {
  UINT32                              Signature;
  EFI_HANDLE                          Handle;

  UINTN                               StartAddress;
  UINTN                               Size;

  EFI_LBA                             StartLba;

  EFI_BLOCK_IO_PROTOCOL               BlockIoProtocol;
  EFI_BLOCK_IO_MEDIA                  Media;

  RAMDISK_BLOCKIO_DEVICE_PATH         DevicePath;
};

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.Reset
//
EFI_STATUS
EFIAPI
RamBlockIoReset (
  IN EFI_BLOCK_IO_PROTOCOL    *This,
  IN BOOLEAN                  ExtendedVerification
  );

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.ReadBlocks
//
EFI_STATUS
EFIAPI
RamBlockIoReadBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL   *This,
  IN  UINT32                  MediaId,
  IN  EFI_LBA                 Lba,
  IN  UINTN                   BufferSizeInBytes,
  OUT VOID                    *Buffer
);

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.WriteBlocks
//
EFI_STATUS
EFIAPI
RamBlockIoWriteBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL   *This,
  IN  UINT32                  MediaId,
  IN  EFI_LBA                 Lba,
  IN  UINTN                   BufferSizeInBytes,
  IN  VOID                    *Buffer
);

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.FlushBlocks
//
EFI_STATUS
EFIAPI
RamBlockIoFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL    *This
);

RAMDISK_BLOCKIO_INSTANCE  mRamBlockIoInstanceTemplate = {
  RAMDISK_BLOCKIO_SIGNATURE, // Signature
  NULL, // Handle ... NEED TO BE FILLED

  0, // StartAddress ... NEED TO BE FILLED
  0, // Size ... NEED TO BE FILLED
  0, // StartLba

  {
    EFI_BLOCK_IO_PROTOCOL_REVISION2, // Revision
    NULL, // Media ... NEED TO BE FILLED
    RamBlockIoReset, // Reset;
    RamBlockIoReadBlocks,          // ReadBlocks
    RamBlockIoWriteBlocks,         // WriteBlocks
    RamBlockIoFlushBlocks          // FlushBlocks
  }, // BlockIoProtocol

  {
    0, // MediaId ... NEED TO BE FILLED
    FALSE, // RemovableMedia
    TRUE, // MediaPresent
    FALSE, // LogicalPartition
    FALSE, // ReadOnly
    FALSE, // WriteCaching;
    0, // BlockSize ... NEED TO BE FILLED
    4, //  IoAlign
    0, // LastBlock ... NEED TO BE FILLED
    0, // LowestAlignedLba
    1, // LogicalBlocksPerPhysicalBlock
  }, //Media;

  {
    {
      {
        HARDWARE_DEVICE_PATH,
        HW_VENDOR_DP,
        { (UINT8) sizeof (VENDOR_DEVICE_PATH), (UINT8) ((sizeof (VENDOR_DEVICE_PATH)) >> 8) }
      },
      { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }, // GUID ... NEED TO BE FILLED
    },
    {
      END_DEVICE_PATH_TYPE,
      END_ENTIRE_DEVICE_PATH_SUBTYPE,
      { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 }
    }
    } // DevicePath
};

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.Reset
//
EFI_STATUS
EFIAPI
RamBlockIoReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.ReadBlocks
//
EFI_STATUS
EFIAPI
RamBlockIoReadBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL   *This,
  IN  UINT32                  MediaId,
  IN  EFI_LBA                 Lba,
  IN  UINTN                   BufferSizeInBytes,
  OUT VOID                    *Buffer
  )
{
  RAMDISK_BLOCKIO_INSTANCE  *Instance;
  EFI_STATUS                Status;
  EFI_BLOCK_IO_MEDIA        *Media;
  UINT32                    NumBlocks;

  DEBUG ((DEBUG_BLKIO,
          "%a (MediaId=0x%x, Lba=%ld, BufferSize=0x%x bytes (%d kB), BufferPtr @ 0x%08x)\n",
          __FUNCTION__,
          MediaId,
          Lba,
          BufferSizeInBytes,
          BufferSizeInBytes,
          Buffer));

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Instance = INSTANCE_FROM_BLKIO_THIS (This);

  // The buffer must be valid
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Return if we have not any byte to read
  if (BufferSizeInBytes == 0) {
    return EFI_SUCCESS;
  }

  // The size of the buffer must be a multiple of the block size
  if ((BufferSizeInBytes % Instance->Media.BlockSize) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // All blocks must be within the device
  NumBlocks = ((UINT32) BufferSizeInBytes) / Instance->Media.BlockSize ;

  if ((Lba + NumBlocks) > (Instance->Media.LastBlock + 1)) {
    DEBUG ((DEBUG_ERROR, "%a: Read will exceed last block\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Media = This->Media;

  if (Media == NULL) {
    Status = EFI_INVALID_PARAMETER;
  } else if (!Media->MediaPresent) {
    Status = EFI_NO_MEDIA;
  } else if (Media->MediaId != MediaId) {
    Status = EFI_MEDIA_CHANGED;
  } else if ((Media->IoAlign > 2) && (((UINTN)Buffer & (Media->IoAlign - 1)) != 0)) {
    Status = EFI_INVALID_PARAMETER;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Read from address %p\n",
            __FUNCTION__,
            (VOID *)(Instance->StartAddress + Lba * Instance->Media.BlockSize)));
    CopyMem (Buffer, (VOID *)(Instance->StartAddress + Lba * Instance->Media.BlockSize), BufferSizeInBytes);
    Status = EFI_SUCCESS;
  }

  return Status;
}

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.WriteBlocks
//
EFI_STATUS
EFIAPI
RamBlockIoWriteBlocks (
  IN  EFI_BLOCK_IO_PROTOCOL   *This,
  IN  UINT32                  MediaId,
  IN  EFI_LBA                 Lba,
  IN  UINTN                   BufferSizeInBytes,
  IN  VOID                    *Buffer
  )
{
  RAMDISK_BLOCKIO_INSTANCE  *Instance;
  UINT32                    NumBlocks = 0;
  EFI_STATUS                Status;

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Instance = INSTANCE_FROM_BLKIO_THIS (This);

  DEBUG((DEBUG_BLKIO,
        "%a: NumBlocks=%d, LastBlock=%ld, Lba=%ld.\n",
        __FUNCTION__,
        NumBlocks,
        Instance->Media.LastBlock,
        Lba));

  // The buffer must be valid
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Instance->Media.ReadOnly == TRUE) {
    return EFI_WRITE_PROTECTED;
  }

  // We must have some bytes to read
  DEBUG ((DEBUG_BLKIO, "%a: BufferSizeInBytes=0x%x\n", __FUNCTION__, BufferSizeInBytes));
  if (BufferSizeInBytes == 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // The size of the buffer must be a multiple of the block size
  DEBUG ((DEBUG_BLKIO, "%a: BlockSize in bytes =0x%x\n", __FUNCTION__, Instance->Media.BlockSize));
  if ((BufferSizeInBytes % Instance->Media.BlockSize) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  // All blocks must be within the device
  NumBlocks = ((UINT32)BufferSizeInBytes) / Instance->Media.BlockSize ;

  if ((Lba + NumBlocks) > (Instance->Media.LastBlock + 1)) {
    DEBUG ((DEBUG_ERROR, "%a: Write will exceed last block.\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_BLKIO,
          "%a (MediaId=0x%x, Lba=%ld, BufferSize=0x%x bytes (%d kB), BufferPtr @ 0x%08x)\n",
          __FUNCTION__,
          MediaId,
          Lba,
          BufferSizeInBytes,
          Buffer));

  if(!This->Media->MediaPresent) {
    Status = EFI_NO_MEDIA;
  } else if(This->Media->MediaId != MediaId) {
    Status = EFI_MEDIA_CHANGED;
  } else if(This->Media->ReadOnly) {
    Status = EFI_WRITE_PROTECTED;
  } else {
    CopyMem ((VOID *)(Instance->StartAddress + Lba * Instance->Media.BlockSize), Buffer, BufferSizeInBytes);
    Status = EFI_SUCCESS;
  }

  return Status;
}

//
// BlockIO Protocol function EFI_BLOCK_IO_PROTOCOL.FlushBlocks
//
EFI_STATUS
EFIAPI
RamBlockIoFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
RamBlockIoCreateInstance (
  IN UINT32                 MediaId,
  IN UINT64                 StartAddress,
  IN UINT32                 Size,
  IN UINT32                 BlockSize,
  IN CONST GUID             *Guid
  )
{
  EFI_STATUS                  Status;
  RAMDISK_BLOCKIO_INSTANCE*   Instance;

  Instance = AllocateCopyPool (sizeof (RAMDISK_BLOCKIO_INSTANCE), &mRamBlockIoInstanceTemplate);
  if (Instance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Instance->StartAddress = StartAddress;
  Instance->Size = Size;
  Instance->BlockIoProtocol.Media = &Instance->Media;
  Instance->Media.MediaId = MediaId;
  Instance->Media.BlockSize = BlockSize;
  Instance->Media.LastBlock = (Instance->Size / BlockSize) - 1;

  CopyGuid (&Instance->DevicePath.Vendor.Guid, Guid);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Instance->Handle,
                  &gEfiDevicePathProtocolGuid, &Instance->DevicePath,
                  &gEfiBlockIoProtocolGuid,  &Instance->BlockIoProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    FreePool (Instance);
    return Status;
  }

  return Status;
}

EFI_STATUS
EFIAPI
RamBlockIoInitialise (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS              Status;

  Status = RamBlockIoCreateInstance (
             0,
             RAM_BLOCKIO_START_ADDRESS,
             RAM_BLOCKIO_SIZE,
             RAM_BLOCKIO_BLOCKSIZE,
             &gRamBlockIoGuid
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,
            "%a: Fail to create instance for Ramdisk BlockIo\n",
            __FUNCTION__));
  }

  return Status;
}
