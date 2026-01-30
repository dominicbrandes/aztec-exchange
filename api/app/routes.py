"""API routes for the exchange."""

from fastapi import APIRouter, Depends, HTTPException, Request, status

from app.auth import verify_api_key
from app.engine_client import engine_client
from app.logging_config import generate_request_id, logger, request_id_var
from app.models import (
    BookLevel,
    HealthResponse,
    Order,
    OrderBookResponse,
    PlaceOrderRequest,
    PlaceOrderResponse,
    StatsResponse,
    Trade,
    TradesResponse,
)
from app.rate_limit import rate_limit_dependency

router = APIRouter()


async def set_request_id(request: Request) -> str:
    req_id = generate_request_id()
    request_id_var.set(req_id)
    return req_id


# ============================================================================
# Order Endpoints
# ============================================================================

@router.post("/orders", response_model=PlaceOrderResponse)
async def place_order(
    order: PlaceOrderRequest,
    _api_key: str = Depends(verify_api_key),
    _rate_limit: None = Depends(rate_limit_dependency),
    req_id: str = Depends(set_request_id),
) -> PlaceOrderResponse:
    """Place a new order."""
    logger.info("Placing order", extra={"symbol": order.symbol, "side": order.side.value})

    # Build order dict - ONLY include non-None values
    order_dict = {
        "account_id": order.account_id,
        "symbol": order.symbol,
        "side": order.side.value,
        "type": order.type.value,
        "price": order.price,
        "quantity": order.quantity,
    }
    
    # Only add optional fields if they have values (not None)
    if order.idempotency_key is not None:
        order_dict["idempotency_key"] = order.idempotency_key
    if order.client_order_id is not None:
        order_dict["client_order_id"] = order.client_order_id

    result = await engine_client.place_order(order_dict)

    if not result.get("success"):
        error = result.get("error", {})
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"{error.get('code', 'ERROR')}: {error.get('message', 'Order failed')}",
        )

    data = result["data"]
    return PlaceOrderResponse(
        order=Order(**data["order"]),
        trades=[Trade(**t) for t in data.get("trades", [])]
    )


@router.get("/orders/{order_id}", response_model=Order)
async def get_order(
    order_id: int,
    _api_key: str = Depends(verify_api_key),
) -> Order:
    result = await engine_client.get_order(order_id)
    if not result.get("success"):
        raise HTTPException(status_code=404, detail="Order not found")
    return Order(**result["data"]["order"])


@router.delete("/orders/{order_id}", response_model=Order)
async def cancel_order(
    order_id: int,
    _api_key: str = Depends(verify_api_key),
) -> Order:
    result = await engine_client.cancel_order(order_id)
    if not result.get("success"):
        raise HTTPException(status_code=404, detail="Order not found")
    return Order(**result["data"]["order"])

# ============================================================================
# Market Data Endpoints
# ============================================================================

@router.get("/book/{symbol}", response_model=OrderBookResponse)
async def get_book(
    symbol: str,
    depth: int = 10,
    req_id: str = Depends(set_request_id),
) -> OrderBookResponse:
    """Get order book (public)."""
    result = await engine_client.get_book(symbol.upper(), depth)

    if not result.get("success"):
        raise HTTPException(status_code=500, detail="Failed to get book")

    data = result["data"]
    return OrderBookResponse(
        symbol=data["symbol"],
        bids=[BookLevel(**l) for l in data.get("bids", [])],
        asks=[BookLevel(**l) for l in data.get("asks", [])],
    )


@router.get("/trades/{symbol}", response_model=TradesResponse)
async def get_trades(
    symbol: str,
    limit: int = 100,
    req_id: str = Depends(set_request_id),
) -> TradesResponse:
    """Get recent trades (public)."""
    result = await engine_client.get_trades(symbol.upper(), min(limit, 1000))

    if not result.get("success"):
        raise HTTPException(status_code=500, detail="Failed to get trades")

    data = result["data"]
    return TradesResponse(
        symbol=data["symbol"],
        trades=[Trade(**t) for t in data.get("trades", [])],
    )


# ============================================================================
# System Endpoints
# ============================================================================

@router.get("/stats", response_model=StatsResponse)
async def get_stats(
    _api_key: str = Depends(verify_api_key),
    req_id: str = Depends(set_request_id),
) -> StatsResponse:
    """Get engine statistics."""
    result = await engine_client.get_stats()

    if not result.get("success"):
        raise HTTPException(status_code=500, detail="Failed to get stats")

    return StatsResponse(**result["data"])


@router.get("/health", response_model=HealthResponse)
async def health_check() -> HealthResponse:
    """Health check (public)."""
    try:
        result = await engine_client.health()
        return HealthResponse(
            status="healthy",
            timestamp_ns=result.get("data", {}).get("timestamp_ns", 0),
            engine_connected=result.get("success", False),
        )
    except Exception:
        return HealthResponse(status="degraded", timestamp_ns=0, engine_connected=False)
