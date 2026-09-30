/** @file

  Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "HelloWorld.h"
#include <Library/HobLib.h>
#include <Guid/LoaderPlatformInfoGuid.h>
#include <Guid/MemoryMapInfoGuid.h>
#include <Guid/PerformanceInfoGuid.h>

//
// Upper bound on the number of HOBs the explorer is willing to walk.
// It keeps a corrupted list from turning into an endless traversal.
//
#define HOB_EXPLORER_MAX_HOBS  512

//
// Highest address representable by a native pointer on this build.
//
#define HOB_EXPLORER_MAX_ADDRESS  (~(UINTN)0)

//
// The only LOADER_PLATFORM_INFO revision any producer in this tree emits.
// BootloaderCorePkg/Stage2/Stage2Hob.c:847 sets Revision = 2 and the header
// BootloaderCommonPkg/Include/Guid/LoaderPlatformInfoGuid.h defines exactly
// one layout for it. A different revision is reported but does not stop the
// decode, because the decode is gated on the validated data size instead.
//
#define LOADER_PLATFORM_INFO_KNOWN_REVISION  2

/**
  Get the symbolic name of a HOB type.

  @param[in]  HobType       Numeric HOB type from the HOB generic header.

  @return  ASCII name of the HOB type, "UNKNOWN" if not a PI defined type.

**/
STATIC
CONST CHAR8 *
HobTypeToStr (
  IN  UINT16    HobType
  )
{
  switch (HobType) {
  case EFI_HOB_TYPE_HANDOFF:
    return "HANDOFF";
  case EFI_HOB_TYPE_MEMORY_ALLOCATION:
    return "MEMORY_ALLOCATION";
  case EFI_HOB_TYPE_RESOURCE_DESCRIPTOR:
    return "RESOURCE_DESCRIPTOR";
  case EFI_HOB_TYPE_GUID_EXTENSION:
    return "GUID_EXTENSION";
  case EFI_HOB_TYPE_FV:
    return "FV";
  case EFI_HOB_TYPE_CPU:
    return "CPU";
  case EFI_HOB_TYPE_MEMORY_POOL:
    return "MEMORY_POOL";
  case EFI_HOB_TYPE_FV2:
    return "FV2";
  case EFI_HOB_TYPE_LOAD_PEIM_UNUSED:
    return "LOAD_PEIM_UNUSED";
  case EFI_HOB_TYPE_UEFI_CAPSULE:
    return "UEFI_CAPSULE";
  case EFI_HOB_TYPE_FV3:
    return "FV3";
  case EFI_HOB_TYPE_UNUSED:
    return "UNUSED";
  case EFI_HOB_TYPE_END_OF_HOB_LIST:
    return "END_OF_HOB_LIST";
  default:
    return "UNKNOWN";
  }
}

/**
  Check the common sanity rules of a HOB generic header.

  The header itself must already be known to be fully readable.

  @param[in]  Header        Pointer to a readable HOB generic header.

  @retval  TRUE             Reserved is zero and the length is sane.
  @retval  FALSE            The header is malformed.

**/
STATIC
BOOLEAN
IsHobHeaderSane (
  IN  CONST EFI_HOB_GENERIC_HEADER  *Header
  )
{
  if (Header->Reserved != 0) {
    return FALSE;
  }

  if (Header->HobLength < sizeof (EFI_HOB_GENERIC_HEADER)) {
    return FALSE;
  }

  if ((Header->HobLength & 0x7) != 0) {
    return FALSE;
  }

  return TRUE;
}

/**
  Get the symbolic name of a LOADER_PLATFORM_INFO.BootPartition value.

  Values come from the BOOT_PARTITION enum in
  BootloaderCommonPkg/Include/Library/BootloaderCommonLib.h.

  @param[in]  BootPartition Raw BootPartition field.

  @return  ASCII name, "UNKNOWN" if not an enumerated value.

**/
STATIC
CONST CHAR8 *
BootPartitionToStr (
  IN  UINT8     BootPartition
  )
{
  switch (BootPartition) {
  case PrimaryPartition:
    return "PrimaryPartition";
  case BackupPartition:
    return "BackupPartition";
  default:
    return "UNKNOWN";
  }
}

