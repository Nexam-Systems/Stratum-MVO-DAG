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

## 2. Build + package (one shot)

The repo's `build-windows.ps1` configures Release (tests off) with `qt-cmake`,
builds, deploys the Qt runtime with `windeployqt`, and produces **both** the NSIS
`.exe` and the WiX `.msi`. Point it at your Qt kit:

```powershell
.\build-windows.ps1 -QtKit C:\Qt\6.10.3\msvc2022_64
```

Useful overrides:

```powershell
.\build-windows.ps1 -Jobs 4 -QtKit C:\Qt\6.10.3\msvc2022_64
```

Resume or diagnose a single stage instead of restarting the whole run:

```powershell
.\build-windows.ps1 -Stage Configure -QtKit C:\Qt\6.10.3\msvc2022_64
.\build-windows.ps1 -Stage Build
.\build-windows.ps1 -Stage Package -WixBin C:\tools\wix3 -NsisDir "C:\Program Files (x86)\NSIS"
```

The first `configure` is slow — `qt-cmake` resolves the full Qt module set. That
is expected, not a hang. Build files and the run log land under `build\windows\`.

---

## 3. Collect the installer / run

Outputs:

- **MSI:** `build\windows\msi\STRATUM-<version>-x64.msi`
- **NSIS installer:** `build\windows\STRATUM-installer-*.exe`

Install by double-clicking the MSI. Installers are **unsigned** (same as CI).

To test without installing, run the deployed executable directly:

```powershell
.\build\Release\STRATUM.exe
```

---

## 4. Manual sequence (fallback / understanding)

This is exactly what CI runs, so it is proven. Use it if you would rather drive
the steps yourself or a scripted stage fails.

```powershell
# Developer PowerShell for VS 2022
cd C:\dev\Stratum-MVO-DAG\qgroundcontrol

# Configure (Release, no tests) — for a distributable app
& "C:\Qt\6.10.3\msvc2022_64\bin\qt-cmake.bat" -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DQGC_BUILD_TESTING=OFF

# Build
cmake --build build --parallel

# Deploy Qt runtime + build the NSIS installer in one step
cmake --install build --prefix staging
```

For a first-time development build with tests, use `-DCMAKE_BUILD_TYPE=Debug
-DQGC_BUILD_TESTING=ON`, then run `.\build\Debug\STRATUM.exe`.

Optional tests:

```powershell
cd build
ctest --output-on-failure -L "Unit|Integration" -LE "Flaky|Network"
```

---

## 5. Common pitfalls

- **MinGW kit selected in Qt.** ABI mismatch against MSVC-built Qt → link failure.
  Re-run the Qt installer and pick **MSVC 2022 64-bit**.
- **Qt < 6.10.** `configure` aborts — the tree enforces `qt_minimum_version 6.10.0`.
- **Plain PowerShell + Ninja.** `cl.exe` not found. Use **Developer PowerShell for
  VS 2022** (or `vcvars64.bat`).
- **Missing Qt module.** `configure` fails naming the module — add it via the Qt
  Maintenance Tool (see the module list in §0).
- **Building under OneDrive.** See the warning above — the most common cause of a
  flaky local build.
- **GStreamer.** Skip it first. When added, it must be the **MSVC x64** build; any
  other ABI will not link.

---

## Appendix — CI is now manual-only, and reclaiming storage

`ci-build.yml` no longer builds on push/PR; it runs only from the **Actions** tab
(**Run workflow**). `build-on-tag.yml` is unchanged — a `v*` tag still builds a
release. Disabling future runs does **not** free the quota already consumed;
delete the stored artifacts:

```powershell
# GitHub CLI, authenticated to the account
gh api /repos/Nexam-Systems/Stratum-MVO-DAG/actions/artifacts --paginate --jq ".artifacts[].id" |
  ForEach-Object { gh api -X DELETE /repos/Nexam-Systems/Stratum-MVO-DAG/actions/artifacts/$_ }
```

Or delete per-run in the UI. Usage is shown under **Settings → Billing → Storage
for Actions**.
