# Building QGroundControl (STRATUM-Desktop) on Windows

## Local Release build and MSI (no GitHub Actions minutes)

From the repository root in PowerShell, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build-windows.ps1
```

The script initializes MSVC 2022, finds its bundled Ninja, configures Qt
6.10.3 in Release mode with tests disabled, builds, deploys Qt runtime files,
and generates the same NSIS and WiX v3 installers as the GitHub workflows.
It uses `installer/windows/stratum.wxs` for the MSI definition.

Prerequisites: MSVC 2022 C++ tools and Windows SDK, Qt 6.10.3 MSVC x64 with
the modules listed in the CI workflow, CMake, Git, Python 3.10+, NSIS, and
WiX v3 (`heat`, `candle`, `light`). The first configure downloads dependencies.
The script also recognizes WiX extracted to `build/tools/wix314` and the
Python environment created by QGC under `qgroundcontrol/.venv`.
Downloads have a one-hour timeout for slower connections; override it with
`-DownloadTimeoutSeconds`. This changes download behavior only.

Defaults and overrides:

```powershell
.\build-windows.ps1 -Jobs 4 -QtKit C:\Qt\6.10.3\msvc2022_64
# Individual stages for resuming or diagnosing a failure:
.\build-windows.ps1 -Stage Configure -PythonExe C:\Python312\python.exe
.\build-windows.ps1 -Stage Build
.\build-windows.ps1 -Stage Package -WixBin C:\tools\wix3 -NsisDir 'C:\Program Files (x86)\NSIS'
```

Build files and the appended run log are under `build/windows`. The MSI is
`build/windows/msi/STRATUM-<version>-x64.msi`; the NSIS installer is
`build/windows/STRATUM-installer-*.exe`. Each packaging run creates a fresh
`staging-<id>` folder under the build directory to exclude stale payload files.
These folders contain deployable application files and consume disk space.
The MSI version is derived from local Git tags, as in CI. Nothing is uploaded,
tagged, or installed by the build script. Installers are unsigned, matching CI.

The remaining sections describe the manual development build.

Target tree: **v5.0.3 + 1086 commits**. Authoritative versions from `qgroundcontrol/build-config.json`:

| Component | Version | Notes |
|-----------|---------|-------|
| Compiler  | **MSVC 2022 (v143)** | `win64_msvc2022_64` — the *only* supported Windows toolchain. **Not MinGW.** |
| Qt        | **6.10.3** (min 6.10.0) | MSVC 2022 64-bit kit |
| CMake     | **>= 3.25** | Qt bundles a recent one |
| Build tool| **Ninja** | ships with Qt "Developer and Designer Tools" |
| GStreamer | 1.28.1 MSVC x64 | optional — video streaming only |

The build is driven by **`qt-cmake`** (Qt's CMake wrapper that injects the correct
`CMAKE_PREFIX_PATH`). This mirrors the repo's `justfile` `configure` / `release` recipes.

---

## Step 0 — Verify the environment

```powershell
cd "C:\Users\Anas-NX\OneDrive\NEXAM\10_Systems_Engineering\Systems_Engineering\STRATUM-Desktop"
powershell -ExecutionPolicy Bypass -File .\check-build-env.ps1
```

Resolve any `BLOCKERS` before continuing.

---

## Step 1 — Open the right shell

You **must** build from an environment where MSVC's `cl.exe` is on PATH. Open:

> **Start → "Developer PowerShell for VS 2022"**

(Using a plain PowerShell with the Ninja generator will fail to find the compiler.)

Then move into the repo:

```powershell
cd "C:\Users\Anas-NX\OneDrive\NEXAM\10_Systems_Engineering\Systems_Engineering\STRATUM-Desktop\qgroundcontrol"
```

Set the path to your Qt kit (adjust the version folder if the checker reported a different one):

```powershell
$QtKit = "C:\Qt\6.10.3\msvc2022_64"
```

---

## Step 2a — Configure + build (Debug, with tests) — recommended first build

```powershell
& "$QtKit\bin\qt-cmake.bat" -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    -DQGC_BUILD_TESTING=ON

cmake --build build --parallel
```

## Step 2b — Configure + build (Release, no tests) — for a distributable app

```powershell
& "$QtKit\bin\qt-cmake.bat" -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DQGC_BUILD_TESTING=OFF

cmake --build build --parallel
```

---

## Step 3 — Run

```powershell
.\build\Debug\QGroundControl.exe      # or .\build\Release\QGroundControl.exe
```

## Step 4 (optional) — Tests

```powershell
cd build
ctest --output-on-failure -L "Unit|Integration" -LE "Flaky|Network"
```

---

## Common pitfalls (the tradespace, not generic advice)

- **MinGW kit selected in Qt installer.** Links against MSVC-built Qt/GStreamer binaries →
  ABI mismatch, fails at link time. Re-run the Qt installer and pick *MSVC 2022 64-bit*.
- **Qt < 6.10.** CMake configure aborts: this tree enforces `qt_minimum_version 6.10.0`.
- **Plain PowerShell + Ninja.** `cl.exe` not found. Use *Developer PowerShell for VS 2022*,
  or call `vcvars64.bat` first.
- **First configure is slow.** `qt-cmake` resolves the full Qt module set
  (qtmultimedia, qtlocation, qtquick3d, etc.). This is expected, not a hang.
- **GStreamer.** Skip it for the first build. When you add it, it must be the *MSVC x64*
  build (1.28.1) — the MinGW/other ABI will not link.
