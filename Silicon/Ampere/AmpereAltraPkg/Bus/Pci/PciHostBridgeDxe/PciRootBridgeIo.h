/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PCI_ROOT_BRIDGE_IO_H_
#define PCI_ROOT_BRIDGE_IO_H_

#include <IndustryStandard/Pci.h>
#include <Library/PciLib.h>

/**
   Polls an address in memory mapped I/O space until an exit condition is met, or
   a timeout occurs.

   This function provides a standard way to poll a PCI memory location. A PCI memory read
   operation is performed at the PCI memory address specified by Address for the width specified
   by Width. The result of this PCI memory read operation is stored in Result. This PCI memory
   read operation is repeated until either a timeout of Delay 100 ns units has expired, or (Result &
   Mask) is equal to Value.

   @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in]   Width     Signifies the width of the memory operations.
   @param[in]   Address   The base address of the memory operations. The caller is
   responsible for aligning Address if required.
   @param[in]   Mask      Mask used for the polling criteria. Bytes above Width in Mask
   are ignored. The bits in the bytes below Width which are zero in
   Mask are ignored when polling the memory address.
   @param[in]   Value     The comparison value used for the polling exit criteria.
   @param[in]   Delay     The number of 100 ns units to poll. Note that timer available may
   be of poorer granularity.
   @param[out]  Result    Pointer to the last value read from the memory location.

   @retval EFI_SUCCESS            The last data returned from the access matched the poll exit criteria.
   @retval EFI_INVALID_PARAMETER  Width is invalid.
   @retval EFI_INVALID_PARAMETER  Result is NULL.
   @retval EFI_TIMEOUT            Delay expired before a match occurred.
   @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
RootBridgeIoPollMem (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN  UINT64                                Address,
  IN  UINT64                                Mask,
  IN  UINT64                                Value,
  IN  UINT64                                Delay,
  OUT UINT64                                *Result
  );

/**
   Reads from the I/O space of a PCI Root Bridge. Returns when either the polling exit criteria is
   satisfied or after a defined duration.

   This function provides a standard way to poll a PCI I/O location. A PCI I/O read operation is
   performed at the PCI I/O address specified by Address for the width specified by Width.
   The result of this PCI I/O read operation is stored in Result. This PCI I/O read operation is
   repeated until either a timeout of Delay 100 ns units has expired, or (Result & Mask) is equal
   to Value.

   @param[in] This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in] Width     Signifies the width of the I/O operations.
   @param[in] Address   The base address of the I/O operations. The caller is responsible
   for aligning Address if required.
   @param[in] Mask      Mask used for the polling criteria. Bytes above Width in Mask
   are ignored. The bits in the bytes below Width which are zero in
   Mask are ignored when polling the I/O address.
   @param[in] Value     The comparison value used for the polling exit criteria.
   @param[in] Delay     The number of 100 ns units to poll. Note that timer available may
   be of poorer granularity.
   @param[out] Result   Pointer to the last value read from the memory location.

   @retval EFI_SUCCESS            The last data returned from the access matched the poll exit criteria.
   @retval EFI_INVALID_PARAMETER  Width is invalid.
   @retval EFI_INVALID_PARAMETER  Result is NULL.
   @retval EFI_TIMEOUT            Delay expired before a match occurred.
   @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
RootBridgeIoPollIo (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN  UINT64                                Address,
  IN  UINT64                                Mask,
  IN  UINT64                                Value,
  IN  UINT64                                Delay,
  OUT UINT64                                *Result
  );

/**
   Enables a PCI driver to access PCI controller registers in the PCI root bridge memory space.

   The Mem.Read(), and Mem.Write() functions enable a driver to access PCI controller
   registers in the PCI root bridge memory space.
   The memory operations are carried out exactly as requested. The caller is responsible for satisfying
   any alignment and memory width restrictions that a PCI Root Bridge on a platform might require.

   @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in]   Width     Signifies the width of the memory operation.
   @param[in]   Address   The base address of the memory operation. The caller is
   responsible for aligning the Address if required.
   @param[in]   Count     The number of memory operations to perform. Bytes moved is
   Width size * Count, starting at Address.
   @param[out]  Buffer    For read operations, the destination buffer to store the results. For
   write operations, the source buffer to write data from.

   @retval EFI_SUCCESS            The data was read from or written to the PCI root bridge.
   @retval EFI_INVALID_PARAMETER  Width is invalid for this PCI root bridge.
   @retval EFI_INVALID_PARAMETER  Buffer is NULL.
   @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
RootBridgeIoMemRead (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN  UINT64                                Address,
  IN  UINTN                                 Count,
  OUT VOID                                  *Buffer
  );

/**
   Enables a PCI driver to access PCI controller registers in the PCI root bridge memory space.

   The Mem.Read(), and Mem.Write() functions enable a driver to access PCI controller
   registers in the PCI root bridge memory space.
   The memory operations are carried out exactly as requested. The caller is responsible for satisfying
   any alignment and memory width restrictions that a PCI Root Bridge on a platform might require.

   @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in]   Width     Signifies the width of the memory operation.
   @param[in]   Address   The base address of the memory operation. The caller is
   responsible for aligning the Address if required.
   @param[in]   Count     The number of memory operations to perform. Bytes moved is
   Width size * Count, starting at Address.
   @param[in]   Buffer    For read operations, the destination buffer to store the results. For
   write operations, the source buffer to write data from.

   @retval EFI_SUCCESS            The data was read from or written to the PCI root bridge.
   @retval EFI_INVALID_PARAMETER  Width is invalid for this PCI root bridge.
   @retval EFI_INVALID_PARAMETER  Buffer is NULL.
   @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a lack of resources.
**/
EFI_STATUS
EFIAPI
RootBridgeIoMemWrite (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN UINT64                                Address,
  IN UINTN                                 Count,
  IN VOID                                  *Buffer
  );

