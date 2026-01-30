#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

export ENGINE_PATH="$PROJECT_DIR/build/engine/exchange_engine"
export DATA_DIR="$PROJECT_DIR/data"

cd "$PROJECT_DIR/api"
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload