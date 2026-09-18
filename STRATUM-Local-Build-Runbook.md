# STRATUM (QGroundControl fork) — Local Windows Build Runbook

**Repo:** `Stratum-MVO-DAG` (the QGC tree lives under `qgroundcontrol/`)
**Toolchain:** MSVC 2022 (v143) · Qt 6.10.3 `msvc2022_64` · Ninja · `qt-cmake`
**Output:** `STRATUM.exe` + NSIS `.exe` installer + WiX `.msi`
**Why local:** GitHub Actions artifact storage is the bottleneck; the code
compiles cleanly on this exact toolchain (that is what CI proved). Builds are now
produced on the machine. CI auto-build is disabled; the workflow remains runnable
on demand from the **Actions** tab.

> This is the same toolchain and sequence as `BUILD-Windows.md` and the (now
> manual-only) `ci-build.yml`, consolidated into one runbook and tuned for a
> local build.

---

## 0. Prerequisites

Install/confirm these once. You already have **Qt 6.10.3**; make sure the rest are present.

| Component | Requirement | Notes |
|-----------|-------------|-------|
| Compiler | **Visual Studio 2022**, *Desktop development with C++* + Windows SDK | The **only** supported Windows compiler. **Not MinGW** — MinGW links against MSVC-built Qt and fails at link time. |
| Qt | **6.10.3**, kit **`msvc2022_64`** (min 6.10.0) | Install with the Qt Online Installer / Maintenance Tool. |
| Qt modules | `qtgraphs qtlocation qtpositioning qtspeech qtmultimedia qtserialport qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors qtscxml qtwebsockets qthttpserver` | Exactly what QGC's own CI installs. A missing module aborts `configure`. |
| CMake | **≥ 3.25** | Bundled with Qt's *Developer and Designer Tools*. |
| Ninja | any recent | Also bundled with Qt's *Developer and Designer Tools*. |
| Python | **3.10+** | Used by the build/deploy scripts. |
| NSIS | v3 | Installer step only. |
| WiX Toolset | **v3** (`heat`, `candle`, `light`) | MSI step only. |
| GStreamer | *optional*, MSVC x64 1.28.1 | Video streaming only — **skip for the first build**. |

Verify with the repo's checker (plain PowerShell):

```powershell
cd C:\dev\Stratum-MVO-DAG
powershell -ExecutionPolicy Bypass -File .\check-build-env.ps1
```

Resolve every `BLOCKER` before continuing. `WARN` items (Node, etc.) do not block a core build.

---

## ⚠️ Build off OneDrive

This repo lives under `OneDrive\…`. OneDrive fights a C++ build the same way it
fights `.git`: it locks and re-syncs the thousands of object files the build
generates, which corrupts or stalls it. Do **one** of:

- **Preferred:** copy the repo to plain local disk and build there —
  `C:\dev\Stratum-MVO-DAG`.
- Or pause OneDrive for the whole build.
- Or exclude the `build/` folder from OneDrive sync.

A mysterious local-build failure is, more often than not, this.

---

## 1. Open the right shell

Start → **Developer PowerShell for VS 2022**.

A plain PowerShell will not have `cl.exe` on `PATH`, and the Ninja generator will
fail to find the compiler. (Alternatively, call `vcvars64.bat` first in a normal
shell.)

```powershell
cd C:\dev\Stratum-MVO-DAG
```

---

## Go to right directory
```powershell
cd C:\dev\mvo
```
---
## Remove any old builds
```powershell
Remove-Item -Recurse -Force .\build\windows
```
---
## Configure
```powershell
.\build-windows.ps1 -Stage Configure -QtKit C:\Qt\6.10.3\msvc2022_64
```
---
## Actual Build
```powershell
.\build-windows.ps1 -Stage Build -Jobs 4
```
---
## Release EXE
```powershell
.\build\windows\Release\STRATUM.exe
```
---
## Release Installer
```powerhshell
.\build-windows.ps1 -Stage Package -QtKit C:\Qt\6.10.3\msvc2022_64 -WixBin C:\tools\wix3
```
---
