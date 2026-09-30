# SBL HOB Explorer

A simple native Slim Bootloader payload that explores the HOB list handed to a payload.

It extends the existing `PayloadPkg/HelloWorld` example and demonstrates:

- Validated traversal of the loader HOB list
- `LOADER_PLATFORM_INFO`
- `MEMORY_MAP_INFO`
- `PERFORMANCE_INFO`

## Example

```text
SBL HOB Explorer
================

LOADER_PLATFORM_INFO
Revision 2  BootPartition 0 (PrimaryPartition)
PlatformId 0x0001  CpuCount 1

MEMORY_MAP_INFO
Revision 1  Count 10 (capacity 32)

PERFORMANCE_INFO
Revision 1  Count 43 (capacity 43)
Frequency 800000 KHz

Total HOBs: 42 (including PHIT and END)
```

## Build

Build the HelloWorld payload:

```powershell
python BuildLoader.py build_dsc `
  -p PayloadPkg\PayloadPkg.dsc `
  -a ia32 `
  -t VS2019
```

Stage `HelloWorld.efi`, then build the QEMU image:

```powershell
python BuildLoader.py build qemu -a ia32 -p HelloWorld.efi
```

The example was validated using QEMU q35 with TCG.

> Performance values from QEMU/TCG are for demonstration only and should not be treated as physical-hardware boot-time measurements.

## More

A detailed walkthrough of the SBL-to-payload HOB handoff and how this explorer works will be published separately.
