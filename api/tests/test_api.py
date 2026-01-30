"""API integration tests."""

import pytest
from httpx import AsyncClient


@pytest.mark.asyncio
async def test_root(client: AsyncClient):
    """Test root endpoint."""
    response = await client.get("/")
    assert response.status_code == 200
    data = response.json()
    assert data["service"] == "aztec-exchange"


@pytest.mark.asyncio
async def test_health(client: AsyncClient):
    """Test health endpoint."""
    response = await client.get("/api/v1/health")
    assert response.status_code == 200
    data = response.json()
    assert data["status"] in ["healthy", "degraded"]


@pytest.mark.asyncio
async def test_place_order_requires_auth(client: AsyncClient):
    """Test that order placement requires API key."""
    # Remove API key
    client.headers.pop("X-API-Key", None)

    response = await client.post(
        "/api/v1/orders",
        json={
            "account_id": "test",
            "symbol": "BTC-USD",
            "side": "BUY",
            "type": "LIMIT",
            "price": 5000000000000,
            "quantity": 100000000,
        },
    )
    assert response.status_code == 422  # Missing header


@pytest.mark.asyncio
async def test_get_book(client: AsyncClient):
    """Test order book endpoint."""
    response = await client.get("/api/v1/book/BTC-USD")
    assert response.status_code == 200
    data = response.json()
    assert "bids" in data
    assert "asks" in data