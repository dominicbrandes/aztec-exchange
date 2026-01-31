"""Prometheus metrics for observability."""

from prometheus_client import Counter, Histogram, Gauge, generate_latest, CONTENT_TYPE_LATEST
from fastapi import APIRouter
from fastapi.responses import Response

router = APIRouter()

# =============================================================================
# Metrics Definitions
# =============================================================================

# Order metrics
ORDERS_TOTAL = Counter(
    'exchange_orders_total',
    'Total number of orders processed',
    ['side', 'type', 'status']
)

ORDERS_REJECTED = Counter(
    'exchange_orders_rejected_total',
    'Total number of rejected orders',
    ['reason']
)

# Trade metrics
TRADES_TOTAL = Counter(
    'exchange_trades_total',
    'Total number of trades executed'
)

TRADE_VOLUME = Counter(
    'exchange_trade_volume_total',
    'Total trade volume (in base units)',
    ['symbol']
)

# Latency metrics
ORDER_LATENCY = Histogram(
    'exchange_order_latency_seconds',
    'Order processing latency in seconds',
    buckets=[0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0]
)

REQUEST_LATENCY = Histogram(
    'exchange_request_latency_seconds',
    'HTTP request latency in seconds',
    ['method', 'endpoint'],
    buckets=[0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0]
)

# System metrics
ENGINE_CONNECTED = Gauge(
    'exchange_engine_connected',
    'Whether the matching engine is connected (1=yes, 0=no)'
)

BOOK_DEPTH = Gauge(
    'exchange_book_depth',
    'Number of price levels in the order book',
    ['symbol', 'side']
)

# =============================================================================
# Metrics Endpoint
# =============================================================================

@router.get("/metrics", include_in_schema=False)
async def metrics():
    """Prometheus metrics endpoint."""
    return Response(
        content=generate_latest(),
        media_type=CONTENT_TYPE_LATEST
    )


# =============================================================================
# Helper Functions
# =============================================================================

def record_order(side: str, order_type: str, status: str):
    """Record an order metric."""
    ORDERS_TOTAL.labels(side=side, type=order_type, status=status).inc()


def record_order_rejected(reason: str):
    """Record a rejected order."""
    ORDERS_REJECTED.labels(reason=reason).inc()


def record_trade(symbol: str, quantity: int):
    """Record a trade metric."""
    TRADES_TOTAL.inc()
    TRADE_VOLUME.labels(symbol=symbol).inc(quantity)


def record_order_latency(latency_seconds: float):
    """Record order processing latency."""
    ORDER_LATENCY.observe(latency_seconds)


def record_request_latency(method: str, endpoint: str, latency_seconds: float):
    """Record HTTP request latency."""
    REQUEST_LATENCY.labels(method=method, endpoint=endpoint).observe(latency_seconds)


def set_engine_connected(connected: bool):
    """Set engine connection status."""
    ENGINE_CONNECTED.set(1 if connected else 0)


def set_book_depth(symbol: str, bid_levels: int, ask_levels: int):
    """Set order book depth gauges."""
    BOOK_DEPTH.labels(symbol=symbol, side='bid').set(bid_levels)
    BOOK_DEPTH.labels(symbol=symbol, side='ask').set(ask_levels)