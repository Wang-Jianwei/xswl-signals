#requires -version 3.0

<#
.SYNOPSIS
    xswl-signals Windows PowerShell Build Script
.DESCRIPTION
    Convenience build script for Windows platforms
.EXAMPLE
    .\build.ps1
    .\build.ps1 build -DebugBuild
    .\build.ps1 rebuild
    .\build.ps1 test
#>

param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'clean', 'rebuild', 'test', 'install', 'help')]
    [string]$Command = 'build',
    [switch]$DebugBuild,
    [switch]$Install,
    [int]$Jobs = 0
)

Set-StrictMode -Version 2
$ErrorActionPreference = 'Continue'

$ScriptDir = Split-Path -Parent -Path $MyInvocation.MyCommand.Definition
$BuildDir = Join-Path $ScriptDir 'build'
$InstallPrefix = Join-Path $ScriptDir 'install'

function Write-Info($msg) { Write-Host "[INFO] $msg" -ForegroundColor Green }
function Write-Err($msg) { Write-Host "[ERROR] $msg" -ForegroundColor Red }
function Write-Warn($msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }

function Show-Help {
    @"
xswl-signals Build Script (Windows PowerShell)

Usage:
  .\build.ps1 [command] [options]

Commands:
  build       Build project (default)
  clean       Clean build directory
  rebuild     Clean and rebuild
  test        Run tests
  install     Install library
  help        Show this help

Options:
  -DebugBuild  Debug mode (default Release)
  -Install     Auto install after build
  -Jobs <n>    Parallel jobs (default CPU count)

Examples:
  .\build.ps1                              # Build
  .\build.ps1 build -DebugBuild            # Debug build
  .\build.ps1 rebuild                      # Clean rebuild
  .\build.ps1 test                         # Run tests
  .\build.ps1 build -Install               # Build and install
  .\build.ps1 build -DebugBuild -Jobs 4    # Debug with 4 jobs

"@ | Write-Host
}

function Setup-BuildDir {
    if (-not (Test-Path $BuildDir)) {
        Write-Info "Creating build directory: $BuildDir"
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }
}

function Get-JobCount {
    if ($Jobs -eq 0) {
        $processors = Get-CimInstance -ClassName Win32_Processor
        # 对于多核系统，取第一个处理器的逻辑处理器数
        if ($processors -is [array]) {
            return $processors[0].NumberOfLogicalProcessors
        }
        return $processors.NumberOfLogicalProcessors
    }
    else {
        return $Jobs
    }
}

function Detect-Generator {
    # 优先使用 Ninja（最快和最可靠）
    $ninja = Get-Command "ninja" -ErrorAction SilentlyContinue
    if ($ninja) {
        return "Ninja"
    }
    
    # 尝试 Visual Studio（如果可用）
    $vsWhere = Get-Command "vswhere.exe" -ErrorAction SilentlyContinue
    if ($vsWhere) {
        return "Visual Studio 16 2019"
    }
    
    # MinGW Makefiles（需要 mingw32-make）
    $make = Get-Command "mingw32-make" -ErrorAction SilentlyContinue
    if ($make) {
        return "MinGW Makefiles"
    }
    
    # Unix Makefiles（需要 make）
    $unixMake = Get-Command "make" -ErrorAction SilentlyContinue
    if ($unixMake) {
        return "Unix Makefiles"
    }
    
    # 默认使用 MinGW Makefiles
    return "MinGW Makefiles"
}

function Get-MakeProgram {
    param([string]$Generator)
    
    if ($Generator -eq "MinGW Makefiles") {
        $make = Get-Command "mingw32-make" -ErrorAction SilentlyContinue
        if ($make) { return $make.Source }
        $make = Get-Command "make" -ErrorAction SilentlyContinue
        if ($make) { return $make.Source }
    }
    
    if ($Generator -eq "Unix Makefiles") {
        $make = Get-Command "make" -ErrorAction SilentlyContinue
        if ($make) { return $make.Source }
    }
    
    if ($Generator -eq "Ninja") {
        return (Get-Command "ninja" -ErrorAction SilentlyContinue).Source
    }
    
    return ""
}

