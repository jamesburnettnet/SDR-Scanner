# Version

Current version: **0.2.0**

## Where the version is coded

- `CMakeLists.txt` (line 2) — `project(SdrScanner VERSION 0.2.0 LANGUAGES CXX)`.
  This is the source-tree fallback version (`PROJECT_VERSION`), used for local/dev
  builds when `APP_VERSION` isn't overridden.
- `CMakeLists.txt` (`APP_VERSION` / `APP_VERSION_STRING`) — `APP_VERSION` defaults
  to `PROJECT_VERSION` above, but can be overridden at configure time with
  `-DAPP_VERSION=<version>` (used by CI release builds, driven by the git tag —
  see `.github/workflows/release.yml`). `APP_VERSION` is compiled in as the
  `APP_VERSION_STRING` preprocessor define for `src/ui/MainWindow.cpp`.
- `src/ui/MainWindow.cpp` — reads `APP_VERSION_STRING` and sets it in the main
  window title (`"SDR Scanner v%1"`). Falls back to `"dev"` if undefined.

## To bump the version

1. Update the version number in `CMakeLists.txt` (`project(... VERSION x.y.z ...)`).
2. Update the "Current version" line at the top of this file.
3. Tagged releases (`.github/workflows/release.yml`) derive `APP_VERSION` from the
   git tag instead, so the CMakeLists.txt value only matters for local/dev builds
   and as the fallback source of truth.
