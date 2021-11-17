/** @file

  Copyright (c) 2020 - 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/ArmCache.h>
#include <Library/ArmLib.h>
#include "AcpiPlatform.h"

EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR PpttProcessorTemplate = {
  EFI_ACPI_6_3_PPTT_TYPE_PROCESSOR,
  sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR),
  { EFI_ACPI_RESERVED_BYTE, EFI_ACPI_RESERVED_BYTE },
  { 0 },      // Flags
  0,          // Parent
  0,          // AcpiProcessorId
  0           // NumberOfPrivateResources
};

EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE PpttCacheTemplate = {
  EFI_ACPI_6_3_PPTT_TYPE_CACHE,
  sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE),
  { EFI_ACPI_RESERVED_BYTE, EFI_ACPI_RESERVED_BYTE },
  { 0 },      // Flags
  0,          // NextLevelOfCache
  0,          // Size
  0,          // NumberOfSets
  0,          // Associativity
  { 0 },      // Attributes
  0
};

EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_HEADER PpttTableHeaderTemplate = {
  __ACPI_HEADER (
    EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_STRUCTURE_SIGNATURE,
    0,
    EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_REVISION
    )
};

STATIC EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_HEADER *mPpttTablePointer;

UINT32
AddProcessorCoreNode (
  VOID   *EntryPointer,
  UINT32 CpuId,
  UINT32 ClusterNodeOffset,
  UINT32 L1ICacheNodeOffset,
  UINT32 L1DCacheNodeOffset
  )
{
  EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR *PpttProcessorEntryPointer = EntryPointer;
  UINT32                                *ResPointer;
  UINTN                                 SocketId;
  UINTN                                 ClusterIdPerSocket;
  UINTN                                 CoreIdPerCpm;

  CopyMem (
    PpttProcessorEntryPointer,
    &PpttProcessorTemplate,
    sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR)
    );

  ClusterIdPerSocket = CLUSTER_ID (CpuId);
  SocketId = SOCKET_ID (CpuId);
  CoreIdPerCpm = CpuId % PLATFORM_CPU_NUM_CORES_PER_CPM;
  PpttProcessorEntryPointer->Flags.AcpiProcessorIdValid = EFI_ACPI_6_3_PPTT_PROCESSOR_ID_VALID;
  PpttProcessorEntryPointer->Flags.NodeIsALeaf = EFI_ACPI_6_3_PPTT_NODE_IS_LEAF;
  PpttProcessorEntryPointer->Flags.IdenticalImplementation = EFI_ACPI_6_3_PPTT_IMPLEMENTATION_IDENTICAL;
  PpttProcessorEntryPointer->AcpiProcessorId = (SocketId << PLATFORM_SOCKET_UID_BIT_OFFSET) | (ClusterIdPerSocket << 8) | CoreIdPerCpm;
  PpttProcessorEntryPointer->Parent = ClusterNodeOffset;
  PpttProcessorEntryPointer->NumberOfPrivateResources = 2; /* L1I + L1D */

  ResPointer = (UINT32 *)((UINT64)EntryPointer +
                          sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR));
  ResPointer[0] = L1ICacheNodeOffset;
  ResPointer[1] = L1DCacheNodeOffset;

  PpttProcessorEntryPointer->Length = sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR) + 2 * sizeof (UINT32);

  return PpttProcessorEntryPointer->Length;
}

UINT32
AddClusterNode (
  VOID   *EntryPointer,
  UINT32 SocketNodeOffset,
  UINT32 *ClusterNodeOffset
  )
{
  EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR *PpttProcessorEntryPointer = EntryPointer;

  *ClusterNodeOffset = (UINT64)EntryPointer - (UINT64)mPpttTablePointer;

  CopyMem (
    PpttProcessorEntryPointer,
    &PpttProcessorTemplate,
    sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR)
    );

  PpttProcessorEntryPointer->Parent = SocketNodeOffset;
  PpttProcessorEntryPointer->Flags.IdenticalImplementation = EFI_ACPI_6_3_PPTT_IMPLEMENTATION_IDENTICAL;

  return PpttProcessorEntryPointer->Length;
}

