# Build doom.wup with libwup (Windows PowerShell).
# Install LLVM from https://releases.llvm.org/ (default C:\Program Files\LLVM)
# and `python -m pip install meson ninja pillow` first.
$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$llvm = Join-Path ${env:ProgramFiles} 'LLVM\bin'
if (Test-Path $llvm) { $env:PATH = "$llvm;$env:PATH" }
$scripts = python -c "import sysconfig; print(sysconfig.get_path('scripts'))"
if (Test-Path $scripts) { $env:PATH = "$scripts;$env:PATH" }
if (-not $env:CC) { $env:CC = 'clang' }

Push-Location $here
try {
    python (Join-Path $here '..\wup-homebrew\libwup\tools\wupbuild.py') .
} finally {
    Pop-Location
}