/**
   Enables a PCI driver to access PCI controller registers in the PCI root bridge I/O space.

   @param[in]   This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in]   Width       Signifies the width of the memory operations.
   @param[in]   UserAddress The base address of the I/O operation. The caller is responsible for
   aligning the Address if required.
   @param[in]   Count       The number of I/O operations to perform. Bytes moved is Width
   size * Count, starting at Address.
   @param[out]  UserBuffer  For read operations, the destination buffer to store the results. For
   write operations, the source buffer to write data from.

   @retval EFI_SUCCESS              The data was read from or written to the PCI root bridge.
   @retval EFI_INVALID_PARAMETER    Width is invalid for this PCI root bridge.
   @retval EFI_INVALID_PARAMETER    Buffer is NULL.
   @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
RootBridgeIoIoRead (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN  UINT64                                UserAddress,
  IN  UINTN                                 Count,
  OUT VOID                                  *UserBuffer
  );

/**
   Enables a PCI driver to access PCI controller registers in the PCI root bridge I/O space.

   @param[in]   This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in]   Width       Signifies the width of the memory operations.
   @param[in]   UserAddress The base address of the I/O operation. The caller is responsible for
   aligning the Address if required.
   @param[in]   Count       The number of I/O operations to perform. Bytes moved is Width
   size * Count, starting at Address.
   @param[in]   UserBuffer  For read operations, the destination buffer to store the results. For
   write operations, the source buffer to write data from.

   @retval EFI_SUCCESS              The data was read from or written to the PCI root bridge.
   @retval EFI_INVALID_PARAMETER    Width is invalid for this PCI root bridge.
   @retval EFI_INVALID_PARAMETER    Buffer is NULL.
   @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
RootBridgeIoIoWrite (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN UINT64                                UserAddress,
  IN UINTN                                 Count,
  IN VOID                                  *UserBuffer
  );

/**
   Enables a PCI driver to copy one region of PCI root bridge memory space to another region of PCI
   root bridge memory space.

   The CopyMem() function enables a PCI driver to copy one region of PCI root bridge memory
   space to another region of PCI root bridge memory space. This is especially useful for video scroll
   operation on a memory mapped video buffer.
   The memory operations are carried out exactly as requested. The caller is responsible for satisfying
   any alignment and memory width restrictions that a PCI root bridge on a platform might require.

   @param[in] This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL instance.
   @param[in] Width       Signifies the width of the memory operations.
   @param[in] DestAddress The destination address of the memory operation. The caller is
   responsible for aligning the DestAddress if required.
   @param[in] SrcAddress  The source address of the memory operation. The caller is
   responsible for aligning the SrcAddress if required.
   @param[in] Count       The number of memory operations to perform. Bytes moved is
   Width size * Count, starting at DestAddress and SrcAddress.

   @retval  EFI_SUCCESS             The data was copied from one memory region to another memory region.
   @retval  EFI_INVALID_PARAMETER   Width is invalid for this PCI root bridge.
   @retval  EFI_OUT_OF_RESOURCES    The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
RootBridgeIoCopyMem (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN UINT64                                DestAddress,
  IN UINT64                                SrcAddress,
  IN UINTN                                 Count
  );

/**
   Enables a PCI driver to access PCI controller registers in a PCI root bridge's configuration space.

   The Pci.Read() and Pci.Write() functions enable a driver to access PCI configuration
   registers for a PCI controller.
   The PCI Configuration operations are carried out exactly as requested. The caller is responsible for
   any alignment and PCI configuration width issues that a PCI Root Bridge on a platform might
   require.

   @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in]   Width     Signifies the width of the memory operations.
   @param[in]   Address   The address within the PCI configuration space for the PCI controller.
   @param[in]   Count     The number of PCI configuration operations to perform. Bytes
   moved is Width size * Count, starting at Address.
   @param[out]  Buffer    For read operations, the destination buffer to store the results. For
   write operations, the source buffer to write data from.

   @retval EFI_SUCCESS            The data was read from or written to the PCI root bridge.
   @retval EFI_INVALID_PARAMETER  Width is invalid for this PCI root bridge.
   @retval EFI_INVALID_PARAMETER  Buffer is NULL.
   @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
RootBridgeIoPciRead (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN  UINT64                                Address,
  IN  UINTN                                 Count,
  OUT VOID                                  *Buffer
  );

/**
   Enables a PCI driver to access PCI controller registers in a PCI root bridge's configuration space.

   The Pci.Read() and Pci.Write() functions enable a driver to access PCI configuration
   registers for a PCI controller.
   The PCI Configuration operations are carried out exactly as requested. The caller is responsible for
   any alignment and PCI configuration width issues that a PCI Root Bridge on a platform might
   require.

   @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in]   Width     Signifies the width of the memory operations.
   @param[in]   Address   The address within the PCI configuration space for the PCI controller.
   @param[in]   Count     The number of PCI configuration operations to perform. Bytes
   moved is Width size * Count, starting at Address.
   @param[in]   Buffer    For read operations, the destination buffer to store the results. For
   write operations, the source buffer to write data from.

   @retval EFI_SUCCESS            The data was read from or written to the PCI root bridge.
   @retval EFI_INVALID_PARAMETER  Width is invalid for this PCI root bridge.
   @retval EFI_INVALID_PARAMETER  Buffer is NULL.
   @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
RootBridgeIoPciWrite (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL       *This,
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH Width,
  IN UINT64                                Address,
  IN UINTN                                 Count,
  IN VOID                                  *Buffer
  );

/**
   Provides the PCI controller-specific addresses required to access system memory from a
   DMA bus master.

   The Map() function provides the PCI controller specific addresses needed to access system
   memory. This function is used to map system memory for PCI bus master DMA accesses.

   @param[in]       This            A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in]       Operation       Indicates if the bus master is going to read or write to system memory.
   @param[in]       HostAddress     The system memory address to map to the PCI controller.
   @param[in, out]  NumberOfBytes   On input the number of bytes to map. On output the number of bytes that were mapped.
   @param[out]      DeviceAddress   The resulting map address for the bus master PCI controller to use
   to access the system memory's HostAddress.
   @param[out]      Mapping         The value to pass to Unmap() when the bus master DMA operation is complete.

   @retval EFI_SUCCESS            The range was mapped for the returned NumberOfBytes.
   @retval EFI_INVALID_PARAMETER  Operation is invalid.
   @retval EFI_INVALID_PARAMETER  HostAddress is NULL.
   @retval EFI_INVALID_PARAMETER  NumberOfBytes is NULL.
   @retval EFI_INVALID_PARAMETER  DeviceAddress is NULL.
   @retval EFI_INVALID_PARAMETER  Mapping is NULL.
   @retval EFI_UNSUPPORTED        The HostAddress cannot be mapped as a common buffer.
   @retval EFI_DEVICE_ERROR       The system hardware could not map the requested address.
   @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a lack of resources.

**/
EFI_STATUS
EFIAPI
RootBridgeIoMap (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL           *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_OPERATION Operation,
  IN     VOID                                      *HostAddress,
  IN OUT UINTN                                     *NumberOfBytes,
  OUT    EFI_PHYSICAL_ADDRESS                      *DeviceAddress,
  OUT    VOID                                      **Mapping
  );

