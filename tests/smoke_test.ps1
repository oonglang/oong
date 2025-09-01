# Smoke test for oong interpreter
# Runs the Release build and checks that the output equals "42"
$exe = Join-Path -Path $PSScriptRoot -ChildPath "..\build\Release\oong.exe"
# If running from repo root, allow alternative path
if (-not (Test-Path $exe)) { $exe = Join-Path -Path $PSScriptRoot -ChildPath "..\build\Release\oong.exe" }
if (-not (Test-Path $exe)) { Write-Error "oong.exe not found at $exe"; exit 2 }
$example = Join-Path -Path $PSScriptRoot -ChildPath "..\example\test.oo"
if (-not (Test-Path $example)) { Write-Error "example/test.oo not found at $example"; exit 2 }

# Run the interpreter
$proc = & $exe $example 2>&1
$out = $proc -join "`n" | Out-String
$out = $out.Trim()
Write-Host "Interpreter output: '$out'"
if ($out -eq '42') {
    Write-Host "SMOKE TEST: PASS"
    exit 0
} else {
    Write-Error "SMOKE TEST: FAIL - expected '42'"
    exit 1
}
