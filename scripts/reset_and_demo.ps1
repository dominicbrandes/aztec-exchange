# Aztec Exchange - Reset and Demo
# This clears all data and runs a fresh demo
# Run: .\scripts\reset_and_demo.ps1

$ProjectRoot = Split-Path -Parent $PSScriptRoot

Write-Host ""
Write-Host "============================================" -ForegroundColor Yellow
Write-Host "       RESETTING AZTEC EXCHANGE            " -ForegroundColor Yellow
Write-Host "============================================" -ForegroundColor Yellow
Write-Host ""

# Stop any running API
Write-Host "[1/3] Stopping any running processes..." -ForegroundColor Yellow
Get-Process -Name "exchange_engine" -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process -Name "python" -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "*aztec*" } | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1
Write-Host "       Done" -ForegroundColor Green
Write-Host ""

# Clear data
Write-Host "[2/3] Clearing data files..." -ForegroundColor Yellow
$dataDir = Join-Path $ProjectRoot "data"
if (Test-Path "$dataDir\events.jsonl") {
    Remove-Item "$dataDir\events.jsonl" -Force
    Write-Host "       Removed events.jsonl" -ForegroundColor Gray
}
if (Test-Path "$dataDir\snapshots") {
    Remove-Item "$dataDir\snapshots\*" -Force -ErrorAction SilentlyContinue
    Write-Host "       Cleared snapshots/" -ForegroundColor Gray
}
Write-Host "       Done" -ForegroundColor Green
Write-Host ""

# Start API
Write-Host "[3/3] Starting API server..." -ForegroundColor Yellow
Write-Host "       Please start the API manually in another terminal:" -ForegroundColor Cyan
Write-Host ""
Write-Host "       cd $ProjectRoot\api" -ForegroundColor White
Write-Host "       .\.venv\Scripts\Activate.ps1" -ForegroundColor White
Write-Host "       python -m uvicorn app.main:app --host 127.0.0.1 --port 8000" -ForegroundColor White
Write-Host ""
Write-Host "       Then press Enter to continue with demo..." -ForegroundColor Yellow
Read-Host

# Run demo
Write-Host ""
Write-Host "Running demo script..." -ForegroundColor Yellow
& "$PSScriptRoot\demo.ps1"