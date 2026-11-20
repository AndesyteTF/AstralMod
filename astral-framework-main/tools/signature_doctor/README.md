# Astral Signature Doctor

This standalone maintenance tool validates the byte patterns in
`tf2/dev_tables.cpp` against installed TF2 modules. It is intentionally not
linked into Astral.

When an updated module no longer contains a pattern, pass a known-good module
directory with `--baseline`. The tool locates the old pattern, fingerprints its
x86-64 instruction stream with Capstone, searches the updated module for that
fingerprint, and prints a wildcarded replacement candidate when the result is
unique. Candidates must be reviewed before updating Astral or its production
signature package.

## Build

From this directory:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

## Usage

```powershell
./build/Release/astral-signature-doctor.exe `
  --tables ../../tf2/dev_tables.cpp `
  --modules "C:/Program Files (x86)/Steam/steamapps/common/Team Fortress 2"
```

Repair assistance using saved pre-update modules:

```powershell
./build/Release/astral-signature-doctor.exe `
  --tables ../../tf2/dev_tables.cpp `
  --modules C:/tf2-current `
  --baseline C:/tf2-known-good
```

Use `--only <signature-id>` to inspect one entry. Exit code `0` means every
selected pattern was unique, `1` means at least one was missing or ambiguous,
and `2` indicates invalid input.

The module directory may be a TF2 root, a `bin/x64` directory, or a flat
directory of archived DLLs. Modules installed outside TF2 (notably Steam's
`steamclient64.dll`) should be copied into the flat directory when auditing
those entries.

