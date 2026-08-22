param(
    [Parameter(Mandatory=$true)]
    [string]$RemoteUrl
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (Test-Path .git) {
    throw "This folder is already a Git repository. Review the existing remote before publishing."
}

git init -b main
git add -- . ':!build' ':!build-*'
git commit -m "chore: bootstrap ardirec foundation"
git remote add origin $RemoteUrl
git push -u origin main
