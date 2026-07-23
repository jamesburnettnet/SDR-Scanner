# Vendored Windows dependencies

CMake's normal discovery path (pkg-config + system dev packages) doesn't
exist on Windows, so the Windows/MinGW build links against the files
vendored here instead of anything installed system-wide.

## Contents

- `include/rtl-sdr.h`, `include/rtl-sdr_export.h` — librtlsdr's public C API
  headers, from the
  [librtlsdr/librtlsdr](https://github.com/librtlsdr/librtlsdr) `v0.9.0`
  tag.
- `win64/librtlsdr.dll` — prebuilt librtlsdr 0.9.0 for 64-bit Windows,
  downloaded from the
  [`v0.9.0` GitHub release](https://github.com/librtlsdr/librtlsdr/releases/tag/v0.9.0)
  (`rtlsdr-bin-w64_dlldep.zip`), built by that project's own CI via a
  mingw-w64 cross-compiler.
- `win64/librtlsdr.dll.a` — MinGW import library generated locally from the
  DLL above (`gendef` + `dlltool`, both part of any mingw-w64 binutils,
  including the Qt MinGW 13.1.0 kit's), since the upstream release doesn't
  ship one. Regenerate if `librtlsdr.dll` is ever updated:
  ```
  gendef librtlsdr.dll
  dlltool -d librtlsdr.def -D librtlsdr.dll -l librtlsdr.dll.a
  ```

## Why there's no libusb DLL here

On Linux, `librtlsdr.so` dynamically links `libusb-1.0.so` at runtime, so a
matching libusb has to be installed separately. The Windows build above is
different: libusb is **statically linked into `librtlsdr.dll`** using its
WinUSB backend (confirmed by inspecting the DLL's PE import table — it only
references `KERNEL32.dll`, `ADVAPI32.dll`, and `msvcrt.dll`, no libusb DLL).
There is nothing to vendor for it.

This does NOT remove the usual RTL-SDR-on-Windows driver step: Windows
auto-installs RTL2832U dongles as a DVB-T TV tuner by default, not as a
generic USB device, so each dongle still needs to be bound to the **WinUSB**
driver once, e.g. with [Zadig](https://zadig.akeo.ie/). WinUSB itself ships
with Windows (7+) — Zadig just rebinds the one device, it isn't installing a
new driver package. This is standard setup for any RTL-SDR software on
Windows, not something specific to this build.

## Compiler ABI compatibility

`librtlsdr.dll` only exports a plain C API (no C++ symbols crossing the DLL
boundary), so it's ABI-stable across mingw-w64 GCC versions — it was linked
successfully against the Qt-bundled MinGW **13.1.0** toolchain (see the
repo's `README.md` for the exact Qt/MinGW versions used), but isn't tied to
that specific compiler version.
