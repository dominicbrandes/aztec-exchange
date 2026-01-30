# reset_data.ps1
# Clears the data directory to reset engine state
# Run this between test runs if you want a fresh start

$ProjectRoot = "C:\Users\brand\aztec-exchange"
$DataDir = "$ProjectRoot\data"

Write-Host "Resetting Aztec Exchange data..." -ForegroundColor Yellow

# Stop the API first if running (optional - warn user)
Write-Host "NOTE: Stop the API server before running this script!" -ForegroundColor Red
Write-Host ""

# Remove data files
if (Test-Path $DataDir) {
    Write-Host "Removing: $DataDir\events.jsonl" -ForegroundColor Gray
    Remove-Item "$DataDir\events.jsonl" -ErrorAction SilentlyContinue
    
    Write-Host "Removing: $DataDir\snapshots\*" -ForegroundColor Gray
    Remove-Item "$DataDir\snapshots\*" -ErrorAction SilentlyContinue
    
    Write-Host ""
    Write-Host "Data reset complete!" -ForegroundColor Green
    Write-Host "Restart the API to start fresh." -ForegroundColor Cyan
} else {
    Write-Host "Data directory does not exist: $DataDir" -ForegroundColor Yellow
    Write-Host "Nothing to reset." -ForegroundColor Gray
}