/**
  Get the symbolic name of a LOADER_PLATFORM_INFO.BootMode value.

  The producer stores GetBootMode(), an EFI_BOOT_MODE, into this field, so the
  names are the PI ones from MdePkg/Include/Pi/PiBootMode.h.

  @param[in]  BootMode      Raw BootMode field.

  @return  ASCII name, "UNKNOWN" if not a PI defined boot mode.

**/
STATIC
CONST CHAR8 *
BootModeToStr (
  IN  UINT8     BootMode
  )
{
  switch (BootMode) {
  case BOOT_WITH_FULL_CONFIGURATION:
    return "BOOT_WITH_FULL_CONFIGURATION";
  case BOOT_WITH_MINIMAL_CONFIGURATION:
    return "BOOT_WITH_MINIMAL_CONFIGURATION";
  case BOOT_ASSUMING_NO_CONFIGURATION_CHANGES:
    return "BOOT_ASSUMING_NO_CONFIGURATION_CHANGES";
  case BOOT_WITH_FULL_CONFIGURATION_PLUS_DIAGNOSTICS:
    return "BOOT_WITH_FULL_CONFIGURATION_PLUS_DIAGNOSTICS";
  case BOOT_WITH_DEFAULT_SETTINGS:
    return "BOOT_WITH_DEFAULT_SETTINGS";
  case BOOT_ON_S4_RESUME:
    return "BOOT_ON_S4_RESUME";
  case BOOT_ON_S5_RESUME:
    return "BOOT_ON_S5_RESUME";
  case BOOT_WITH_MFG_MODE_SETTINGS:
    return "BOOT_WITH_MFG_MODE_SETTINGS";
  case BOOT_ON_S2_RESUME:
    return "BOOT_ON_S2_RESUME";
  case BOOT_ON_S3_RESUME:
    return "BOOT_ON_S3_RESUME";
  case BOOT_ON_FLASH_UPDATE:
    return "BOOT_ON_FLASH_UPDATE";
  case BOOT_IN_RECOVERY_MODE:
    return "BOOT_IN_RECOVERY_MODE";
  default:
    return "UNKNOWN";
  }
}

/**
  Get the symbolic name of a LOADER_PLATFORM_INFO.TpmType value.

  Values come from the TPM_TYPE_* defines in
  BootloaderCommonPkg/Include/Guid/LoaderPlatformInfoGuid.h.

  @param[in]  TpmType       Raw TpmType field.

  @return  ASCII name, "UNKNOWN" if not a defined TPM type.

**/
STATIC
CONST CHAR8 *
TpmTypeToStr (
  IN  UINT8     TpmType
  )
{
  switch (TpmType) {
  case TPM_TYPE_NOT_KNOWN:
    return "TPM_TYPE_NOT_KNOWN";
  case TPM_TYPE_NONE:
    return "TPM_TYPE_NONE";
  case TPM_TYPE_DTPM20:
    return "TPM_TYPE_DTPM20";
  case TPM_TYPE_PTT:
    return "TPM_TYPE_PTT";
  default:
    return "UNKNOWN";
  }
}

