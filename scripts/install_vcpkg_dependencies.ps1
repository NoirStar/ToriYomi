[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$VcpkgRoot = $env:VCPKG_ROOT,

    [Parameter(Mandatory = $true)]
    [string[]]$Packages,

    [Parameter(Mandatory = $false)]
    [string]$Triplet = "x64-windows",

    [Parameter(Mandatory = $false)]
    [string]$Proxy = $null
)

function Resolve-VcpkgExecutable {
    param([string]$Root)

    if (-not $Root -or -not (Test-Path $Root)) {
        throw "Set -VcpkgRoot or VCPKG_ROOT to the folder that contains vcpkg.exe."
    }

    $exe = Join-Path $Root "vcpkg.exe"
    if (-not (Test-Path $exe)) {
        throw "Could not find vcpkg.exe under '$Root'."
    }

    return $exe
}

function Set-OptionalProxy {
    param([string]$Uri)

    if ([string]::IsNullOrWhiteSpace($Uri)) {
        return
    }

    $env:http_proxy = $Uri
    $env:https_proxy = $Uri
    $env:HTTP_PROXY = $Uri
    $env:HTTPS_PROXY = $Uri
}

function Initialize-StdFlags {
    # Force Abseil (and other ports) to configure with the expected language standard
    $env:VCPKG_CMAKE_CONFIGURE_OPTIONS = "-DCMAKE_CXX_STANDARD=20;-DTHREADS_PREFER_PTHREAD_FLAG=OFF"
    $env:VCPKG_CXX_FLAGS = "/std:c++20"
    $env:VCPKG_C_FLAGS = "/std:c17"
}

function Invoke-VcpkgInstall {
    param(
        [string]$Executable,
        [string[]]$TargetPackages,
        [string]$TripletName
    )

    if ($TargetPackages.Count -eq 0) {
        throw "Provide at least one package name via -Packages."
    }

    $arguments = @('install')
    foreach ($pkg in $TargetPackages) {
        $arguments += "$pkg`:$TripletName"
    }

    & $Executable @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg install failed with exit code $LASTEXITCODE."
    }
}

try {
    $vcpkgExe = Resolve-VcpkgExecutable -Root $VcpkgRoot
    Set-OptionalProxy -Uri $Proxy
    Initialize-StdFlags
    Invoke-VcpkgInstall -Executable $vcpkgExe -TargetPackages $Packages -TripletName $Triplet
    Write-Host "vcpkg dependencies installed successfully." -ForegroundColor Green
} catch {
    Write-Error $_.Exception.Message
    exit 1
}
