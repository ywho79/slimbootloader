# SBL HOB Explorer - Final Technical Report

Date: 2026-09-22

## Summary

SBL HOB Explorer implements the complete BUILD #2 scope: validated generic HOB traversal and exactly three typed decoders:

- `LOADER_PLATFORM_INFO`
- `MEMORY_MAP_INFO`
- `PERFORMANCE_INFO`

It is based on the native `PayloadPkg\HelloWorld` payload and preserves the existing `PayloadEntryLib` initialization path. The educational objective is:

> What does Slim Bootloader actually hand to a native payload?

The completed implementation was built for IA32 with VS2019 and exercised on QEMU q35 using TCG. The final run decoded 42 HOBs, including PHIT and the validated END marker.

The development branch is `hob-explorer` in the [ywho79/slimbootloader fork](https://github.com/ywho79/slimbootloader). This document accompanies the reviewed source in that public fork.

## 1. Files changed

| File | Changes |
|---|---|
| `PayloadPkg\HelloWorld\HelloWorld.c` | HOB acquisition, PHIT validation, bounded traversal, generic summaries, three typed decoders, symbolic-value helpers, and invocation before the existing input loop. |
| `PayloadPkg\HelloWorld\HelloWorld.inf` | Explicit `BootloaderLib` dependency and declarations for the three decoded GUIDs. |

`PayloadEntryLib`, the public structure headers, and the payload DSC were not modified.

`Platform\QemuBoardPkg\Binaries\HelloWorld.efi` is the generated payload staged for the QEMU image build. It is not implementation source.

## 2. Functions added

All 11 new functions are static and reside in `HelloWorld.c`.

| Function | Responsibility |
|---|---|
| `HobTypeToStr()` | Names standard PI HOB types while retaining numeric output. |
| `IsHobHeaderSane()` | Checks generic-header length, reserved field, and length alignment. |
| `DumpPhitHob()` | Validates PHIT and establishes the traversal endpoint. |
| `ExploreHobList()` | Acquires the list, validates and walks HOBs, dispatches decoders, and reports END and count. |
| `BootPartitionToStr()` | Maps public boot-partition values. |
| `BootModeToStr()` | Maps public PI boot-mode values. |
| `TpmTypeToStr()` | Maps public SBL TPM-type values. |
| `DumpLoaderPlatformInfo()` | Validates the fixed structure and prints the nine selected fields, excluding `SerialNumber`. |
| `MemMapTypeToStr()` | Maps the four public memory-map types. |
| `DumpMemoryMapInfo()` | Validates the flexible entry array and prints every advertised, validated entry. |
| `DumpPerformanceInfo()` | Validates timestamps, extracts IDs and ticks, and prints time and delta values. |

`PayloadMain()` already existed. Only the explorer invocation was added to it; the entry signature and existing input loop remain intact.

## 3. Public structures, APIs, and macros

### Public definitions

| Public source | Definitions used |
|---|---|
| `MdePkg\Include\Pi\PiHob.h` | `EFI_HOB_GENERIC_HEADER`, `EFI_HOB_HANDOFF_INFO_TABLE`, `EFI_HOB_GUID_TYPE`, HOB types, and PHIT version. |
| `BootloaderCommonPkg\Include\Guid\LoaderPlatformInfoGuid.h` | `LOADER_PLATFORM_INFO`, `gLoaderPlatformInfoGuid`, and TPM constants. |
| `BootloaderCommonPkg\Include\Guid\MemoryMapInfoGuid.h` | `MEMORY_MAP_INFO`, `MEMORY_MAP_ENTRY`, `gLoaderMemoryMapInfoGuid`, memory types, and payload flag. |
| `BootloaderCommonPkg\Include\Guid\PerformanceInfoGuid.h` | `PERFORMANCE_INFO` and `gLoaderPerformanceInfoGuid`. |
| `BootloaderCommonPkg\Include\Library\BootloaderCommonLib.h` | `GetHobListPtr()` declaration and `BOOT_PARTITION`. |
| `MdePkg\Include\Pi\PiBootMode.h` | Symbolic boot-mode constants. |

### Existing helpers

- `GetHobListPtr()` from the payload library reads the `PcdPayloadHobList` state established by `PayloadEntryLib`. The explorer does not treat `PayloadMain()`'s `Param` as the HOB pointer.
- `GET_HOB_TYPE`, `GET_HOB_LENGTH`, `GET_NEXT_HOB`, `GET_GUID_HOB_DATA`, and `GET_GUID_HOB_DATA_SIZE` come from `HobLib.h`.
- `CompareGuid()` comes from `BaseMemoryLib`.
- `OFFSET_OF` is used to measure flexible-array prefixes.
- `RShiftU64()` and `DivU64x32()` come from `BaseLib`.
- `ConsolePrint()` uses SBL PrintLib formatting, including `%p` for addresses, `%g` for GUIDs, and long integer formats for 64-bit values.

### No duplicated SBL structure definitions

The implementation contains no local replacements for the PI HOB structures, `LOADER_PLATFORM_INFO`, `MEMORY_MAP_INFO`, `MEMORY_MAP_ENTRY`, or `PERFORMANCE_INFO`.

All casts, `sizeof` expressions, and `OFFSET_OF` calculations use the authoritative public definitions. The local revision constant for platform information is a scalar policy value, not a duplicated structure layout.

This confirmation applies to the HOB Explorer implementation, not to every source file in the full SBL repository.

## 4. Validation strategy

### Generic HOB traversal

The explorer checks:

1. The acquired pointer is non-NULL and 8-byte aligned.
2. The initial PHIT extent does not overflow native pointer arithmetic.
3. The first HOB has HANDOFF type, the exact PHIT structure length, and a valid generic header.
4. PHIT uses the supported version.
5. PHIT physical-address fields fit `UINTN` before conversion.
6. Memory and free-memory bounds are ordered and contain PHIT appropriately.
7. The claimed END address is aligned and follows the complete PHIT.
8. The END header extent cannot overflow and fits within the used-memory and outer memory bounds.
9. Each subsequent header fits before its fields are read.
10. Each ordinary HOB has a positive, sufficiently large, aligned length and a zero reserved field.
11. Each ordinary HOB's extent stays before the claimed END header.
12. There is no premature END or duplicate HANDOFF HOB.
13. A GUID extension has at least `sizeof (EFI_HOB_GUID_TYPE)` bytes before its GUID or data-size macro is used.
14. The terminal HOB is at PHIT's exact end address, has END type, has exactly the generic-header length, and has a zero reserved field.
15. Traversal does not exceed the explorer-local limit of 512 HOBs.

`GET_NEXT_HOB()` is evaluated only after the length and bounds checks. The reported count includes PHIT and the validated END marker.

PHIT's `EfiEndOfHobList` points at the END header, not one byte past the list. The explorer does not walk arbitrary memory up to `EfiMemoryTop`.

### Fixed-size platform information

The decoder requires:

```c
DataSize >= sizeof (LOADER_PLATFORM_INFO)
```

before casting and reading the structure. It prints:

```text
Revision, BootPartition, BootMode, PlatformId, CpuCount,
HwState, Flags, LdrFeatures, TpmType
```

`SerialNumber` is not accessed or printed.

A revision other than the current producer's revision 2 emits a warning but remains size-gated. The size check establishes memory bounds; it does not prove layout compatibility with an unknown future revision.

### Flexible-array memory map

The decoder first requires the fixed prefix through `Entry`:

```c
FixedPrefixSize = OFFSET_OF (MEMORY_MAP_INFO, Entry);
```

Only after `DataSize >= FixedPrefixSize` may it read `Revision` and `Count`. Capacity is calculated as:

```text
Capacity = (ValidatedDataSize - FixedPrefixSize) / sizeof(MEMORY_MAP_ENTRY)
```

`Count` must not exceed capacity before iteration. This avoids underflow and avoids forming a potentially overflowing `Count * EntrySize` product.

The verified runtime values were:

```text
Data size    = 776 bytes
Fixed prefix = 8 bytes
Entry size   = 24 bytes
Capacity     = 32 entries
Count        = 10 entries
```

Every validated entry prints index, 64-bit Base, 64-bit Size, Type, and Flag. The public memory types are RAM, RESERVED, ACPI_RECLAIM, and ACPI_NVS. `MEM_MAP_FLAG_PAYLOAD` is identified while retaining the raw flag value.

### Flexible-array performance information

The equivalent capacity calculation is:

```text
FixedPrefixSize = OFFSET_OF(PERFORMANCE_INFO, TimeStamp)
Capacity = (ValidatedDataSize - FixedPrefixSize) / sizeof(UINT64)
```

The fixed prefix must be present before reading fields, and `Count <= Capacity` must hold before iteration.

The final runtime values were:

```text
Data size    = 360 bytes
Fixed prefix = 12 bytes
Capacity     = 43 timestamps
Count        = 43 timestamps
Padding      = 4 bytes
```

### Error handling and limits

Malformed typed data prints an explicit error and skips that decoder's iteration. The generic walk may continue because its independently validated bounds remain valid; the final summary reports typed decode failures.

The initial pointer's mapped-memory validity relies on the established payload-entry contract. The explorer cannot independently prove that memory is mapped, and `PayloadEntryLib` already consumes loader HOBs before `PayloadMain()`.

The recorded host boundary tests supplement the QEMU run. They are not firmware-binary fuzz tests and use host substitutes for portions of arithmetic or formatting.

## 5. Performance timestamp semantics

The decoder follows `BootloaderCommonPkg\Library\ShellLib\CmdPerf.c:69-74`:

```c
Id    = (UINT16)RShiftU64 (Stamp, 48);
Ticks = Stamp & 0x0000FFFFFFFFFFFFULL;
Time  = DivU64x32 (Ticks, PerfInfo->Frequency);
```

The upper 16 bits contain the measurement ID. The lower 48 bits contain the recorded TSC value.

### Frequency and time units

| Evidence | Source |
|---|---|
| HOB `Frequency` receives `PerfData.FreqKhz`. | `BootloaderCorePkg\Stage2\Stage2Hob.c:824` |
| `FreqKhz` receives `GetTimeStampFrequency()`. | `BootloaderCorePkg\Stage1A\Stage1A.c:485` |
| Timestamp-frequency function returns kHz. | `BootloaderCommonPkg\Library\TimeStampLib\TimeStampLib.c:28-45` |

Dividing ticks by frequency in kHz gives integer milliseconds, discarding the fractional remainder. No scaling multiplication is required.

`Time` is relative to the TSC origin. `Delta` is the difference between successive displayed millisecond values; the first entry uses zero as its baseline. Backward values print an unavailable delta rather than underflowing.

When frequency is zero, the decoder prints a warning and raw timestamps with `Time unavailable`. It does not divide.

The implementation retains numeric performance IDs to avoid an additional library dependency or a duplicated description table.

## 6. Authoritative performance-ID definitions

The current tree uses numeric IDs at measurement recording sites and a description lookup, rather than one central symbolic enum for the standard bootloader IDs.

| Authority | Source |
|---|---|
| Default bootloader ID descriptions | `BootloaderCommonPkg\Library\LoaderPerformanceLib\LoaderPerformancePrintLib.c:45-143`, `DefPerfIdToStr()` |
| Description lookup and optional override | Same file, `PerfIdToStr()` at `:200-218` |
| Public measurement and lookup API declarations | `BootloaderCommonPkg\Include\Library\LoaderPerformanceLib.h` |
| ID/timestamp packing and recording | `BootloaderCommonPkg\Library\LoaderPerformanceLib\LoaderPerformanceAddLib.c:20-48`, `AddMeasurePointTimestamp()` and `AddMeasurePoint()` |
| HOB construction from recorded measurements | `BootloaderCorePkg\Stage2\Stage2Hob.c:817-826` |
| Existing HOB consumer and unpacking | `BootloaderCommonPkg\Library\ShellLib\CmdPerf.c:68-76` |

Verified examples from the description table:

| ID | Description |
|---|---|
| `0x1000` | `Reset vector` |
| `0x1010` | `Stage1A entry point` |
| `0x1040` | `Board PostTempRamInit hook` |
| `0x1060` | `Stage1A continuation` |
| `0x1080` | `Load Stage1B` |
| `0x10A0` | `Verify Stage1B` |
| `0x10B0` | `Decompress Stage1B` |
| `0x2000` | `Stage1B entry point` |
| `0x3000` | `Stage2 entry point` |

The actual recording sites establish stage ownership:

- `BootloaderCorePkg\Stage1A\Stage1A.c`: the observed `0x1xxx` measurements, including initialization of the first timestamps.
- `BootloaderCorePkg\Stage1B\Stage1B.c`: the observed `0x2xxx` measurements.
- `BootloaderCorePkg\Stage2\Stage2.c`: the observed `0x3xxx` measurements, including Stage2's payload-loading operations.

`PayloadPkg\Library\PayloadEntryLib\PayloadEntryLib.c:270-271` separately records `0x4000`. The default core description table does not name that ID.

FSP and CSME performance descriptions have separate lookup paths. They were not added to the explorer.

Line references identify the checkout reviewed for this report and may shift in later revisions.

## 7. Build and run commands

These commands reproduce the successful IA32/VS2019 build on the development workstation. Tool and key paths are workstation-specific; signing keys remain outside the repository.

```powershell
Set-Location C:\slimbootloader

$env:SBL_KEY_DIR = 'C:\SBLKeys'
$env:OPENSSL_PATH = 'c:\OpenSSL\bin'
$env:NASM_PREFIX = 'C:\Nasm\'
$env:IASL_PREFIX = 'C:\ASL\'

python BuildLoader.py build_dsc `
  -p PayloadPkg\PayloadPkg.dsc `
  -a ia32 `
  -t VS2019

New-Item -ItemType Directory -Force `
  Platform\QemuBoardPkg\Binaries | Out-Null

Copy-Item `
  Build\PayloadPkg\DEBUG_VS2019\IA32\PayloadPkg\HelloWorld\HelloWorld\OUTPUT\HelloWorld.efi `
  Platform\QemuBoardPkg\Binaries\HelloWorld.efi

python BuildLoader.py build qemu -a ia32 -p HelloWorld.efi
```

Check that each build succeeds before staging or running its output.

The selected payload is staged in the QEMU board's `Binaries` directory and selected by filename with `-p HelloWorld.efi`.

The final SBL image is:

```text
Outputs\qemu\SlimBootloader.bin
```

Run it with:

```powershell
& 'C:\Program Files\qemu\qemu-system-x86_64.exe' `
  -machine q35,accel=tcg `
  -cpu max `
  -m 256M `
  -nographic `
  -serial mon:stdio `
  -no-reboot `
  -drive 'file=Outputs\qemu\SlimBootloader.bin,if=pflash,format=raw'
```

After inspecting the output, exit QEMU with Ctrl+A, then X. The existing HelloWorld input loop remains active after the explorer completes.

The stock `qemu_test.py` OS/FWU-oriented suite is not the acceptance test for this native payload.

## 8. Recorded validation results

| Check | Result |
|---|---|
| Payload build | IA32/VS2019 build succeeded. |
| SBL image build | QEMU image build succeeded. |
| Platform information | Revision 2; all nine selected fields; no `SerialNumber`. |
| Memory map | 10 entries validated against capacity 32. |
| Performance information | 43 timestamps validated against capacity 43. |
| Timestamp conversions | All captured millisecond values and deltas checked against raw ticks and frequency. |
| END marker | `BEC14C8`, matching PHIT. |
| Final count | 42 HOBs, including PHIT and END. |
| Typed decode errors | None in the final run. |

Final traversal output:

```text
 41: BEC14C8  Type 0xFFFF END_OF_HOB_LIST  Length 0x0008

Total HOBs: 42 (including PHIT and END)
```

Host boundary cases additionally exercised truncated prefixes, zero counts, excessive counts, exact capacities, trailing capacity/padding, 64-bit values, zero frequency, and backward timestamps. These harnesses test decoder logic with host substitutions; they do not reproduce firmware memory mapping or inject malformed HOBs into the IA32 payload.

The final runtime capture was inspected during validation but is not published with this source-only example. The recorded results above summarize that run.

## 9. Scope and publication assessment

The agreed BUILD #2 functionality is complete. The implementation demonstrates native-payload HOB acquisition, bounded generic traversal, fixed-size decoding, flexible-array validation, and packed timestamp interpretation.

It adds no decoder beyond the three planned GUID HOBs and does not expose `SerialNumber`.

This document provides the build/run instructions, scope, and validation limits for the published example. QEMU's fallback timestamp frequency and TCG execution mean the displayed timings are illustrative emulator values, not silicon boot-time benchmarks.

The repository contains reviewed source and documentation only, not generated binaries, signing keys, logs, or private artifacts.
