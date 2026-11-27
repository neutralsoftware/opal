[CmdletBinding(DefaultParameterSetName = 'Build')]
param(
    [Parameter(ParameterSetName = 'Build')]
    [ValidateSet('Vulkan', 'OpenGL')]
    [string]$Backend = 'Vulkan',

    [string]$Prefix,

    [Parameter(ParameterSetName = 'Build')]
    [string]$PackageDirectory,

    [Parameter(ParameterSetName = 'Build')]
    [switch]$NoRegister,

    [Parameter(Mandatory, ParameterSetName = 'Register')]
    [switch]$RegisterOnly
)

$ErrorActionPreference = 'Stop'

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)

    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program exited with code $LASTEXITCODE."
    }
}

if ($RegisterOnly) {
    if (-not $Prefix) {
        $Prefix = $PSScriptRoot
    }
} else {
    if (-not $Prefix) {
        $Prefix = Join-Path $env:LOCALAPPDATA "OpalSDK/$Backend"
    }
    $Prefix = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Prefix)
    $buildDirectory = Join-Path $PSScriptRoot "build/sdk-$Backend-x64"

    Invoke-Checked cmake @(
        '-S', $PSScriptRoot, '-B', $buildDirectory,
        '-A', 'x64', '-DOPAL_INSTALL=ON', "-DOPAL_BACKEND=$Backend",
        '-DCMAKE_INSTALL_LIBDIR=lib', "-DCMAKE_INSTALL_PREFIX=$Prefix"
    )
    Invoke-Checked cmake @('--build', $buildDirectory, '--config', 'Release', '--parallel')
    Invoke-Checked cmake @('--install', $buildDirectory, '--config', 'Release')

    if ($PackageDirectory) {
        $PackageDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($PackageDirectory)
        $cpackConfig = Join-Path $buildDirectory 'CPackConfig.cmake'
        Invoke-Checked cpack @('--config', $cpackConfig, '-G', 'ZIP', '-C', 'Release', '-B', $PackageDirectory)
    }
}

$config = Join-Path $Prefix 'lib/cmake/Opal/OpalConfig.cmake'
if (-not (Test-Path -LiteralPath $config -PathType Leaf)) {
    throw "OpalConfig.cmake was not found at '$config'."
}

$Prefix = (Get-Item -LiteralPath $Prefix).FullName
if (-not $NoRegister) {
    [Environment]::SetEnvironmentVariable('Opal_ROOT', $Prefix, 'User')
    $env:Opal_ROOT = $Prefix
    Write-Host "Opal_ROOT is set to '$Prefix' for this session and future sessions."
}
Write-Host "Opal SDK is ready at '$Prefix'."
