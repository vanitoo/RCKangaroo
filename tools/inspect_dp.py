#!/usr/bin/env python3
"""Inspect Sprint 1 RCKangaroo distinguished-point export files."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

HEADER = struct.Struct("<8sHHIIQII")
RECORD_PREFIX = struct.Struct("<IQ")
EXPECTED_MAGIC = b"RCKDP01\0"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path, help="DP export file")
    parser.add_argument("--show", type=int, default=3, help="show first N records")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    data = args.file.read_bytes()
    if len(data) < HEADER.size:
        raise SystemExit(f"file is too small: {len(data)} bytes")

    magic, version, header_size, record_size, worker_id, seed, range_bits, dp_bits = HEADER.unpack_from(data)
    if magic != EXPECTED_MAGIC:
        raise SystemExit(f"unexpected magic: {magic!r}")
    if header_size < HEADER.size:
        raise SystemExit(f"invalid header size: {header_size}")
    if record_size < 48:
        raise SystemExit(f"invalid record size: {record_size}")
    if len(data) < header_size:
        raise SystemExit("truncated header")

    payload_size = len(data) - header_size
    records, remainder = divmod(payload_size, record_size)

    print("format: RCKDP01")
    print(f"version: {version}")
    print(f"header_size: {header_size}")
    print(f"record_size: {record_size}")
    print(f"worker_id: {worker_id}")
    print(f"seed: {seed}")
    print(f"range_bits: {range_bits}")
    print(f"dp_bits: {dp_bits}")
    print(f"records: {records}")
    print(f"trailing_bytes: {remainder}")

    limit = min(max(args.show, 0), records)
    for index in range(limit):
        offset = header_size + index * record_size
        worker, sequence = RECORD_PREFIX.unpack_from(data, offset)
        x_start = offset + RECORD_PREFIX.size
        x = data[x_start : x_start + 12]
        distance = data[x_start + 12 : x_start + 34]
        point_type = data[x_start + 34]
        print(
            f"record[{index}]: worker={worker} sequence={sequence} "
            f"type={point_type} x={x.hex()} distance={distance.hex()}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