UINT32
AddSocketNode (
  VOID   *EntryPointer,
  UINT32 RootNodeOffset,
  UINT32 *SocketNodeOffset
  )
{
  EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR *PpttProcessorEntryPointer = EntryPointer;

  *SocketNodeOffset = (UINT64)EntryPointer - (UINT64)mPpttTablePointer;

  CopyMem (
    PpttProcessorEntryPointer,
    &PpttProcessorTemplate,
    sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR)
    );

  PpttProcessorEntryPointer->Flags.PhysicalPackage = EFI_ACPI_6_3_PPTT_PACKAGE_PHYSICAL;
  PpttProcessorEntryPointer->Flags.IdenticalImplementation = EFI_ACPI_6_3_PPTT_IMPLEMENTATION_IDENTICAL;
  PpttProcessorEntryPointer->Parent = RootNodeOffset;

  return PpttProcessorEntryPointer->Length;
}

UINT32
AddRootNode (
  VOID *EntryPointer,
  UINT32 *RootNodeOffset
  )
{
  EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR *PpttProcessorEntryPointer = EntryPointer;

  *RootNodeOffset = (UINT64)EntryPointer - (UINT64)mPpttTablePointer;

  CopyMem (
    PpttProcessorEntryPointer,
    &PpttProcessorTemplate,
    sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR)
    );

  PpttProcessorEntryPointer->Flags.IdenticalImplementation = EFI_ACPI_6_3_PPTT_IMPLEMENTATION_IDENTICAL;

  return PpttProcessorEntryPointer->Length;
}

VOID
AddFillCacheSizeInfo (
  EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE *Node,
  UINT32                            Level,
  BOOLEAN                           DataCache,
  BOOLEAN                           UnifiedCache
  )
{
  CSSELR_DATA CsselrData;
  CCSIDR_DATA CcsidrData;

  CsselrData.Data = 0;
  CsselrData.Bits.Level = Level - 1;
  CsselrData.Bits.InD = (!DataCache && !UnifiedCache);

  CcsidrData.Data = ReadCCSIDR (CsselrData.Data);

  Node->Flags.SizePropertyValid = 1;
  Node->Flags.NumberOfSetsValid = EFI_ACPI_6_3_PPTT_NUMBER_OF_SETS_VALID;
  Node->Flags.AssociativityValid = EFI_ACPI_6_3_PPTT_ASSOCIATIVITY_VALID;
  Node->Flags.CacheTypeValid = EFI_ACPI_6_3_PPTT_CACHE_TYPE_VALID;
  Node->Flags.LineSizeValid = EFI_ACPI_6_3_PPTT_LINE_SIZE_VALID;
  Node->NumberOfSets = (UINT16)CcsidrData.BitsNonCcidx.NumSets + 1;
  Node->Associativity = (UINT16)CcsidrData.BitsNonCcidx.Associativity + 1;
  Node->LineSize = (UINT16)(1 << (CcsidrData.BitsNonCcidx.LineSize + 4));;
  Node->Size = Node->NumberOfSets *
               Node->Associativity *
               Node->LineSize;
}

UINT32
AddL1DataCacheNode (
  VOID   *EntryPointer,
  UINT32 L2CacheNodeOffset,
  UINT32 *L1DCacheNodeOffset
  )
{
  EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE *PpttCacheEntryPointer = EntryPointer;

  *L1DCacheNodeOffset = (UINT64)EntryPointer - (UINT64)mPpttTablePointer;

  CopyMem (
    PpttCacheEntryPointer,
    &PpttCacheTemplate,
    sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE)
    );

  AddFillCacheSizeInfo (PpttCacheEntryPointer, 1, TRUE, FALSE);
  PpttCacheEntryPointer->Attributes.CacheType = EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_DATA;
  PpttCacheEntryPointer->NextLevelOfCache = L2CacheNodeOffset;

  return PpttCacheEntryPointer->Length;
}

UINT32
AddL1InstructionCacheNode (
  VOID   *EntryPointer,
  UINT32 L2CacheNodeOffset,
  UINT32 *L1ICacheNodeOffset
  )
{
  EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE *PpttCacheEntryPointer = EntryPointer;

  *L1ICacheNodeOffset = (UINT64)EntryPointer - (UINT64)mPpttTablePointer;

  CopyMem (
    PpttCacheEntryPointer,
    &PpttCacheTemplate,
    sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE)
    );

  AddFillCacheSizeInfo (PpttCacheEntryPointer, 1, FALSE, FALSE);
  PpttCacheEntryPointer->Attributes.CacheType = EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_INSTRUCTION;
  PpttCacheEntryPointer->NextLevelOfCache = L2CacheNodeOffset;

  return PpttCacheEntryPointer->Length;
}