/**
  Decode and print the payload of a gLoaderPlatformInfoGuid HOB.

  The caller must already have proven that the HOB is a GUID extension HOB
  whose generic header and whole body are readable, which makes the value of
  GET_GUID_HOB_DATA_SIZE() trustworthy. This function refuses to cast the data
  pointer until that size covers the whole LOADER_PLATFORM_INFO structure, so
  no field is ever read past the validated data.

  SerialNumber is deliberately not printed.

  @param[in]  GuidHob       Pointer to a validated gLoaderPlatformInfoGuid HOB.

  @retval  TRUE             The payload was decoded and printed.
  @retval  FALSE            The payload is too short, error has been printed.

**/
STATIC
BOOLEAN
DumpLoaderPlatformInfo (
  IN  EFI_HOB_GUID_TYPE  *GuidHob
  )
{
  UINT16                      DataSize;
  CONST LOADER_PLATFORM_INFO  *Info;

  DataSize = GET_GUID_HOB_DATA_SIZE (GuidHob);
  if (DataSize < sizeof (LOADER_PLATFORM_INFO)) {
    ConsolePrint ("     ERROR: LOADER_PLATFORM_INFO data is 0x%04x bytes, need 0x%04x\n",
                  DataSize, (UINT16)sizeof (LOADER_PLATFORM_INFO));
    return FALSE;
  }

  Info = (CONST LOADER_PLATFORM_INFO *)GET_GUID_HOB_DATA (GuidHob);

  if (Info->Revision != LOADER_PLATFORM_INFO_KNOWN_REVISION) {
    ConsolePrint ("     WARNING: LOADER_PLATFORM_INFO revision %d, known revision is %d\n",
                  Info->Revision, LOADER_PLATFORM_INFO_KNOWN_REVISION);
  }

  ConsolePrint ("     Revision %d  BootPartition %d (%a)  BootMode 0x%02x (%a)\n",
                Info->Revision,
                Info->BootPartition, BootPartitionToStr (Info->BootPartition),
                Info->BootMode, BootModeToStr (Info->BootMode));
  ConsolePrint ("     PlatformId 0x%04x  CpuCount %d  HwState 0x%04x  Flags 0x%04x\n",
                Info->PlatformId, Info->CpuCount, Info->HwState, Info->Flags);
  ConsolePrint ("     LdrFeatures 0x%08x  TpmType 0x%02x (%a)\n",
                Info->LdrFeatures, Info->TpmType, TpmTypeToStr (Info->TpmType));

  return TRUE;
}

/**
  Get the symbolic name of a MEMORY_MAP_ENTRY.Type value.

  Values come from the MEM_MAP_TYPE_* defines in
  BootloaderCommonPkg/Include/Guid/MemoryMapInfoGuid.h:14-17. That header
  defines no other type, so anything else is reported as UNKNOWN and the raw
  value stays visible in the printout.

  @param[in]  Type          Raw Type field of a memory map entry.

  @return  ASCII name, "UNKNOWN" if not a defined memory map type.

**/
STATIC
CONST CHAR8 *
MemMapTypeToStr (
  IN  UINT8     Type
  )
{
  switch (Type) {
  case MEM_MAP_TYPE_RAM:
    return "RAM";
  case MEM_MAP_TYPE_RESERVED:
    return "RESERVED";
  case MEM_MAP_TYPE_ACPI_RECLAIM:
    return "ACPI_RECLAIM";
  case MEM_MAP_TYPE_ACPI_NVS:
    return "ACPI_NVS";
  default:
    return "UNKNOWN";
  }
}

