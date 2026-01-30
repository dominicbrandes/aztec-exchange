#!/usr/bin/env python3
"""Simple latency benchmark for the matching engine."""

import json
import subprocess
import sys
import time
from pathlib import Path
from statistics import mean, median, stdev


def run_benchmark(engine_path: str, num_orders: int = 1000) -> dict:
    """Run benchmark against the engine."""
    process = subprocess.Popen(
        [engine_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    latencies = []

    # Warm up
    for i in range(100):
        cmd = {
            "cmd": "place_order",
            "req_id": f"warmup-{i}",
            "order": {
                "account_id": f"trader{i % 10}",
                "symbol": "BTC-USD",
                "side": "SELL" if i % 2 == 0 else "BUY",
                "type": "LIMIT",
                "price": (10000 + i) * 100000000,
                "quantity": 100,
            },
        }
        process.stdin.write(json.dumps(cmd) + "\n")
        process.stdin.flush()
        process.stdout.readline()

    # Benchmark
    for i in range(num_orders):
        cmd = {
            "cmd": "place_order",
            "req_id": f"bench-{i}",
            "order": {
                "account_id": f"trader{i % 10}",
                "symbol": "BTC-USD",
                "side": "SELL" if i % 2 == 0 else "BUY",
                "type": "LIMIT",
                "price": (10000 + (i % 100)) * 100000000,
                "quantity": 100,
            },
        }

        start = time.perf_counter_ns()
        process.stdin.write(json.dumps(cmd) + "\n")
        process.stdin.flush()
        process.stdout.readline()
        end = time.perf_counter_ns()

        latencies.append((end - start) / 1000)  # Convert to microseconds

    # Shutdown
    process.stdin.write(json.dumps({"cmd": "shutdown", "req_id": "end"}) + "\n")
    process.stdin.flush()
    process.terminate()

    return {
        "num_orders": num_orders,
        "mean_us": mean(latencies),
        "median_us": median(latencies),
        "stdev_us": stdev(latencies) if len(latencies) > 1 else 0,
        "min_us": min(latencies),
        "max_us": max(latencies),
        "p50_us": sorted(latencies)[int(num_orders * 0.5)],
        "p95_us": sorted(latencies)[int(num_orders * 0.95)],
        "p99_us": sorted(latencies)[int(num_orders * 0.99)],
        "throughput_ops": num_orders / (sum(latencies) / 1_000_000),
    }


def main():
    project_dir = Path(__file__).parent.parent
    engine_path = project_dir / "build" / "engine" / "exchange_engine"

    if not engine_path.exists():
        print(f"Engine not found at {engine_path}. Run 'make build' first.")
        sys.exit(1)

    print("Running benchmark...")
    results = run_benchmark(str(engine_path), num_orders=10000)

    print("\n=== Benchmark Results ===")
    print(f"Orders:      {results['num_orders']}")
    print(f"Mean:        {results['mean_us']:.2f} µs")
    print(f"Median:      {results['median_us']:.2f} µs")
    print(f"Std Dev:     {results['stdev_us']:.2f} µs")
    print(f"Min:         {results['min_us']:.2f} µs")
    print(f"Max:         {results['max_us']:.2f} µs")
    print(f"P50:         {results['p50_us']:.2f} µs")
    print(f"P95:         {results['p95_us']:.2f} µs")
    print(f"P99:         {results['p99_us']:.2f} µs")
    print(f"Throughput:  {results['throughput_ops']:.0f} ops/sec")


if __name__ == "__main__":
    main()