/**
   Completes the Map() operation and releases any corresponding resources.

   The Unmap() function completes the Map() operation and releases any corresponding resources.
   If the operation was an EfiPciOperationBusMasterWrite or
   EfiPciOperationBusMasterWrite64, the data is committed to the target system memory.
   Any resources used for the mapping are freed.

   @param[in] This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in] Mapping   The mapping value returned from Map().

   @retval EFI_SUCCESS            The range was unmapped.
   @retval EFI_INVALID_PARAMETER  Mapping is not a value that was returned by Map().
   @retval EFI_DEVICE_ERROR       The data was not committed to the target system memory.

**/
EFI_STATUS
EFIAPI
RootBridgeIoUnmap (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *This,
  IN VOID                            *Mapping
  );

/**
   Allocates pages that are suitable for an EfiPciOperationBusMasterCommonBuffer or
   EfiPciOperationBusMasterCommonBuffer64 mapping.

   @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param Type        This parameter is not used and must be ignored.
   @param MemoryType  The type of memory to allocate, EfiBootServicesData or EfiRuntimeServicesData.
   @param Pages       The number of pages to allocate.
   @param HostAddress A pointer to store the base system memory address of the allocated range.
   @param Attributes  The requested bit mask of attributes for the allocated range. Only
   the attributes EFI_PCI_ATTRIBUTE_MEMORY_WRITE_COMBINE, EFI_PCI_ATTRIBUTE_MEMORY_CACHED,
   and EFI_PCI_ATTRIBUTE_DUAL_ADDRESS_CYCLE may be used with this function.

   @retval EFI_SUCCESS            The requested memory pages were allocated.
   @retval EFI_INVALID_PARAMETER  MemoryType is invalid.
   @retval EFI_INVALID_PARAMETER  HostAddress is NULL.
   @retval EFI_UNSUPPORTED        Attributes is unsupported. The only legal attribute bits are
   MEMORY_WRITE_COMBINE, MEMORY_CACHED, and DUAL_ADDRESS_CYCLE.
   @retval EFI_OUT_OF_RESOURCES   The memory pages could not be allocated.

**/
EFI_STATUS
EFIAPI
RootBridgeIoAllocateBuffer (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *This,
  IN  EFI_ALLOCATE_TYPE               Type,
  IN  EFI_MEMORY_TYPE                 MemoryType,
  IN  UINTN                           Pages,
  OUT VOID                            **HostAddress,
  IN  UINT64                          Attributes
  );

