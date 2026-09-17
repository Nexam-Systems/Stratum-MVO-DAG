# Local equivalent of .github/workflows/ci-build.yml and package-windows.
[CmdletBinding()]
param(
    [string]$QtKit = 'C:\Qt\6.10.3\msvc2022_64',
    [string]$BuildDir,
    [string]$PythonExe,
    [string]$WixBin,
    [string]$NsisDir,
    [ValidateRange(120,86400)][int]$DownloadTimeoutSeconds = 3600,
    [ValidateRange(1,128)][int]$Jobs = 4,
    [ValidateSet('All','Configure','Build','Package')][string]$Stage = 'All'
)
$ErrorActionPreference = 'Stop'
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed (exit $LASTEXITCODE)." }
}
$source = Join-Path $PSScriptRoot 'qgroundcontrol'
if (-not $BuildDir) { $BuildDir = Join-Path $PSScriptRoot 'build\windows' }
if (-not [IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path (Get-Location).Path $BuildDir }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
if (-not $PythonExe -and (Test-Path "$source\.venv\Scripts\python.exe")) {
    $PythonExe = "$source\.venv\Scripts\python.exe"
}
if (-not $WixBin -and (Test-Path "$PSScriptRoot\build\tools\wix314\heat.exe")) {
    $WixBin = "$PSScriptRoot\build\tools\wix314"
}
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'Install Visual Studio 2022 Build Tools with Desktop development with C++.' }
$vs = & $vswhere -latest -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC 2022 C++ tools not found.' }
# Import the developer environment into this process (a plain .bat call cannot do this).
$devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
$devEnvironment = & $env:ComSpec /d /c "call `"$devcmd`" -no_logo -arch=x64 -host_arch=x64 && set"
if ($LASTEXITCODE -ne 0) { throw 'Could not initialize MSVC.' }
foreach ($line in $devEnvironment) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
$env:Path = "$QtKit\bin;$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:Path"
if ($PythonExe) { $env:Path = "$(Split-Path $PythonExe);$env:Path" }
if ($NsisDir) { $env:Path = "$NsisDir;$env:Path" }
if (-not $WixBin) {
    $heat = Get-ChildItem "${env:ProgramFiles(x86)}\WiX Toolset v3*\bin\heat.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($heat) { $WixBin = $heat.DirectoryName }
}
if ($WixBin) { $env:Path = "$WixBin;$env:Path" }
foreach ($tool in @('cmake','ninja','git')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool not found." }
}
if (-not (Test-Path "$QtKit\bin\qt-cmake.bat")) { throw "Qt kit not found: $QtKit" }
New-Item -ItemType Directory -Force $BuildDir | Out-Null
Start-Transcript -Path (Join-Path $BuildDir 'local-build.log') -Append | Out-Null
try {
    if ($Stage -in @('All','Configure')) {
        Invoke-Checked "$QtKit\bin\qt-cmake.bat" @('-S',$source,'-B',$BuildDir,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DQGC_BUILD_TESTING=OFF',"-DQGC_DOWNLOAD_TIMEOUT=$DownloadTimeoutSeconds")
    }
    if ($Stage -in @('All','Build')) {
        Invoke-Checked 'cmake' @('--build',$BuildDir,'--parallel',"$Jobs")
    }
    if ($Stage -in @('All','Package')) {
        foreach ($tool in @('heat','candle','light')) {
            if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw 'WiX v3 is required. Supply -WixBin with its bin directory.' }
        }
        # Unique staging prevents obsolete files from entering a subsequent installer.
        $staging = Join-Path $BuildDir ('staging-' + [guid]::NewGuid().ToString('N'))
        Invoke-Checked 'cmake' @('--install',$BuildDir,'--prefix',$staging)
        if (-not (Test-Path "$staging\bin\STRATUM.exe")) { throw 'Deployed STRATUM.exe missing.' }
        $raw = & git -C $PSScriptRoot describe --tags --always
        if ($LASTEXITCODE -ne 0) { throw 'Cannot determine source version.' }
        $version = '0.0.0'
        if ($raw -match '(\d+)\.(\d+)\.(\d+)(?:\.(\d+))?') { $version = $matches[0] }
        $parts = @($version.Split('.') | ForEach-Object { [int]$_ })
        $parts[0] = [Math]::Min(255,$parts[0]); $parts[1] = [Math]::Min(255,$parts[1])
        $version = $parts -join '.'
        $out = Join-Path $BuildDir 'msi'
        New-Item -ItemType Directory -Force $out | Out-Null
        $msi = Join-Path $out "STRATUM-$version-x64.msi"
        Invoke-Checked 'heat' @('dir',$staging,'-nologo','-cg','PayloadComponents','-dr','INSTALLFOLDER','-ke','-srd','-scom','-sreg','-sfrag','-gg','-g1','-var','var.PayloadDir','-out',"$out\payload.wxs")
        Invoke-Checked 'candle' @('-nologo','-arch','x64',"-dPayloadDir=$staging","-dAppVersion=$version",'-dManufacturer=Nexam Systems','-out',"$out\",(Join-Path $PSScriptRoot 'installer\windows\stratum.wxs'),"$out\payload.wxs")
        Invoke-Checked 'light' @('-nologo','-ext','WixUIExtension','-spdb','-cultures:en-us','-out',$msi,"$out\stratum.wixobj","$out\payload.wixobj")
        if (-not (Test-Path $msi)) { throw 'MSI was not produced.' }
        Get-Item $msi
        Get-FileHash $msi -Algorithm SHA256
        Write-Host "Installer ready: $msi"
    }
} finally { Stop-Transcript | Out-Null }
