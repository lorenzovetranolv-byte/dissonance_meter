# Increments BUILD_NUMBER in Source/BuildNumber.h on every build.
$headerPath = Join-Path $PSScriptRoot "..\..\Source\BuildNumber.h"
$headerPath = [System.IO.Path]::GetFullPath($headerPath)

$buildNumber = 0
if (Test-Path $headerPath) {
    $existing = Get-Content $headerPath -Raw
    if ($existing -match '#define\s+BUILD_NUMBER\s+(\d+)') {
        $buildNumber = [int]$Matches[1]
    }
}
$buildNumber++

$content = "// Auto-generated before each build by update_build_number.ps1 - do not edit manually.`r`n#pragma once`r`n#define BUILD_NUMBER $buildNumber`r`n"
Set-Content -Path $headerPath -Value $content -NoNewline