/**
   Frees memory that was allocated with AllocateBuffer().

   The FreeBuffer() function frees memory that was allocated with AllocateBuffer().

   @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param Pages       The number of pages to free.
   @param HostAddress The base system memory address of the allocated range.

   @retval EFI_SUCCESS            The requested memory pages were freed.
   @retval EFI_INVALID_PARAMETER  The memory range specified by HostAddress and Pages
   was not allocated with AllocateBuffer().

**/
EFI_STATUS
EFIAPI
RootBridgeIoFreeBuffer (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *This,
  IN  UINTN                           Pages,
  OUT VOID                            *HostAddress
  );

/**
   Flushes all PCI posted write transactions from a PCI host bridge to system memory.

   The Flush() function flushes any PCI posted write transactions from a PCI host bridge to system
   memory. Posted write transactions are generated by PCI bus masters when they perform write
   transactions to target addresses in system memory.
   This function does not flush posted write transactions from any PCI bridges. A PCI controller
   specific action must be taken to guarantee that the posted write transactions have been flushed from
   the PCI controller and from all the PCI bridges into the PCI host bridge. This is typically done with
   a PCI read transaction from the PCI controller prior to calling Flush().

   @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.

   @retval EFI_SUCCESS        The PCI posted write transactions were flushed from the PCI host
   bridge to system memory.
   @retval EFI_DEVICE_ERROR   The PCI posted write transactions were not flushed from the PCI
   host bridge due to a hardware error.

**/
EFI_STATUS
EFIAPI
RootBridgeIoFlush (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *This
  );