/**
  Decode and print the payload of a gLoaderMemoryMapInfoGuid HOB.

  The caller must already have proven that the HOB is a GUID extension HOB
  whose generic header and whole body are readable, which makes the value of
  GET_GUID_HOB_DATA_SIZE() trustworthy.

  MEMORY_MAP_INFO ends with a flexible array (Entry[0]), so its sizeof() only
  covers the fixed prefix and cannot be used to bound the entries. The fixed
  prefix is measured with OFFSET_OF (MdePkg/Include/Base.h:754/758), and the
  number of entries the validated data can actually hold is

    Capacity = (DataSize - OFFSET_OF (MEMORY_MAP_INFO, Entry)) / sizeof (MEMORY_MAP_ENTRY)

  The subtraction is only done once DataSize is known to cover the prefix, so
  it cannot wrap, and the division cannot overflow. Comparing Count against
  Capacity with this division avoids computing Count * sizeof (MEMORY_MAP_ENTRY),
  which could overflow for a corrupted Count. A Count of zero and trailing
  padding after the last entry both fall out of this naturally.

  @param[in]  GuidHob       Pointer to a validated gLoaderMemoryMapInfoGuid HOB.

  @retval  TRUE             The payload was decoded and printed.
  @retval  FALSE            The payload is inconsistent, error has been printed.

**/
STATIC
BOOLEAN
DumpMemoryMapInfo (
  IN  EFI_HOB_GUID_TYPE  *GuidHob
  )
{
  UINT16                    DataSize;
  UINTN                     FixedPrefixSize;
  UINT32                    Capacity;
  UINT32                    Index;
  CONST MEMORY_MAP_INFO     *MemMap;
  CONST MEMORY_MAP_ENTRY    *Entry;

  DataSize        = GET_GUID_HOB_DATA_SIZE (GuidHob);
  FixedPrefixSize = OFFSET_OF (MEMORY_MAP_INFO, Entry);

  if (DataSize < FixedPrefixSize) {
    ConsolePrint ("     ERROR: MEMORY_MAP_INFO data is 0x%04x bytes, need 0x%04x\n",
                  DataSize, (UINT16)FixedPrefixSize);
    return FALSE;
  }

  //
  // Only now, with the fixed prefix proven readable, may Revision and Count
  // be read.
  //
  MemMap   = (CONST MEMORY_MAP_INFO *)GET_GUID_HOB_DATA (GuidHob);
  Capacity = (UINT32)((DataSize - FixedPrefixSize) / sizeof (MEMORY_MAP_ENTRY));

  if (MemMap->Count > Capacity) {
    ConsolePrint ("     ERROR: MEMORY_MAP_INFO Count %u exceeds capacity %u\n",
                  MemMap->Count, Capacity);
    return FALSE;
  }

  ConsolePrint ("     Revision %d  Count %u (capacity %u)\n",
                MemMap->Revision, MemMap->Count, Capacity);

  for (Index = 0; Index < MemMap->Count; Index++) {
    Entry = &MemMap->Entry[Index];
    ConsolePrint ("     [%2d] Base 0x%016lx  Size 0x%016lx  Type 0x%02x (%a)  Flag 0x%02x%a\n",
                  Index, Entry->Base, Entry->Size,
                  Entry->Type, MemMapTypeToStr (Entry->Type), Entry->Flag,
                  ((Entry->Flag & MEM_MAP_FLAG_PAYLOAD) != 0) ? " PAYLOAD" : "");
  }

  return TRUE;
}

