"""Structured logging configuration."""

import logging
import sys
from contextvars import ContextVar
from typing import Any
from uuid import uuid4

from pythonjsonlogger import jsonlogger

# Context variable for request ID
request_id_var: ContextVar[str] = ContextVar("request_id", default="")


class CustomJsonFormatter(jsonlogger.JsonFormatter):
    """Custom JSON formatter that includes request_id."""

    def add_fields(
        self,
        log_record: dict[str, Any],
        record: logging.LogRecord,
        message_dict: dict[str, Any],
    ) -> None:
        super().add_fields(log_record, record, message_dict)
        log_record["timestamp"] = self.formatTime(record)
        log_record["level"] = record.levelname
        log_record["logger"] = record.name

        req_id = request_id_var.get()
        if req_id:
            log_record["request_id"] = req_id


def setup_logging(level: int = logging.INFO) -> logging.Logger:
    """Configure structured JSON logging."""
    logger = logging.getLogger("aztec_exchange")
    logger.setLevel(level)

    # Remove existing handlers
    logger.handlers.clear()

    # JSON handler for stdout
    handler = logging.StreamHandler(sys.stdout)
    handler.setFormatter(
        CustomJsonFormatter("%(timestamp)s %(level)s %(name)s %(message)s")
    )
    logger.addHandler(handler)

    return logger


def generate_request_id() -> str:
    """Generate a unique request ID."""
    return str(uuid4())[:8]


logger = setup_logging()