UINT32
AddL2CacheNode (
  VOID   *EntryPointer,
  UINT32 *L2CacheNodeOffset
  )
{
  EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE *PpttCacheEntryPointer = EntryPointer;

  *L2CacheNodeOffset = (UINT64)EntryPointer - (UINT64)mPpttTablePointer;

  CopyMem (
    PpttCacheEntryPointer,
    &PpttCacheTemplate,
    sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE)
    );

  AddFillCacheSizeInfo (PpttCacheEntryPointer, 2, FALSE, TRUE);
  PpttCacheEntryPointer->Attributes.CacheType = EFI_ACPI_6_3_CACHE_ATTRIBUTES_CACHE_TYPE_UNIFIED;
  PpttCacheEntryPointer->NextLevelOfCache = 0;

  return PpttCacheEntryPointer->Length;
}

/*
 *  Install PPTT table.
 */
EFI_STATUS
AcpiInstallPpttTable (
  VOID
  )
{
  EFI_ACPI_TABLE_PROTOCOL               *AcpiTableProtocol;
  UINTN                                 PpttTableKey;
  EFI_STATUS                            Status;
  UINTN                                 Size;
  UINTN                                 NumSockets;
  UINTN                                 MaxNumCores;
  UINTN                                 SocketId;
  UINTN                                 CpuId;
  UINTN                                 Length;
  VOID                                  *EntryPointer;
  UINT32                                RootOffset;
  UINT32                                SocketOffset;
  UINT32                                ClusterOffset;
  UINT32                                L2CacheOffset;
  UINT32                                L1ICacheOffset;
  UINT32                                L1DCacheOffset;

  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&AcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NumSockets = GetNumberOfActiveSockets ();
  MaxNumCores = PLATFORM_CPU_MAX_CPM * PLATFORM_CPU_NUM_CORES_PER_CPM;

  Size = sizeof (EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_HEADER) +
         sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR) +                                      /* Root node    */
         sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR) * NumSockets +                         /* Socket node  */
         sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR) * (GetNumberOfActiveCores () / PLATFORM_CPU_NUM_CORES_PER_CPM) +  /* Cluster node */
         (sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_PROCESSOR) + 2 * sizeof (UINT32)) * GetNumberOfActiveCores () +             /* Core node    */
         sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE) +                                          /* L1I node     */
         sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE) +                                          /* L1D node     */
         sizeof (EFI_ACPI_6_3_PPTT_STRUCTURE_CACHE);                                           /* L2 node      */

  mPpttTablePointer =
    (EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_HEADER *)AllocateZeroPool (Size);
  if (mPpttTablePointer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  EntryPointer = (VOID *)((UINT64)mPpttTablePointer + sizeof (EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_HEADER));
  Length = 0;

  Length += AddRootNode (EntryPointer + Length, &RootOffset);
  Length += AddL2CacheNode (EntryPointer + Length, &L2CacheOffset);
  Length += AddL1InstructionCacheNode (EntryPointer + Length, L2CacheOffset, &L1ICacheOffset);
  Length += AddL1DataCacheNode (EntryPointer + Length, L2CacheOffset, &L1DCacheOffset);

  for (SocketId = 0; SocketId < NumSockets; SocketId++) {
    Length += AddSocketNode (EntryPointer + Length, RootOffset, &SocketOffset);
    CpuId = SocketId * MaxNumCores;
    while (CpuId < (SocketId + 1) * MaxNumCores) {
      if (IsCpuEnabled (CpuId)) {
        // Assume that the cores in a cluster are enabled in pairs
        if (CpuId % PLATFORM_CPU_NUM_CORES_PER_CPM == 0) {
          // A cluster contains two processors
          Length += AddClusterNode (EntryPointer + Length, SocketOffset, &ClusterOffset);
        }
        ASSERT (ClusterOffset != 0);

        Length += AddProcessorCoreNode (
                    EntryPointer + Length,
                    CpuId,
                    ClusterOffset,
                    L1ICacheOffset,
                    L1DCacheOffset
                    );
      }
      CpuId++;
    }
  }
  ASSERT ((Length + sizeof (EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_HEADER)) == Size);

  CopyMem (
    mPpttTablePointer,
    &PpttTableHeaderTemplate,
    sizeof (EFI_ACPI_6_3_PROCESSOR_PROPERTIES_TOPOLOGY_TABLE_HEADER)
    );

  mPpttTablePointer->Header.Length = Size;

  AcpiTableChecksum (
    (UINT8 *)mPpttTablePointer,
    mPpttTablePointer->Header.Length
    );

  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                (VOID *)mPpttTablePointer,
                                mPpttTablePointer->Header.Length,
                                &PpttTableKey
                                );
  ASSERT_EFI_ERROR (Status);

  FreePool ((VOID *)mPpttTablePointer);
  return Status;
}
