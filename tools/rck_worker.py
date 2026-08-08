#!/usr/bin/env python3
"""Launch RCKangaroo as a reproducible Sprint 1 worker.

The upstream executable keeps its original CLI. This launcher adds worker
identity/export settings through environment variables and forwards everything
after `--` unchanged to RCKangaroo.
"""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True, help="path to rckangaroo executable")
    parser.add_argument("--worker", type=int, required=True, help="uint32 worker id")
    parser.add_argument("--seed", type=int, required=True, help="uint64 deterministic worker seed")
    parser.add_argument("--dpout", type=Path, required=True, help="output DP file")
    parser.add_argument("--range-meta", type=int, default=0, help="range bits stored in DP header")
    parser.add_argument("--dp-meta", type=int, default=0, help="DP bits stored in DP header")
    parser.add_argument("rck_args", nargs=argparse.REMAINDER, help="arguments passed to RCKangaroo after --")
    args = parser.parse_args()

    if not 0 <= args.worker <= 0xFFFFFFFF:
        parser.error("--worker must fit uint32")
    if not 0 <= args.seed <= 0xFFFFFFFFFFFFFFFF:
        parser.error("--seed must fit uint64")
    if args.rck_args and args.rck_args[0] == "--":
        args.rck_args = args.rck_args[1:]
    return args


def main() -> int:
    args = parse_args()
    exe = args.exe.resolve()
    if not exe.exists():
        raise SystemExit(f"executable not found: {exe}")

    env = os.environ.copy()
    env.update(
        {
            "RCK_DP_OUT": str(args.dpout.resolve()),
            "RCK_WORKER_ID": str(args.worker),
            "RCK_SEED": str(args.seed),
            "RCK_RANGE": str(args.range_meta),
            "RCK_DP_BITS": str(args.dp_meta),
        }
    )

    command = [str(exe), *args.rck_args]
    print(f"worker={args.worker} seed={args.seed} dpout={args.dpout}")
    print("exec:", " ".join(command))
    completed = subprocess.run(command, env=env, check=False)
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