function Invoke-Build {
    param([bool]$DebugMode = $false, [bool]$InstallAfter = $false)
    
    $buildType = if ($DebugMode) { "Debug" } else { "Release" }
    $jobCount = Get-JobCount
    $generator = Detect-Generator
    $makeProgram = Get-MakeProgram $generator
    
    Setup-BuildDir
    Write-Info "Starting build (mode: $buildType, jobs: $jobCount, generator: $generator)"
    
    if ($makeProgram) {
        Write-Info "Using make program: $makeProgram"
    }
    
    Push-Location $BuildDir
    
    $cacheFile = Join-Path $BuildDir 'CMakeCache.txt'
    $needsCMake = -not (Test-Path $cacheFile)
    
    if ($needsCMake) {
        Write-Info "Running CMake..."
        
        $cmakeCmd = "cmake -G `"$generator`" -DCMAKE_BUILD_TYPE=`"$buildType`" -DCMAKE_INSTALL_PREFIX=`"$InstallPrefix`""
        
        if ($makeProgram) {
            $cmakeCmd += " -DCMAKE_MAKE_PROGRAM=`"$makeProgram`""
        }
        
        $cmakeCmd += " `"$ScriptDir`""
        
        Invoke-Expression $cmakeCmd
        if ($LASTEXITCODE -ne 0) {
            Pop-Location
            Write-Err "CMake configuration failed"
            return $false
        }
    }
    
    Write-Info "Building..."
    & cmake --build "." "--parallel=$jobCount"
    if ($LASTEXITCODE -ne 0) {
        Pop-Location
        Write-Err "Build failed"
        return $false
    }
    
    Write-Info "Build complete!"
    
    Pop-Location
    
    if ($InstallAfter) {
        return (Invoke-Install)
    }
    
    return $true
}

function Invoke-Clean {
    if (Test-Path $BuildDir) {
        Write-Info "Cleaning build directory..."
        Remove-Item -Path $BuildDir -Recurse -Force
        Write-Info "Clean complete!"
    }
    else {
        Write-Warn "Build directory not found"
    }
    return $true
}

function Invoke-Rebuild {
    param([bool]$DebugMode = $false, [bool]$InstallAfter = $false)
    
    Write-Info "Cleaning..."
    Invoke-Clean | Out-Null
    
    Write-Info "Building..."
    return (Invoke-Build -DebugMode $DebugMode -InstallAfter $InstallAfter)
}

function Invoke-Tests {
    $cacheFile = Join-Path $BuildDir 'CMakeCache.txt'
    if (-not (Test-Path $cacheFile)) {
        Write-Err "Project not built. Run: .\build.ps1 build"
        return $false
    }

    Write-Info "Running tests (via CTest)..."
    Push-Location $BuildDir

    & ctest --output-on-failure
    $testResult = $LASTEXITCODE

    Pop-Location

    if ($testResult -ne 0) {
        Write-Err "Tests failed"
        return $false
    }

    Write-Info "All tests passed"
    return $true
}

function Invoke-Install {
    $cacheFile = Join-Path $BuildDir 'CMakeCache.txt'
    if (-not (Test-Path $cacheFile)) {
        Write-Err "Project not built. Run: .\build.ps1 build"
        return $false
    }
    
    Write-Info "Installing to: $InstallPrefix"
    Push-Location $BuildDir
    
    & cmake --install .
    $installResult = $LASTEXITCODE
    
    Pop-Location
    
    if ($installResult -ne 0) {
        Write-Err "Installation failed"
        return $false
    }
    
    Write-Info "Installation complete!"
    return $true
}

# Main logic
if ($Command -eq 'help') {
    Show-Help
    exit 0
}

$success = $false
switch ($Command) {
    'build' {
        $success = Invoke-Build -DebugMode $DebugBuild -InstallAfter $Install
    }
    'clean' {
        $success = Invoke-Clean
    }
    'rebuild' {
        $success = Invoke-Rebuild -DebugMode $DebugBuild -InstallAfter $Install
    }
    'test' {
        $success = Invoke-Tests
    }
    'install' {
        $success = Invoke-Install
    }
    default {
        Write-Err "Unknown command: $Command"
        Show-Help
        exit 1
    }
}

if ($success) { exit 0 } else { exit 1 }
