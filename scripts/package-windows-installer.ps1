# Inno build and end-to-end per-user install/launch/uninstall smoke.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$StageDir,
    [Parameter(Mandatory)][string]$OutputDir
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Version -notmatch '^0\.2\.0-(alpha|rc)\.\d+$') { throw "Invalid candidate version: $Version" }
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$stage = (Resolve-Path $StageDir).Path
if (-not (Test-Path (Join-Path $stage 'ardirec.exe') -PathType Leaf)) { throw 'Staged executable missing' }
foreach ($dll in @('MSVCP140.dll','MSVCP140_1.dll','MSVCP140_2.dll','VCRUNTIME140.dll','VCRUNTIME140_1.dll')) {
    if (-not (Test-Path (Join-Path $stage $dll))) { throw "Missing VC runtime: $dll" }
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$out = (Resolve-Path $OutputDir).Path
$programFiles32 = (Get-Item 'Env:ProgramFiles(x86)' -ErrorAction SilentlyContinue).Value
$candidates = @((Join-Path $programFiles32 'Inno Setup 6\ISCC.exe'), (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'))
$iscc = $candidates | Where-Object { Test-Path $_ -PathType Leaf } | Select-Object -First 1
if (-not $iscc) { throw 'Inno Setup 6 compiler is missing' }
& $iscc "/DMyAppVersion=$Version" "/DMySourceDir=$stage" "/DMyOutputDir=$out" (Join-Path $root 'installer/ardirec.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno compiler failed: $LASTEXITCODE" }
$base = "ArDiRec-v$Version-Windows-x64-Setup"
$setup = Join-Path $out "$base.exe"
if (-not (Test-Path $setup -PathType Leaf)) { throw "Missing installer: $setup" }
$installDir = Join-Path $env:RUNNER_TEMP "ardirec-smoke-$PID"
$log = Join-Path $env:RUNNER_TEMP "ardirec-smoke-$PID.log"
try {
    if (Test-Path $installDir) { throw "Test directory already exists: $installDir" }
    $args = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$installDir","/LOG=$log")
    $installed = Start-Process -FilePath $setup -ArgumentList $args -PassThru -Wait
    if ($installed.ExitCode -ne 0) { throw "Installer exit: $($installed.ExitCode)" }
    $exe = Join-Path $installDir 'ardirec.exe'
    if (-not (Test-Path $exe -PathType Leaf)) { throw 'Setup did not install ardirec.exe' }
    if (-not (Test-Path (Join-Path $installDir 'platforms/qwindows.dll'))) { throw 'Qt Windows platform plugin not deployed' }
    $env:QT_QPA_PLATFORM = 'offscreen'
    $app = Start-Process -FilePath $exe -WorkingDirectory $installDir -PassThru
    Start-Sleep -Seconds 5
    if ($app.HasExited) { throw "Installed application exited early: $($app.ExitCode)" }
    Stop-Process -Id $app.Id -Force
    $app.WaitForExit()
    Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
    $uninstaller = Join-Path $installDir 'unins000.exe'
    if (-not (Test-Path $uninstaller -PathType Leaf)) { throw 'Uninstaller missing' }
    $uninstall = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -PassThru -Wait
    if ($uninstall.ExitCode -ne 0) { throw "Uninstaller exited: $($uninstall.ExitCode)" }
    if (Test-Path $exe) { throw 'Uninstall left application executable behind' }
} catch {
    if (Test-Path $log) { Get-Content $log -Tail 100 }
    throw
} finally {
    if (Get-Variable -Name app -ErrorAction SilentlyContinue) {
        if ($app -and -not $app.HasExited) { Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue }
    }
}
$readme = Join-Path $out 'README-Windows.txt'
@"
ArDiRec $Version - Windows 10/11 x64
1. Extract this ZIP.
2. Double-click $base.exe and follow the setup wizard.
3. Launch ArDiRec from the Windows Start menu (no administrator access needed).
4. Uninstall through Windows Settings > Installed apps > ArDiRec.
Existing per-user installations are updated in place.
This development candidate is UNSIGNED. A ZIP does not remove SmartScreen warnings.
Verify the official GitHub release and SHA256SUMS.txt before installing.
https://github.com/masarray/ardirec
"@ | Set-Content -Path $readme -Encoding UTF8
$zip = Join-Path $out "$base.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -LiteralPath @($setup,$readme) -DestinationPath $zip -CompressionLevel Optimal
if (-not (Test-Path $zip) -or (Get-Item $zip).Length -lt 1MB) { throw "Invalid ZIP: $zip" }
Write-Host "Inno install/startup/uninstall: PASS"
Get-FileHash -Path $setup,$zip -Algorithm SHA256 | Format-Table Path,Hash
