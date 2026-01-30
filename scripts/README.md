# Aztec Exchange

High-integrity matching engine with REST API, built for exchange-grade reliability.

## Features

- **Price-Time Priority Matching**: FIFO ordering within price levels
- **Fixed-Point Arithmetic**: Deterministic, exact decimal handling (1e8 scale)
- **Event Sourcing**: Full audit trail with replay capability
- **Snapshot Recovery**: Fast restart from periodic snapshots
- **Risk Controls**: Order validation, size limits, self-trade prevention
- **Rate Limiting**: Per-client request throttling

## Architecture
```
┌─────────────────┐     JSON/stdin/stdout     ┌──────────────────┐
│  FastAPI (Python) │ ◄──────────────────────► │  Engine (C++)    │
│  - REST API       │                          │  - Order Book    │
│  - Auth           │                          │  - Matching      │
│  - Rate Limiting  │                          │  - Event Log     │
└─────────────────┘                          └──────────────────┘
```

## Quick Start

### Prerequisites
- Python 3.10+
- CMake 3.16+
- C++17 compiler (MSVC, GCC, Clang)

### Build C++ Engine
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Debug
```

### Run Tests
```bash
# C++ tests
./build/engine/tests/Debug/exchange_tests.exe

# API tests
cd api && pytest
```

### Start API Server
```bash
cd api
python -m venv .venv
.venv\Scripts\Activate.ps1  # Windows
pip install -r requirements.txt
python -m uvicorn app.main:app --host 127.0.0.1 --port 8000
```

### API Endpoints

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/v1/health` | GET | No | Health check |
| `/api/v1/book/{symbol}` | GET | No | Order book |
| `/api/v1/trades/{symbol}` | GET | No | Recent trades |
| `/api/v1/orders` | POST | Yes | Place order |
| `/api/v1/orders/{id}` | GET | Yes | Get order |
| `/api/v1/orders/{id}` | DELETE | Yes | Cancel order |
| `/api/v1/stats` | GET | Yes | Engine stats |

### Example: Place Order
```bash
curl -X POST "http://127.0.0.1:8000/api/v1/orders" \
  -H "X-API-Key: test-key-1" \
  -H "Content-Type: application/json" \
  -d '{"account_id":"user1","symbol":"BTC-USD","side":"BUY","type":"LIMIT","price":5000000000000,"quantity":100000000}'
```

## Fixed-Point Format

All prices and quantities use 8 decimal places:
- `100000000` = 1.0
- `5000000000000` = 50,000.00

## Project Structure
```
aztec-exchange/
├── engine/                 # C++ matching engine
│   ├── include/exchange/   # Headers
│   ├── src/                # Implementation
│   └── tests/              # Catch2 tests
├── api/                    # Python FastAPI
│   └── app/                # Application code
├── data/                   # Runtime data
│   ├── events.jsonl        # Event log
│   └── snapshots/          # State snapshots
└── docs/                   # Documentation
```

## License

MIT