# ToriYomi Build and Test Script
# Usage: .\build.ps1

param(
    [string]$Config = "Release",
    [switch]$Clean,
    [switch]$Test,
    [string]$VcpkgRoot = "C:\vcpkg"
)

$ErrorActionPreference = "Stop"

Write-Host "=== ToriYomi Build Script ===" -ForegroundColor Cyan

# Clean build directory if requested
if ($Clean -and (Test-Path "build")) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build
}

# Create build directory
if (-not (Test-Path "build")) {
    Write-Host "Creating build directory..." -ForegroundColor Green
    New-Item -ItemType Directory -Path build | Out-Null
}

# Configure
Write-Host "`nConfiguring CMake..." -ForegroundColor Green
Set-Location build

$toolchainFile = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"

if (Test-Path $toolchainFile) {
    cmake .. -DCMAKE_TOOLCHAIN_FILE=$toolchainFile
} else {
    Write-Host "Warning: vcpkg toolchain not found at $toolchainFile" -ForegroundColor Yellow
    Write-Host "Trying to configure without vcpkg..." -ForegroundColor Yellow
    cmake ..
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

# Build
Write-Host "`nBuilding project ($Config)..." -ForegroundColor Green
cmake --build . --config $Config

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host "`nBuild successful!" -ForegroundColor Green

# Run tests if requested
if ($Test) {
    Write-Host "`nRunning tests..." -ForegroundColor Green
    ctest -C $Config --output-on-failure
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`nAll tests passed!" -ForegroundColor Green
    } else {
        Write-Host "`nSome tests failed!" -ForegroundColor Red
    }
}

Set-Location ..

Write-Host "`n=== Build Complete ===" -ForegroundColor Cyan
