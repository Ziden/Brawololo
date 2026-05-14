param(
    [int]$Port = 8080,
    [switch]$SkipBuild,
    [switch]$NoBrowser,
    [string]$EmsdkRoot
)

$ErrorActionPreference = 'Stop'

function Get-EmsdkRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot,
        [string]$PreferredRoot
    )

    $candidates = @()

    if ($PreferredRoot) {
        $candidates += $PreferredRoot
    }

    if ($env:EMSDK) {
        $candidates += $env:EMSDK
    }

    $repoParent = Split-Path $RepositoryRoot -Parent
    if ($repoParent) {
        $candidates += (Join-Path $repoParent 'emsdk')
    }

    $candidates += @(
        (Join-Path $env:USERPROFILE 'emsdk'),
        'C:\emsdk',
        'C:\dev\emsdk',
        'C:\tools\emsdk'
    )

    foreach ($candidate in $candidates | Select-Object -Unique) {
        if (-not $candidate) {
            continue
        }

        $envScript = Join-Path $candidate 'emsdk_env.ps1'
        if (Test-Path $envScript) {
            return $candidate
        }
    }

    return $null
}

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed: $Command $($Arguments -join ' ')"
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$emsdkRoot = Get-EmsdkRoot -RepositoryRoot $repoRoot -PreferredRoot $EmsdkRoot

if (-not $emsdkRoot) {
    throw @"
EMSDK is not configured on this machine.

Install it with:
  git clone https://github.com/emscripten-core/emsdk.git "$env:USERPROFILE\emsdk"
  Set-Location "$env:USERPROFILE\emsdk"
  .\emsdk install latest
  .\emsdk activate latest

Then rerun this script. You can also set EMSDK permanently with:
  setx EMSDK "$env:USERPROFILE\emsdk"
"@
}

. (Join-Path $emsdkRoot 'emsdk_env.ps1') | Out-Null

Push-Location $repoRoot
try {
    if (-not $SkipBuild) {
        Invoke-Step -Command 'cmake' -Arguments @('--preset', 'emscripten-client')
        Invoke-Step -Command 'cmake' -Arguments @('--build', '--preset', 'emscripten-client', '--target', 'Client')
    }

    $outputDir = Join-Path $repoRoot 'build\emscripten-client\Client'
    $htmlPath = Join-Path $outputDir 'Client.html'
    if (-not (Test-Path $htmlPath)) {
        throw "Browser build output not found at $htmlPath. Run the build first or omit -SkipBuild."
    }

    Push-Location $outputDir
    try {
        $emrun = Get-Command emrun -ErrorAction SilentlyContinue
        if ($emrun) {
            $arguments = @('--port', $Port)
            if ($NoBrowser) {
                $arguments += '--no_browser'
            }
            $arguments += 'Client.html'
            Invoke-Step -Command $emrun.Source -Arguments $arguments
            return
        }

        $python = Get-Command py -ErrorAction SilentlyContinue
        if (-not $python) {
            $python = Get-Command python -ErrorAction SilentlyContinue
        }

        if (-not $python) {
            throw "Neither emrun nor Python was found. Install the EMSDK tools or Python to serve Client.html over HTTP."
        }

        Write-Host "Serving browser preview at http://127.0.0.1:$Port/Client.html"
        & $python.Source -m http.server $Port
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed: $($python.Source) -m http.server $Port"
        }
    }
    finally {
        Pop-Location
    }
}
finally {
    Pop-Location
}