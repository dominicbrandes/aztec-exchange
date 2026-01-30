# Architecture Overview

## System Design

### Matching Engine (C++)
The core matching engine is implemented in C++ for performance and determinism.

**Key Components:**
- `OrderBook`: Price-level aggregated order storage using `std::map`
- `MatchingEngine`: Order lifecycle, matching logic, event emission
- `EventLog`: Append-only JSONL file for durability
- `SnapshotManager`: Periodic state serialization

**Matching Algorithm:**
1. Incoming order validated (risk checks)
2. Order assigned ID and timestamp
3. Match against opposite side of book
4. Trades emitted for each fill
5. Remaining quantity rests on book (limit orders only)

**Price-Time Priority:**
- Orders sorted by price (best first)
- Within price level: FIFO by timestamp

### API Layer (Python/FastAPI)
Stateless REST interface that communicates with engine via subprocess.

**Components:**
- `engine_client.py`: Subprocess management, JSON protocol
- `routes.py`: HTTP endpoint handlers
- `auth.py`: API key validation
- `rate_limit.py`: Sliding window rate limiter

### Data Flow
```
Client Request
      │
      ▼
┌─────────────┐
│   FastAPI   │ ─── Validate, Rate Limit, Auth
└─────────────┘
      │
      ▼ JSON command
┌─────────────┐
│   Engine    │ ─── Match, Log Event
└─────────────┘
      │
      ▼ JSON response
┌─────────────┐
│   FastAPI   │ ─── Format Response
└─────────────┘
      │
      ▼
Client Response
```

## Invariants

1. **Book never crossed**: `best_bid < best_ask` always
2. **Quantity conservation**: `filled + remaining = original`
3. **Deterministic replay**: Same events → Same state
4. **Monotonic IDs**: Order/Trade IDs always increasing

## Recovery Process

1. Load latest snapshot (if exists)
2. Read events after snapshot sequence
3. Replay events to rebuild state
4. Resume normal operation

## Trade-offs

| Decision | Rationale |
|----------|-----------|
| Subprocess vs embedded | Isolation, crash safety, language flexibility |
| JSON vs binary protocol | Debuggability, simplicity over raw performance |
| In-memory book | Speed; durability via event log |
| Fixed-point arithmetic | Determinism, no floating-point errors |