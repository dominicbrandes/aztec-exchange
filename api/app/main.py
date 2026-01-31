"""
Aztec Exchange API - Main Application Entry Point
"""

import time
from contextlib import asynccontextmanager
from typing import AsyncGenerator

from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

from app.config import settings
from app.engine_client import engine_client
from app.logging_config import logger, request_id_var, generate_request_id
from app.routes import router
from app.metrics import router as metrics_router, set_engine_connected, record_request_latency


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncGenerator[None, None]:
    """Manage application lifespan events."""
    logger.info("=" * 60)
    logger.info("Starting Aztec Exchange API")
    logger.info("=" * 60)
    
    try:
        await engine_client.start()
        set_engine_connected(True)
        logger.info("Engine started successfully")
    except Exception as e:
        set_engine_connected(False)
        logger.error(f"Failed to start engine: {e}")
        raise
    
    logger.info(f"API ready at http://{settings.HOST}:{settings.PORT}")
    logger.info(f"Swagger docs at http://{settings.HOST}:{settings.PORT}/docs")
    logger.info(f"Metrics at http://{settings.HOST}:{settings.PORT}/metrics")
    logger.info("=" * 60)
    
    yield
    
    logger.info("=" * 60)
    logger.info("Shutting down Aztec Exchange API")
    
    try:
        await engine_client.stop()
        set_engine_connected(False)
        logger.info("Engine stopped")
    except Exception as e:
        logger.error(f"Error stopping engine: {e}")
    
    logger.info("Shutdown complete")
    logger.info("=" * 60)


app = FastAPI(
    title="Aztec Exchange API",
    description="""
## High-Integrity Matching Engine REST API

A production-grade order matching engine with:
- **Price-Time Priority**: FIFO matching within price levels
- **Deterministic Execution**: Fixed-point arithmetic, reproducible results
- **Event Sourcing**: Full audit trail with replay capability
- **Risk Controls**: Order validation, size limits, self-trade prevention

### Authentication
Most endpoints require `X-API-Key` header. Market data endpoints are public.

### Fixed-Point Format
All prices and quantities use 8 decimal places (1e8 scale):
- `100000000` = 1.0
- `5000000000000` = 50,000.00
    """,
    version="0.1.0",
    lifespan=lifespan,
    docs_url="/docs",
    redoc_url="/redoc",
)


app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.middleware("http")
async def log_and_measure_requests(request: Request, call_next):
    """Middleware to log requests and record metrics."""
    req_id = generate_request_id()
    request_id_var.set(req_id)
    
    start_time = time.perf_counter()
    
    try:
        response = await call_next(request)
    except Exception as e:
        duration = time.perf_counter() - start_time
        logger.error("Request failed", extra={
            "method": request.method,
            "path": request.url.path,
            "duration_ms": round(duration * 1000, 2),
            "error": str(e),
            "request_id": req_id,
        })
        raise
    
    duration = time.perf_counter() - start_time
    
    if not request.url.path.endswith("/metrics"):
        endpoint = request.url.path.split("/")[-1] or "root"
        record_request_latency(request.method, endpoint, duration)
    
    logger.info("Request completed", extra={
        "method": request.method,
        "path": request.url.path,
        "status": response.status_code,
        "duration_ms": round(duration * 1000, 2),
        "request_id": req_id,
    })
    
    response.headers["X-Request-ID"] = req_id
    return response


@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception) -> JSONResponse:
    """Handle unexpected exceptions."""
    req_id = request_id_var.get() or "unknown"
    
    logger.error("Unhandled exception", extra={
        "error": str(exc),
        "error_type": type(exc).__name__,
        "request_id": req_id,
        "path": request.url.path,
    }, exc_info=True)
    
    return JSONResponse(
        status_code=500,
        content={
            "success": False,
            "error": {
                "code": "INTERNAL_ERROR",
                "message": "An internal error occurred",
            },
            "request_id": req_id,
        },
    )


app.include_router(router, prefix="/api/v1", tags=["Exchange"])
app.include_router(metrics_router, tags=["Monitoring"])


@app.get("/", tags=["Root"])
async def root():
    """Root endpoint."""
    return {
        "service": "aztec-exchange",
        "version": "0.1.0",
        "docs": "/docs",
        "health": "/api/v1/health",
        "metrics": "/metrics",
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("app.main:app", host=settings.HOST, port=settings.PORT, reload=True)