/**
  Decode and print the payload of a gLoaderPerformanceInfoGuid HOB.

  The caller must already have proven that the HOB is a GUID extension HOB
  whose generic header and whole body are readable, which makes the value of
  GET_GUID_HOB_DATA_SIZE() trustworthy.

  PERFORMANCE_INFO (BootloaderCommonPkg/Include/Guid/PerformanceInfoGuid.h:16-23)
  ends with a flexible array TimeStamp[0], so the same bounds rule as the memory
  map decoder applies:

    Capacity = (DataSize - OFFSET_OF (PERFORMANCE_INFO, TimeStamp)) / sizeof (UINT64)

  The subtraction only happens after DataSize is known to cover the fixed
  prefix, so it cannot wrap, and comparing Count against Capacity avoids ever
  computing Count * sizeof (UINT64).

  Field semantics, all taken from the in-tree producer and consumers:
    - Producer BootloaderCorePkg/Stage2/Stage2Hob.c:819-826 copies
      LdrGlobal->PerfData (Revision 1, Flags 0).
    - Frequency is LdrGlobal->PerfData.FreqKhz (Stage2Hob.c:824), set from
      GetTimeStampFrequency() in BootloaderCorePkg/Stage1A/Stage1A.c:485, which
      returns the TSC frequency in KHz
      (BootloaderCommonPkg/Library/TimeStampLib/TimeStampLib.c:34-45).
      Ticks / FreqKhz therefore yields milliseconds, which is exactly how
      BootloaderCommonPkg/Library/ShellLib/CmdPerf.c:74 and
      BootloaderCorePkg/Library/AcpiInitLib/AcpiFpdt.c:277 convert.
    - Each TimeStamp entry packs a 16-bit measurement Id in bits 63:48 and the
      raw TSC value in bits 47:0 (CmdPerf.c:69-70).
    - The tick value is an absolute AsmReadTsc() reading
      (TimeStampLib.c:19-23, first entries stored in Stage1A.c:486-487), so the
      time column is time since the TSC origin, not a per-entry delta. The
      delta column is computed here, like CmdPerf.c:71-77 does.

  @param[in]  GuidHob       Pointer to a validated gLoaderPerformanceInfoGuid HOB.

  @retval  TRUE             The payload was decoded and printed.
  @retval  FALSE            The payload is inconsistent, error has been printed.

**/
STATIC
BOOLEAN
DumpPerformanceInfo (
  IN  EFI_HOB_GUID_TYPE  *GuidHob
  )
{
  UINT16                    DataSize;
  UINTN                     FixedPrefixSize;
  UINT32                    Capacity;
  UINT32                    Index;
  CONST PERFORMANCE_INFO    *PerfInfo;
  UINT64                    Stamp;
  UINT64                    Ticks;
  UINT64                    Time;
  UINT64                    PrevTime;
  UINT16                    Id;

  DataSize        = GET_GUID_HOB_DATA_SIZE (GuidHob);
  FixedPrefixSize = OFFSET_OF (PERFORMANCE_INFO, TimeStamp);

  if (DataSize < FixedPrefixSize) {
    ConsolePrint ("     ERROR: PERFORMANCE_INFO data is 0x%04x bytes, need 0x%04x\n",
                  DataSize, (UINT16)FixedPrefixSize);
    return FALSE;
  }

  //
  // Only now, with the fixed prefix proven readable, may Revision, Count,
  // Flags and Frequency be read.
  //
  PerfInfo = (CONST PERFORMANCE_INFO *)GET_GUID_HOB_DATA (GuidHob);
  Capacity = (UINT32)((DataSize - FixedPrefixSize) / sizeof (UINT64));

  if (PerfInfo->Count > Capacity) {
    ConsolePrint ("     ERROR: PERFORMANCE_INFO Count %u exceeds capacity %u\n",
                  PerfInfo->Count, Capacity);
    return FALSE;
  }

  ConsolePrint ("     Revision %d  Count %u (capacity %u)  Flags 0x%04x\n",
                PerfInfo->Revision, PerfInfo->Count, Capacity, PerfInfo->Flags);
  ConsolePrint ("     Frequency %u KHz\n", PerfInfo->Frequency);
  if (PerfInfo->Frequency == 0) {
    ConsolePrint ("     WARNING: Frequency is 0, tick to time conversion unavailable\n");
  }

  PrevTime = 0;
  for (Index = 0; Index < PerfInfo->Count; Index++) {
    Stamp = PerfInfo->TimeStamp[Index];
    Id    = (UINT16)RShiftU64 (Stamp, 48);
    Ticks = Stamp & 0x0000FFFFFFFFFFFFULL;

    if (PerfInfo->Frequency == 0) {
      ConsolePrint ("     [%2d] Id 0x%04x  Ticks 0x%012lx  Time unavailable\n",
                    Index, Id, Ticks);
      continue;
    }

    //
    // Ticks is at most 2^48-1 and Frequency is a non zero UINT32, so the
    // 64-bit by 32-bit division needs no scaling and loses no range.
    //
    Time = DivU64x32 (Ticks, PerfInfo->Frequency);
    if (Time >= PrevTime) {
      ConsolePrint ("     [%2d] Id 0x%04x  Ticks 0x%012lx  Time %lu ms  Delta %lu ms\n",
                    Index, Id, Ticks, Time, Time - PrevTime);
    } else {
      ConsolePrint ("     [%2d] Id 0x%04x  Ticks 0x%012lx  Time %lu ms  Delta n/a (went backwards)\n",
                    Index, Id, Ticks, Time);
    }
    PrevTime = Time;
  }

  return TRUE;
}