/**
   Gets the attributes that a PCI root bridge supports setting with SetAttributes(), and the
   attributes that a PCI root bridge is currently using.

   The GetAttributes() function returns the mask of attributes that this PCI root bridge supports
   and the mask of attributes that the PCI root bridge is currently using.

   @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param Supported   A pointer to the mask of attributes that this PCI root bridge
   supports setting with SetAttributes().
   @param Attributes  A pointer to the mask of attributes that this PCI root bridge is
   currently using.

   @retval  EFI_SUCCESS           If Supports is not NULL, then the attributes that the PCI root
   bridge supports is returned in Supports. If Attributes is
   not NULL, then the attributes that the PCI root bridge is currently
   using is returned in Attributes.
   @retval  EFI_INVALID_PARAMETER Both Supports and Attributes are NULL.

**/
EFI_STATUS
EFIAPI
RootBridgeIoGetAttributes (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *This,
  OUT UINT64                          *Supported,
  OUT UINT64                          *Attributes
  );

/**
   Sets attributes for a resource range on a PCI root bridge.

   The SetAttributes() function sets the attributes specified in Attributes for the PCI root
   bridge on the resource range specified by ResourceBase and ResourceLength. Since the
   granularity of setting these attributes may vary from resource type to resource type, and from
   platform to platform, the actual resource range and the one passed in by the caller may differ. As a
   result, this function may set the attributes specified by Attributes on a larger resource range
   than the caller requested. The actual range is returned in ResourceBase and
   ResourceLength. The caller is responsible for verifying that the actual range for which the
   attributes were set is acceptable.

   @param[in]       This            A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[in]       Attributes      The mask of attributes to set. If the attribute bit
   MEMORY_WRITE_COMBINE, MEMORY_CACHED, or
   MEMORY_DISABLE is set, then the resource range is specified by
   ResourceBase and ResourceLength. If
   MEMORY_WRITE_COMBINE, MEMORY_CACHED, and
   MEMORY_DISABLE are not set, then ResourceBase and
   ResourceLength are ignored, and may be NULL.
   @param[in, out]  ResourceBase    A pointer to the base address of the resource range to be modified
   by the attributes specified by Attributes.
   @param[in, out]  ResourceLength  A pointer to the length of the resource range to be modified by the
   attributes specified by Attributes.

   @retval  EFI_SUCCESS     The current configuration of this PCI root bridge was returned in Resources.
   @retval  EFI_UNSUPPORTED The current configuration of this PCI root bridge could not be retrieved.
   @retval  EFI_INVALID_PARAMETER Invalid pointer of EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL

**/
EFI_STATUS
EFIAPI
RootBridgeIoSetAttributes (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *This,
  IN     UINT64                          Attributes,
  IN OUT UINT64                          *ResourceBase,
  IN OUT UINT64                          *ResourceLength
  );

/**
   Retrieves the current resource settings of this PCI root bridge in the form of a set of ACPI 2.0
   resource descriptors.

   There are only two resource descriptor types from the ACPI Specification that may be used to
   describe the current resources allocated to a PCI root bridge. These are the QWORD Address
   Space Descriptor (ACPI 2.0 Section 6.4.3.5.1), and the End Tag (ACPI 2.0 Section 6.4.2.8). The
   QWORD Address Space Descriptor can describe memory, I/O, and bus number ranges for dynamic
   or fixed resources. The configuration of a PCI root bridge is described with one or more QWORD
   Address Space Descriptors followed by an End Tag.

   @param[in]   This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
   @param[out]  Resources   A pointer to the ACPI 2.0 resource descriptors that describe the
   current configuration of this PCI root bridge. The storage for the
   ACPI 2.0 resource descriptors is allocated by this function. The
   caller must treat the return buffer as read-only data, and the buffer
   must not be freed by the caller.

   @retval  EFI_SUCCESS     The current configuration of this PCI root bridge was returned in Resources.
   @retval  EFI_UNSUPPORTED The current configuration of this PCI root bridge could not be retrieved.
   @retval  EFI_INVALID_PARAMETER Invalid pointer of EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL

**/
EFI_STATUS
EFIAPI
RootBridgeIoConfiguration (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *This,
  OUT VOID                            **Resources
  );

#endif /* PCI_ROOT_BRIDGE_IO_H_ */
