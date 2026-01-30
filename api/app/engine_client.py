"""Client for communicating with the C++ matching engine."""

import asyncio
import json
import subprocess
from pathlib import Path
from typing import Any
from uuid import uuid4

from app.config import settings
from app.logging_config import logger


class EngineClient:
    """Manages communication with the matching engine subprocess."""

    def __init__(self) -> None:
        self.process: subprocess.Popen | None = None
        self._lock = asyncio.Lock()
        self._started = False

    async def start(self) -> None:
        """Start the engine subprocess."""
        if self._started:
            return

        engine_path = Path(settings.ENGINE_PATH)
        if not engine_path.exists():
            raise RuntimeError(f"Engine binary not found at {engine_path}")

        # Create data directories
        Path(settings.DATA_DIR).mkdir(parents=True, exist_ok=True)
        Path(settings.SNAPSHOT_DIR).mkdir(parents=True, exist_ok=True)

        cmd = [
            str(engine_path),
            "--event-log",
            settings.EVENT_LOG_PATH,
            "--snapshot-dir",
            settings.SNAPSHOT_DIR,
        ]

        logger.info("Starting engine", extra={"cmd": cmd})

        self.process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            bufsize=1,
        )

        self._started = True
        logger.info("Engine started", extra={"pid": self.process.pid})

    async def stop(self) -> None:
        """Stop the engine subprocess.

        IMPORTANT: Uvicorn --reload triggers frequent shutdown/start cycles.
        On Windows the pipe can already be closed when we try to send 'shutdown',
        so we must never crash during teardown.
        """
        proc = self.process
        if not proc:
            self._started = False
            return

        try:
            # Only try graceful shutdown if the process is still alive AND stdin exists
            if proc.poll() is None and proc.stdin:
                try:
                    await self.send_command({"cmd": "shutdown", "req_id": str(uuid4())})
                except Exception as e:
                    logger.warning("Graceful engine shutdown failed (ignored)", extra={"error": str(e)})
        finally:
            # Always force terminate as a fallback
            try:
                if proc.poll() is None:
                    proc.terminate()
                    proc.wait(timeout=5)
            except Exception as e:
                logger.warning("Engine terminate/wait failed (ignored)", extra={"error": str(e)})

            self.process = None
            self._started = False
            logger.info("Engine stopped")

    async def send_command(self, command: dict[str, Any]) -> dict[str, Any]:
        """Send a command to the engine and get the response."""
        proc = self.process
        if not proc or not proc.stdin or not proc.stdout:
            raise RuntimeError("Engine not running")

        if proc.poll() is not None:
            raise RuntimeError("Engine process already exited")

        async with self._lock:
            cmd_json = json.dumps(command)
            logger.debug("Sending to engine", extra={"command": command})

            try:
                proc.stdin.write(cmd_json + "\n")
                proc.stdin.flush()
            except (BrokenPipeError, OSError, ValueError) as e:
                raise RuntimeError(f"Engine stdin write failed: {e}")

            response_line = proc.stdout.readline()
            if not response_line:
                raise RuntimeError("Engine closed connection (no response)")

            try:
                response = json.loads(response_line)
            except json.JSONDecodeError as e:
                raise RuntimeError(f"Engine returned invalid JSON: {e}; line={response_line!r}")

            logger.debug("Received from engine", extra={"response": response})
            return response

    async def place_order(self, order: dict[str, Any]) -> dict[str, Any]:
        return await self.send_command({"cmd": "place_order", "req_id": str(uuid4()), "order": order})

    async def cancel_order(self, order_id: int) -> dict[str, Any]:
        return await self.send_command({"cmd": "cancel_order", "req_id": str(uuid4()), "order_id": order_id})

    async def get_book(self, symbol: str, depth: int = 10) -> dict[str, Any]:
        return await self.send_command({"cmd": "get_book", "req_id": str(uuid4()), "symbol": symbol, "depth": depth})

    async def get_trades(self, symbol: str, limit: int = 100) -> dict[str, Any]:
        return await self.send_command({"cmd": "get_trades", "req_id": str(uuid4()), "symbol": symbol, "limit": limit})

    async def get_order(self, order_id: int) -> dict[str, Any]:
        return await self.send_command({"cmd": "get_order", "req_id": str(uuid4()), "order_id": order_id})

    async def get_stats(self) -> dict[str, Any]:
        return await self.send_command({"cmd": "get_stats", "req_id": str(uuid4())})

    async def health(self) -> dict[str, Any]:
        return await self.send_command({"cmd": "health", "req_id": str(uuid4())})


engine_client = EngineClient()
