"""Simple in-memory rate limiter."""

import time
from collections import defaultdict
from dataclasses import dataclass
from typing import Callable

from fastapi import HTTPException, Request, status

from app.config import settings


@dataclass
class RateLimitState:
    """Track rate limit state for a client."""

    requests: list[float]


class RateLimiter:
    """Token bucket rate limiter."""

    def __init__(
        self,
        max_requests: int = settings.RATE_LIMIT_REQUESTS,
        window_seconds: int = settings.RATE_LIMIT_WINDOW_SECONDS,
    ) -> None:
        self.max_requests = max_requests
        self.window_seconds = window_seconds
        self._clients: dict[str, RateLimitState] = defaultdict(
            lambda: RateLimitState(requests=[])
        )

    def _get_client_key(self, request: Request) -> str:
        """Extract client identifier from request."""
        # Use API key if present, otherwise IP
        api_key = request.headers.get(settings.API_KEY_HEADER, "")
        if api_key:
            return f"key:{api_key}"
        return f"ip:{request.client.host if request.client else 'unknown'}"

    def check(self, request: Request) -> bool:
        """Check if request is within rate limit."""
        client_key = self._get_client_key(request)
        state = self._clients[client_key]
        now = time.time()

        # Remove old requests outside window
        cutoff = now - self.window_seconds
        state.requests = [t for t in state.requests if t > cutoff]

        # Check limit
        if len(state.requests) >= self.max_requests:
            return False

        # Record this request
        state.requests.append(now)
        return True

    def remaining(self, request: Request) -> int:
        """Get remaining requests for client."""
        client_key = self._get_client_key(request)
        state = self._clients[client_key]
        now = time.time()
        cutoff = now - self.window_seconds
        current_count = sum(1 for t in state.requests if t > cutoff)
        return max(0, self.max_requests - current_count)


rate_limiter = RateLimiter()


def rate_limit_dependency(request: Request) -> None:
    """FastAPI dependency for rate limiting."""
    if not rate_limiter.check(request):
        raise HTTPException(
            status_code=status.HTTP_429_TOO_MANY_REQUESTS,
            detail="Rate limit exceeded",
            headers={"Retry-After": str(settings.RATE_LIMIT_WINDOW_SECONDS)},
        )