"""API routes for the exchange."""

import time
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
from app.metrics import (
    record_order,
    record_order_rejected,
    record_trade,
    record_order_latency,
    set_engine_connected,
    set_book_depth,
)

router = APIRouter()


async def set_request_id(request: Request) -> str:
    req_id = generate_request_id()
    request_id_var.set(req_id)
    return req_id


# =============================================================================
# Order Endpoints
# =============================================================================

@router.post(
    "/orders",
    response_model=PlaceOrderResponse,
    summary="Place a new order",
    description="Submit a limit or market order to the matching engine.",
    responses={
        200: {"description": "Order accepted"},
        400: {"description": "Order rejected (validation or risk check failed)"},
        401: {"description": "Invalid API key"},
        429: {"description": "Rate limit exceeded"},
    },
)
async def place_order(
    order: PlaceOrderRequest,
    _api_key: str = Depends(verify_api_key),
    _rate_limit: None = Depends(rate_limit_dependency),
    req_id: str = Depends(set_request_id),
) -> PlaceOrderResponse:
    """Place a new order."""
    start_time = time.perf_counter()
    
    logger.info("Placing order", extra={
        "symbol": order.symbol,
        "side": order.side.value,
        "type": order.type.value,
        "price": order.price,
        "quantity": order.quantity,
    })

    order_dict = {
        "account_id": order.account_id,
        "symbol": order.symbol,
        "side": order.side.value,
        "type": order.type.value,
        "price": order.price,
        "quantity": order.quantity,
    }
    
    if order.idempotency_key is not None:
        order_dict["idempotency_key"] = order.idempotency_key
    if order.client_order_id is not None:
        order_dict["client_order_id"] = order.client_order_id

    result = await engine_client.place_order(order_dict)
    
    latency = time.perf_counter() - start_time
    record_order_latency(latency)

    if not result.get("success"):
        error = result.get("error", {})
        error_code = error.get("code", "UNKNOWN_ERROR")
        record_order_rejected(error_code)
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"{error_code}: {error.get('message', 'Order failed')}",
        )

    data = result["data"]
    order_response = Order(**data["order"])
    trades = [Trade(**t) for t in data.get("trades", [])]
    
    # Record metrics
    record_order(order.side.value, order.type.value, order_response.status.value)
    for trade in trades:
        record_trade(order.symbol, trade.quantity)
    
    return PlaceOrderResponse(order=order_response, trades=trades)


@router.get(
    "/orders/{order_id}",
    response_model=Order,
    summary="Get order details",
    description="Retrieve the current state of an order by its ID.",
    responses={
        200: {"description": "Order found"},
        404: {"description": "Order not found"},
        401: {"description": "Invalid API key"},
    },
)
async def get_order(
    order_id: int,
    _api_key: str = Depends(verify_api_key),
    req_id: str = Depends(set_request_id),
) -> Order:
    """Get order by ID."""
    result = await engine_client.get_order(order_id)
    
    if not result.get("success"):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Order not found"
        )
    
    return Order(**result["data"]["order"])


@router.delete(
    "/orders/{order_id}",
    response_model=Order,
    summary="Cancel an order",
    description="Cancel an open order. Only orders with status NEW or PARTIAL can be cancelled.",
    responses={
        200: {"description": "Order cancelled"},
        404: {"description": "Order not found or already filled/cancelled"},
        401: {"description": "Invalid API key"},
        429: {"description": "Rate limit exceeded"},
    },
)
async def cancel_order(
    order_id: int,
    _api_key: str = Depends(verify_api_key),
    _rate_limit: None = Depends(rate_limit_dependency),
    req_id: str = Depends(set_request_id),
) -> Order:
    """Cancel an order."""
    logger.info("Cancelling order", extra={"order_id": order_id})
    
    result = await engine_client.cancel_order(order_id)
    
    if not result.get("success"):
        error = result.get("error", {})
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=error.get("message", "Order not found or cannot be cancelled"),
        )
    
    return Order(**result["data"]["order"])


# =============================================================================
# Market Data Endpoints (Public)
# =============================================================================

@router.get(
    "/book/{symbol}",
    response_model=OrderBookResponse,
    summary="Get order book",
    description="Retrieve the current order book for a trading pair. No authentication required.",
    responses={
        200: {"description": "Order book snapshot"},
    },
)
async def get_book(
    symbol: str,
    depth: int = 10,
    req_id: str = Depends(set_request_id),
) -> OrderBookResponse:
    """Get order book (public endpoint)."""
    symbol = symbol.upper()
    result = await engine_client.get_book(symbol, depth)

    if not result.get("success"):
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to retrieve order book"
        )

    data = result["data"]
    bids = [BookLevel(**l) for l in data.get("bids", [])]
    asks = [BookLevel(**l) for l in data.get("asks", [])]
    
    # Update metrics
    set_book_depth(symbol, len(bids), len(asks))
    
    return OrderBookResponse(symbol=data["symbol"], bids=bids, asks=asks)


@router.get(
    "/trades/{symbol}",
    response_model=TradesResponse,
    summary="Get recent trades",
    description="Retrieve recent trades for a trading pair. No authentication required.",
    responses={
        200: {"description": "Recent trades"},
    },
)
async def get_trades(
    symbol: str,
    limit: int = 100,
    req_id: str = Depends(set_request_id),
) -> TradesResponse:
    """Get recent trades (public endpoint)."""
    symbol = symbol.upper()
    limit = min(limit, 1000)
    
    result = await engine_client.get_trades(symbol, limit)

    if not result.get("success"):
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to retrieve trades"
        )

    data = result["data"]
    return TradesResponse(
        symbol=data["symbol"],
        trades=[Trade(**t) for t in data.get("trades", [])],
    )


# =============================================================================
# System Endpoints
# =============================================================================

@router.get(
    "/stats",
    response_model=StatsResponse,
    summary="Get engine statistics",
    description="Retrieve performance statistics from the matching engine.",
    responses={
        200: {"description": "Engine statistics"},
        401: {"description": "Invalid API key"},
    },
)
async def get_stats(
    _api_key: str = Depends(verify_api_key),
    req_id: str = Depends(set_request_id),
) -> StatsResponse:
    """Get engine statistics."""
    result = await engine_client.get_stats()

    if not result.get("success"):
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Failed to retrieve statistics"
        )

    return StatsResponse(**result["data"])


@router.get(
    "/health",
    response_model=HealthResponse,
    summary="Health check",
    description="Check the health of the API and matching engine connection.",
    responses={
        200: {"description": "Service health status"},
    },
)
async def health_check() -> HealthResponse:
    """Health check (public endpoint)."""
    try:
        result = await engine_client.health()
        connected = result.get("success", False)
        set_engine_connected(connected)
        
        return HealthResponse(
            status="healthy" if connected else "degraded",
            timestamp_ns=result.get("data", {}).get("timestamp_ns", 0),
            engine_connected=connected,
        )
    except Exception as e:
        set_engine_connected(False)
        logger.warning(f"Health check failed: {e}")
        return HealthResponse(
            status="degraded",
            timestamp_ns=0,
            engine_connected=False,
        )