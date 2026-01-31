# Aztec Exchange - Full Demo Script
# Run: .\scripts\demo.ps1

$BaseUrl = "http://127.0.0.1:8000/api/v1"
$Headers = @{ "X-API-Key" = "test-key-1"; "Content-Type" = "application/json" }
$RunId = Get-Random

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "       AZTEC EXCHANGE - LIVE DEMO          " -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# 1. Health Check
Write-Host "[1/10] Checking System Health..." -ForegroundColor Yellow
try {
    $health = Invoke-RestMethod "$BaseUrl/health"
    Write-Host "       Status: $($health.status)" -ForegroundColor Green
    Write-Host "       Engine Connected: $($health.engine_connected)" -ForegroundColor Green
    if (-not $health.engine_connected) {
        Write-Host "       ERROR: Engine not connected. Start the API first." -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "       ERROR: API not running. Start with:" -ForegroundColor Red
    Write-Host "       cd api; python -m uvicorn app.main:app --host 127.0.0.1 --port 8000" -ForegroundColor Gray
    exit 1
}
Write-Host ""

# 2. Show initial book
Write-Host "[2/10] Fetching Order Book (BTC-USD)..." -ForegroundColor Yellow
$book = Invoke-RestMethod "$BaseUrl/book/BTC-USD"
Write-Host "       Bids: $($book.bids.Count) levels" -ForegroundColor Green
Write-Host "       Asks: $($book.asks.Count) levels" -ForegroundColor Green
Write-Host ""

# 3. Place SELL order (market maker provides liquidity)
Write-Host "[3/10] Market Maker: Placing SELL 100 BTC @ 50,000 USD..." -ForegroundColor Yellow
$sellOrder = @{
    account_id = "market-maker-1"
    symbol = "BTC-USD"
    side = "SELL"
    type = "LIMIT"
    price = 5000000000000
    quantity = 10000000000
    idempotency_key = "demo-sell-$RunId"
} | ConvertTo-Json
$sellRes = Invoke-RestMethod "$BaseUrl/orders" -Method POST -Headers $Headers -Body $sellOrder
Write-Host "       Order ID: $($sellRes.order.id)" -ForegroundColor Green
Write-Host "       Status: $($sellRes.order.status)" -ForegroundColor Green
Write-Host "       Trades: $($sellRes.trades.Count) (no match yet - resting on book)" -ForegroundColor Cyan
$sellId = $sellRes.order.id
Write-Host ""

# 4. Place another SELL at different price
Write-Host "[4/10] Market Maker: Placing SELL 50 BTC @ 50,100 USD..." -ForegroundColor Yellow
$sellOrder2 = @{
    account_id = "market-maker-1"
    symbol = "BTC-USD"
    side = "SELL"
    type = "LIMIT"
    price = 5010000000000
    quantity = 5000000000
    idempotency_key = "demo-sell2-$RunId"
} | ConvertTo-Json
$sellRes2 = Invoke-RestMethod "$BaseUrl/orders" -Method POST -Headers $Headers -Body $sellOrder2
Write-Host "       Order ID: $($sellRes2.order.id), Status: $($sellRes2.order.status)" -ForegroundColor Green
Write-Host ""

# 5. Place BID order
Write-Host "[5/10] Market Maker: Placing BUY 75 BTC @ 49,900 USD..." -ForegroundColor Yellow
$bidOrder = @{
    account_id = "market-maker-2"
    symbol = "BTC-USD"
    side = "BUY"
    type = "LIMIT"
    price = 4990000000000
    quantity = 7500000000
    idempotency_key = "demo-bid-$RunId"
} | ConvertTo-Json
$bidRes = Invoke-RestMethod "$BaseUrl/orders" -Method POST -Headers $Headers -Body $bidOrder
Write-Host "       Order ID: $($bidRes.order.id), Status: $($bidRes.order.status)" -ForegroundColor Green
Write-Host ""

# 6. Show order book with spread
Write-Host "[6/10] Order Book (with spread)..." -ForegroundColor Yellow
$book = Invoke-RestMethod "$BaseUrl/book/BTC-USD"
Write-Host "       --- ASKS (sellers) ---" -ForegroundColor Red
foreach ($ask in $book.asks) {
    $price = $ask.price / 100000000
    $qty = $ask.quantity / 100000000
    Write-Host "       $qty BTC @ `$$price" -ForegroundColor Red
}
Write-Host "       --- SPREAD ---" -ForegroundColor Gray
Write-Host "       --- BIDS (buyers) ---" -ForegroundColor Green
foreach ($bid in $book.bids) {
    $price = $bid.price / 100000000
    $qty = $bid.quantity / 100000000
    Write-Host "       $qty BTC @ `$$price" -ForegroundColor Green
}
Write-Host ""

# 7. Aggressive BUY crosses the spread
Write-Host "[7/10] Taker: Aggressive BUY 60 BTC @ 50,000 (crosses spread!)..." -ForegroundColor Yellow
$aggressiveBuy = @{
    account_id = "taker-1"
    symbol = "BTC-USD"
    side = "BUY"
    type = "LIMIT"
    price = 5000000000000
    quantity = 6000000000
    idempotency_key = "demo-aggbuy-$RunId"
} | ConvertTo-Json
$buyRes = Invoke-RestMethod "$BaseUrl/orders" -Method POST -Headers $Headers -Body $aggressiveBuy
Write-Host "       Order ID: $($buyRes.order.id)" -ForegroundColor Green
Write-Host "       Status: $($buyRes.order.status)" -ForegroundColor Green
Write-Host "       Trades Executed: $($buyRes.trades.Count)" -ForegroundColor Cyan
if ($buyRes.trades.Count -gt 0) {
    foreach ($trade in $buyRes.trades) {
        $tprice = $trade.price / 100000000
        $tqty = $trade.quantity / 100000000
        Write-Host "         -> Trade: $tqty BTC @ `$$tprice" -ForegroundColor Cyan
    }
}
Write-Host ""

# 8. Show trades
Write-Host "[8/10] Recent Trades..." -ForegroundColor Yellow
$trades = Invoke-RestMethod "$BaseUrl/trades/BTC-USD"
if ($trades.trades.Count -eq 0) {
    Write-Host "       No trades yet" -ForegroundColor Gray
} else {
    foreach ($t in $trades.trades | Select-Object -Last 5) {
        $tprice = $t.price / 100000000
        $tqty = $t.quantity / 100000000
        Write-Host "       Trade #$($t.id): $tqty BTC @ `$$tprice" -ForegroundColor Cyan
    }
}
Write-Host ""

# 9. Check remaining order
Write-Host "[9/10] Checking Original Sell Order (partial fill)..." -ForegroundColor Yellow
$orderCheck = Invoke-RestMethod "$BaseUrl/orders/$sellId" -Headers $Headers
$origQty = $orderCheck.quantity / 100000000
$remQty = $orderCheck.remaining_qty / 100000000
Write-Host "       Order ID: $($orderCheck.id)" -ForegroundColor Green
Write-Host "       Original Qty: $origQty BTC" -ForegroundColor Green
Write-Host "       Remaining Qty: $remQty BTC" -ForegroundColor Green
Write-Host "       Status: $($orderCheck.status)" -ForegroundColor Green
Write-Host ""

# 10. Engine stats
Write-Host "[10/10] Engine Statistics..." -ForegroundColor Yellow
$stats = Invoke-RestMethod "$BaseUrl/stats" -Headers $Headers
Write-Host "       Total Orders Processed: $($stats.total_orders)" -ForegroundColor Green
Write-Host "       Total Trades Executed: $($stats.total_trades)" -ForegroundColor Green
Write-Host "       Total Cancellations: $($stats.total_cancels)" -ForegroundColor Green
Write-Host "       Total Rejections: $($stats.total_rejects)" -ForegroundColor Green
Write-Host ""

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "            DEMO COMPLETE                  " -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Try these next:" -ForegroundColor Gray
Write-Host "  - Open http://127.0.0.1:8000/docs for Swagger UI" -ForegroundColor Gray
Write-Host "  - Open http://127.0.0.1:8000/metrics for Prometheus metrics" -ForegroundColor Gray
Write-Host "  - Run this script again (idempotency keys auto-rotate)" -ForegroundColor Gray
Write-Host ""