/**
  Validate and print the PHIT (handoff) HOB.

  The caller must already have validated that the whole PHIT HOB body, as
  described by its generic header, is readable.

  @param[in]   Phit         Pointer to the PHIT HOB.
  @param[out]  ListEnd      On success, the address of the end-of-list header
                            as reported by the PHIT.

  @retval  TRUE             The PHIT is consistent, summary has been printed.
  @retval  FALSE            The PHIT is inconsistent, error has been printed.

**/
STATIC
BOOLEAN
DumpPhitHob (
  IN  CONST EFI_HOB_HANDOFF_INFO_TABLE  *Phit,
  OUT UINTN                             *ListEnd
  )
{
  UINT64    MaxAddress;
  UINTN     PhitAddress;
  UINTN     MemoryBottom;
  UINTN     MemoryTop;
  UINTN     FreeBottom;
  UINTN     FreeTop;
  UINTN     EndAddress;

  if (Phit->Version != EFI_HOB_HANDOFF_TABLE_VERSION) {
    ConsolePrint ("ERROR: PHIT version 0x%08x, expected 0x%08x\n",
                  Phit->Version, EFI_HOB_HANDOFF_TABLE_VERSION);
    return FALSE;
  }

  //
  // Every physical address the PHIT reports has to be representable as a
  // native pointer before it is cast down and used.
  //
  MaxAddress = (UINT64)HOB_EXPLORER_MAX_ADDRESS;
  if ((Phit->EfiMemoryBottom > MaxAddress) || (Phit->EfiMemoryTop > MaxAddress) ||
      (Phit->EfiFreeMemoryBottom > MaxAddress) || (Phit->EfiFreeMemoryTop > MaxAddress) ||
      (Phit->EfiEndOfHobList > MaxAddress)) {
    ConsolePrint ("ERROR: PHIT reports an address outside the native address space\n");
    return FALSE;
  }

  PhitAddress  = (UINTN)Phit;
  MemoryBottom = (UINTN)Phit->EfiMemoryBottom;
  MemoryTop    = (UINTN)Phit->EfiMemoryTop;
  FreeBottom   = (UINTN)Phit->EfiFreeMemoryBottom;
  FreeTop      = (UINTN)Phit->EfiFreeMemoryTop;
  EndAddress   = (UINTN)Phit->EfiEndOfHobList;

  //
  // Bounds have to be ordered, and the PHIT has to live inside them.
  //
  if ((MemoryBottom >= MemoryTop) || (FreeBottom > FreeTop) ||
      (FreeBottom < MemoryBottom) || (FreeTop > MemoryTop) ||
      (PhitAddress < MemoryBottom)) {
    ConsolePrint ("ERROR: PHIT memory bounds are not coherent\n");
    return FALSE;
  }

  //
  // EfiEndOfHobList points at the end-of-list header itself, so that header
  // must be aligned, must sit past the whole PHIT, must not wrap, and must
  // stay inside the used part of the HOB region (the producer leaves
  // EfiFreeMemoryBottom right behind it).
  //
  if ((EndAddress & 0x7) != 0) {
    ConsolePrint ("ERROR: PHIT end-of-list pointer %p is not 8-byte aligned\n", (VOID *)EndAddress);
    return FALSE;
  }

  if (EndAddress < PhitAddress + Phit->Header.HobLength) {
    ConsolePrint ("ERROR: PHIT end-of-list pointer %p overlaps the PHIT HOB\n", (VOID *)EndAddress);
    return FALSE;
  }

  if (EndAddress > HOB_EXPLORER_MAX_ADDRESS - sizeof (EFI_HOB_GENERIC_HEADER)) {
    ConsolePrint ("ERROR: PHIT end-of-list header wraps the address space\n");
    return FALSE;
  }

  if ((EndAddress + sizeof (EFI_HOB_GENERIC_HEADER) > FreeBottom) ||
      (EndAddress + sizeof (EFI_HOB_GENERIC_HEADER) > MemoryTop) ||
      (EndAddress < MemoryBottom)) {
    ConsolePrint ("ERROR: PHIT end-of-list header %p is outside the HOB region\n", (VOID *)EndAddress);
    return FALSE;
  }

  ConsolePrint ("PHIT HOB    : %p  Length 0x%04x  Version 0x%08x\n",
                (VOID *)PhitAddress, Phit->Header.HobLength, Phit->Version);
  ConsolePrint ("Boot Mode   : 0x%08x\n", Phit->BootMode);
  ConsolePrint ("Memory      : Bottom %p  Top %p\n", (VOID *)MemoryBottom, (VOID *)MemoryTop);
  ConsolePrint ("Free Memory : Bottom %p  Top %p\n", (VOID *)FreeBottom, (VOID *)FreeTop);
  ConsolePrint ("End Of List : %p\n\n", (VOID *)EndAddress);

  *ListEnd = EndAddress;
  return TRUE;
}

