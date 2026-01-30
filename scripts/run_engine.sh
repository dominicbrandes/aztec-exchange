#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

mkdir -p "$PROJECT_DIR/data/snapshots"

"$PROJECT_DIR/build/engine/exchange_engine" \
    --event-log "$PROJECT_DIR/data/events.jsonl" \
    --snapshot-dir "$PROJECT_DIR/data/snapshots"