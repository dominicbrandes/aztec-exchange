# test_exchange.ps1
$BaseUrl = "http://127.0.0.1:8000/api/v1"
$Headers = @{ "X-API-Key" = "test-key-1"; "Content-Type" = "application/json" }
$Timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()

Write-Host "=== Aztec Exchange Test (ID: $Timestamp) ===" -ForegroundColor Cyan

# 1. Health
Write-Host "`n[1/8] Health..." -ForegroundColor Yellow
$health = Invoke-RestMethod -Uri "$BaseUrl/health" -Method GET
Write-Host "  engine_connected: $($health.engine_connected)" -ForegroundColor Green

# 2. Book before
Write-Host "`n[2/8] Book before..." -ForegroundColor Yellow
$book = Invoke-RestMethod -Uri "$BaseUrl/book/BTC-USD" -Method GET
Write-Host "  Bids: $($book.bids.Count), Asks: $($book.asks.Count)" -ForegroundColor Green

# 3. Place SELL
Write-Host "`n[3/8] SELL 100 units..." -ForegroundColor Yellow
$sell = @{ account_id="seller"; symbol="BTC-USD"; side="SELL"; type="LIMIT"; price=1000000000000; quantity=10000000000; idempotency_key="sell-$Timestamp" } | ConvertTo-Json
$sellRes = Invoke-RestMethod -Uri "$BaseUrl/orders" -Method POST -Headers $Headers -Body $sell
$sellId = $sellRes.order.id
Write-Host "  Order ID: $sellId, Status: $($sellRes.order.status)" -ForegroundColor Green

# 4. Place BUY
Write-Host "`n[4/8] BUY 60 units (should match)..." -ForegroundColor Yellow
$buy = @{ account_id="buyer"; symbol="BTC-USD"; side="BUY"; type="LIMIT"; price=1000000000000; quantity=6000000000; idempotency_key="buy-$Timestamp" } | ConvertTo-Json
$buyRes = Invoke-RestMethod -Uri "$BaseUrl/orders" -Method POST -Headers $Headers -Body $buy
Write-Host "  Order ID: $($buyRes.order.id), Status: $($buyRes.order.status), Trades: $($buyRes.trades.Count)" -ForegroundColor Green

# 5. Book after
Write-Host "`n[5/8] Book after..." -ForegroundColor Yellow
$book2 = Invoke-RestMethod -Uri "$BaseUrl/book/BTC-USD" -Method GET
foreach ($ask in $book2.asks) { Write-Host "  Ask: price=$($ask.price), qty=$($ask.quantity)" -ForegroundColor Cyan }

# 6. Get sell order
Write-Host "`n[6/8] Get sell order $sellId..." -ForegroundColor Yellow
$order = Invoke-RestMethod -Uri "$BaseUrl/orders/$sellId" -Method GET -Headers $Headers
Write-Host "  remaining_qty: $($order.remaining_qty) (expect 4000000000)" -ForegroundColor Green

# 7. Cancel
Write-Host "`n[7/8] Cancel order $sellId..." -ForegroundColor Yellow
$cancel = Invoke-RestMethod -Uri "$BaseUrl/orders/$sellId" -Method DELETE -Headers $Headers
Write-Host "  Status: $($cancel.status)" -ForegroundColor Green

# 8. Stats
Write-Host "`n[8/8] Stats..." -ForegroundColor Yellow
$stats = Invoke-RestMethod -Uri "$BaseUrl/stats" -Method GET -Headers $Headers
Write-Host "  Orders: $($stats.total_orders), Trades: $($stats.total_trades), Cancels: $($stats.total_cancels)" -ForegroundColor Green

Write-Host "`n=== DONE ===" -ForegroundColor Cyan