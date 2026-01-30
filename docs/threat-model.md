# Threat Model

## Assets
- Order book state
- Event log (audit trail)
- API keys
- Account balances (future)

## Threat Actors
1. **Malicious Client**: Attempts to manipulate markets or DoS
2. **Insider**: Has API key, attempts unauthorized actions
3. **Network Attacker**: MITM, replay attacks

## Threats & Mitigations

### T1: Order Flooding (DoS)
**Risk**: Attacker sends many orders to overwhelm engine
**Mitigation**: 
- Rate limiting (100 req/min per key)
- Order size limits in risk checks

### T2: Self-Trade Manipulation
**Risk**: Wash trading to manipulate prices
**Mitigation**: 
- Self-trade prevention (orders from same account don't match)
- Rejected with `SELF_TRADE_PREVENTED` error

### T3: Duplicate Order Submission
**Risk**: Network retry causes double execution
**Mitigation**: 
- Idempotency keys tracked
- Duplicate rejected with `DUPLICATE_IDEMPOTENCY_KEY`

### T4: Price Manipulation via Crossed Book
**Risk**: Attacker creates crossed book state
**Mitigation**: 
- Matching loop runs until book uncrossed
- Invariant checked in fuzz tests

### T5: Replay Attack
**Risk**: Attacker replays old valid requests
**Mitigation**: 
- Idempotency keys prevent duplicate orders
- Future: Add timestamp/nonce validation

### T6: Unauthorized Access
**Risk**: Access without valid API key
**Mitigation**: 
- API key required for order operations
- Keys validated on every request

### T7: Data Loss
**Risk**: Engine crash loses orders
**Mitigation**: 
- Event sourcing: all state changes logged before execution
- Recovery replays events to rebuild state
- Periodic snapshots for faster recovery

## Future Improvements
- [ ] TLS encryption for API
- [ ] API key rotation
- [ ] Per-account rate limits
- [ ] Request signing (HMAC)
- [ ] IP allowlisting