# Sprint 1: distributed-mode design notes

## Goal

Prepare RCKangaroo for a future distributed worker/server mode without changing CUDA kernels or the local collision algorithm.

Sprint 1 adds:

- a documented map of the distinguished-point (DP) pipeline;
- deterministic worker metadata (`worker_id` and `seed`);
- optional export of normalized DP records to a binary file;
- build and smoke-test instructions.

The normal local solving path remains unchanged when DP export is not enabled.

## Current DP pipeline

```text
CUDA kernels
    |
    v
RCGpuKang::Execute()
    |
    v
AddPointsToList()
    - converts GPU kangaroo index to local type
    - appends fixed-size GPU records to the shared CPU buffer
    |
    v
CheckNewPoints()
    - normalizes the GPU record into DBRec
    - stores x[12], distance[22], type[1]
    - checks the local TFastBase table for collisions
    |
    v
Collision_SOTA()
    - reconstructs and verifies the private key candidate
```

The safest Sprint 1 export point is after `DBRec` has been normalized in `CheckNewPoints()` and before it is inserted into the local database. This keeps the export independent of GPU architecture and preserves the existing local collision path.

## Normalized local record

The upstream local database uses the packed record below:

```cpp
struct DBRec {
    uint8_t x[12];
    uint8_t d[22];
    uint8_t type;
};
```

`type` values:

- `0`: tame;
- `1`: wild;
- generation mode always exports tame records.

Sprint 1 wraps this record with file-level metadata. The binary file is append-only and intended for development and offline inspection. It is not yet the network protocol.

## Binary export format

Header, written once when a new file is opened:

```text
magic[8]      = "RCKDP01\0"
version       = 1
header_size   = sizeof(DPExportHeader)
record_size   = sizeof(DPExportRecord)
worker_id     = CLI worker id
seed          = CLI seed
range_bits    = selected range
 dp_bits      = selected DP value
```

Each record contains:

```text
worker_id
sequence
x[12]
distance[22]
type
reserved
```

All fields are written in the host byte order in Sprint 1. A later protocol version must define canonical little-endian encoding and stronger integrity checks before records are sent over a network.

## Determinism scope

The upstream code intentionally uses a constant random seed while generating jump tables so tame files stay compatible, then switches to a time-derived seed for later random behavior.

Sprint 1 adds explicit metadata:

- `-worker <uint32>`
- `-seed <uint64>`

The supplied seed is used in place of the time-derived seed after jump-table creation. This gives reproducible worker-side random initialization while preserving upstream-compatible jump tables.

Different workers should use distinct `(worker_id, seed)` pairs. Sprint 1 records these values but does not yet provide a central coordinator that guarantees uniqueness.

## Non-goals

Sprint 1 does not add:

- networking;
- a shared collision database;
- checkpoint/resume;
- signed work receipts;
- automatic task assignment;
- new CUDA kernels;
- changes to the mathematical method.

## Sprint 1 acceptance criteria

1. A build without `-dpout` behaves like upstream.
2. A run with `-dpout FILE` creates a header and appends normalized records.
3. `-worker` and `-seed` are visible in startup output and in the export header.
4. The export file can be inspected with `tools/inspect_dp.py`.
5. Two runs with different worker metadata produce files attributable to different workers.