/**
  Walk and print the bootloader HOB list.

  The HOB list is acquired through the normal payload helper, which returns
  the PcdPayloadHobList value that PayloadEntryLib has already set up.

**/
STATIC
VOID
ExploreHobList (
  VOID
  )
{
  VOID                        *HobList;
  EFI_HOB_HANDOFF_INFO_TABLE  *Phit;
  EFI_HOB_GENERIC_HEADER      *Header;
  EFI_HOB_GUID_TYPE           *GuidHob;
  VOID                        *Hob;
  UINTN                       Current;
  UINTN                       ListEnd;
  UINT32                      Count;
  UINT32                      DecodeErrors;
  UINT16                      HobType;
  UINT16                      HobLength;

  ConsolePrint ("SBL HOB Explorer\n");
  ConsolePrint ("================\n");

  Count        = 0;
  DecodeErrors = 0;
  HobList      = GetHobListPtr ();
  if (HobList == NULL) {
    ConsolePrint ("ERROR: HOB list pointer is NULL, valid HOBs: %d\n", Count);
    return;
  }

  Current = (UINTN)HobList;
  if ((Current & 0x7) != 0) {
    ConsolePrint ("ERROR: HOB list %p is not 8-byte aligned, valid HOBs: %d\n", HobList, Count);
    return;
  }

  //
  // Make sure the PHIT generic header can be read at all before touching it.
  //
  if (Current > HOB_EXPLORER_MAX_ADDRESS - sizeof (EFI_HOB_HANDOFF_INFO_TABLE)) {
    ConsolePrint ("ERROR: HOB list %p wraps the address space, valid HOBs: %d\n", HobList, Count);
    return;
  }

  Header = (EFI_HOB_GENERIC_HEADER *)HobList;
  if (!IsHobHeaderSane (Header) || (Header->HobType != EFI_HOB_TYPE_HANDOFF) ||
      (Header->HobLength != sizeof (EFI_HOB_HANDOFF_INFO_TABLE))) {
    ConsolePrint ("ERROR: first HOB is not a well formed PHIT, valid HOBs: %d\n", Count);
    return;
  }

  Phit = (EFI_HOB_HANDOFF_INFO_TABLE *)HobList;
  if (!DumpPhitHob (Phit, &ListEnd)) {
    ConsolePrint ("Valid HOBs: %d\n", Count);
    return;
  }
  Count   = 1;
  Current = (UINTN)HobList + Header->HobLength;

  while (Count < HOB_EXPLORER_MAX_HOBS) {
    if (Current > ListEnd) {
      ConsolePrint ("ERROR: traversal ran past the end of the HOB list, valid HOBs: %d\n", Count);
      return;
    }

    if (Current == ListEnd) {
      //
      // Terminal HOB. Readability of this header was proven while validating
      // the PHIT end pointer.
      //
      Header = (EFI_HOB_GENERIC_HEADER *)Current;
      if ((Header->HobType != EFI_HOB_TYPE_END_OF_HOB_LIST) ||
          (Header->HobLength != sizeof (EFI_HOB_GENERIC_HEADER)) ||
          (Header->Reserved != 0)) {
        ConsolePrint ("ERROR: malformed terminal HOB at %p, valid HOBs: %d\n", (VOID *)Current, Count);
        return;
      }

      ConsolePrint ("%3d: %p  Type 0x%04x %a  Length 0x%04x\n",
                    Count, (VOID *)Current, Header->HobType,
                    HobTypeToStr (Header->HobType), Header->HobLength);
      Count++;
      ConsolePrint ("\nTotal HOBs: %d (including PHIT and END)\n", Count);
      if (DecodeErrors != 0) {
        ConsolePrint ("ERROR: %d typed HOB decode error(s), the walk itself was valid\n", DecodeErrors);
      }
      return;
    }

    //
    // A non terminal HOB must have its whole header below the end header.
    // Current is strictly below ListEnd here, so the subtraction is safe.
    //
    if (ListEnd - Current < sizeof (EFI_HOB_GENERIC_HEADER)) {
      ConsolePrint ("ERROR: HOB header at %p overlaps the end HOB, valid HOBs: %d\n", (VOID *)Current, Count);
      return;
    }

    Hob       = (VOID *)Current;
    HobType   = GET_HOB_TYPE (Hob);
    HobLength = GET_HOB_LENGTH (Hob);

    if (!IsHobHeaderSane ((EFI_HOB_GENERIC_HEADER *)Hob) || (HobLength == 0)) {
      ConsolePrint ("ERROR: malformed HOB header at %p, valid HOBs: %d\n", (VOID *)Current, Count);
      return;
    }

    if (HobLength > ListEnd - Current) {
      ConsolePrint ("ERROR: HOB at %p extends past the end HOB, valid HOBs: %d\n", (VOID *)Current, Count);
      return;
    }

    if (HobType == EFI_HOB_TYPE_END_OF_HOB_LIST) {
      ConsolePrint ("ERROR: unexpected END HOB at %p, valid HOBs: %d\n", (VOID *)Current, Count);
      return;
    }

    if (HobType == EFI_HOB_TYPE_HANDOFF) {
      ConsolePrint ("ERROR: duplicate PHIT HOB at %p, valid HOBs: %d\n", (VOID *)Current, Count);
      return;
    }

    ConsolePrint ("%3d: %p  Type 0x%04x %a  Length 0x%04x\n",
                  Count, (VOID *)Current, HobType, HobTypeToStr (HobType), HobLength);

    if (HobType == EFI_HOB_TYPE_GUID_EXTENSION) {
      if (HobLength < sizeof (EFI_HOB_GUID_TYPE)) {
        ConsolePrint ("ERROR: GUID HOB at %p is too short, valid HOBs: %d\n", (VOID *)Current, Count);
        return;
      }
      GuidHob = (EFI_HOB_GUID_TYPE *)Hob;
      ConsolePrint ("     Guid %g  DataSize 0x%04x\n",
                    &GuidHob->Name, GET_GUID_HOB_DATA_SIZE (GuidHob));

      if (CompareGuid (&GuidHob->Name, &gLoaderPlatformInfoGuid)) {
        ConsolePrint ("     LOADER_PLATFORM_INFO\n");
        if (!DumpLoaderPlatformInfo (GuidHob)) {
          //
          // A bad payload only invalidates this decoder. The generic bounds
          // are untouched, so the walk continues and the failure is reported
          // in the summary.
          //
          DecodeErrors++;
        }
      } else if (CompareGuid (&GuidHob->Name, &gLoaderMemoryMapInfoGuid)) {
        ConsolePrint ("     MEMORY_MAP_INFO\n");
        if (!DumpMemoryMapInfo (GuidHob)) {
          DecodeErrors++;
        }
      } else if (CompareGuid (&GuidHob->Name, &gLoaderPerformanceInfoGuid)) {
        ConsolePrint ("     PERFORMANCE_INFO\n");
        if (!DumpPerformanceInfo (GuidHob)) {
          DecodeErrors++;
        }
      }
    }

    Count++;
    Current = (UINTN)GET_NEXT_HOB (Hob);
  }

  ConsolePrint ("ERROR: HOB count limit %d reached, valid HOBs: %d\n", HOB_EXPLORER_MAX_HOBS, Count);
}

/**
  Payload main entry.

  @param  Param           parameter passed from SwitchStack().
  @param  PldBase         payload base passed from SwitchStack().

**/
VOID
EFIAPI
PayloadMain (
  IN  VOID             *Param,
  IN  VOID             *PldBase
  )
{
  UINT8      Key;

  DEBUG ((DEBUG_INFO, "\n\n==================== Hello World ====================\n\n"));

  ExploreHobList ();

  Key = 0;
  while (Key != 0x1B) {
    if (ConsolePoll ()) {
      if (ConsoleRead (&Key, 1) > 0) {
        if ((Key >= 0x20) && (Key < 0x7F)) {
          ConsolePrint("Key '%c' pressed !\n", Key);
        }
      }
    }
  }

  DEBUG ((DEBUG_INFO, "\nExit\n"));
  CpuDeadLoop ();
}
