# Build and smoke-test guide

## Scope

These steps build the Sprint 1 branch and verify three things:

1. the original bounded benchmark still starts with no worker settings;
2. the optional distinguished-point exporter produces a readable file;
3. different worker seeds create deterministic, independent GPU start-distance streams without changing the original jump-table RNG.

The CUDA assembler kernels `kernel_sm89.cubin` and `kernel_sm120.cubin` must be present beside the executable at runtime for RTX 40xx and RTX 50xx cards respectively.

## Linux prerequisites

- NVIDIA driver compatible with CUDA 12.8;
- CUDA Toolkit 12.8 installed at `/usr/local/cuda-12.8`, or pass another path to CMake;
- CMake 3.17 or newer;
- GCC/G++ supported by the installed CUDA Toolkit;
- Python 3.9 or newer for the worker/inspection scripts.

Check the environment:

```bash
nvidia-smi
/usr/local/cuda-12.8/bin/nvcc --version
cmake --version
python3 --version
```

## Linux build

```bash
git clone https://github.com/vanitoo/RCKangaroo.git
cd RCKangaroo
git checkout feature/sprint1-dp-export

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCUDAToolkit_ROOT=/usr/local/cuda-12.8

cmake --build build --parallel
```

Executables are created under:

```text
build/bin/rckangaroo
build/bin/dp_export_smoketest
```

Copy the matching cubin files next to `rckangaroo`:

```bash
cp kernel_sm89.cubin build/bin/
cp kernel_sm120.cubin build/bin/
```

## Windows prerequisites

- NVIDIA driver;
- CUDA Toolkit 12.8;
- Visual Studio 2022 with Desktop development with C++;
- CMake available in `PATH`;
- Python 3.9 or newer.

## Windows build: Developer PowerShell

```powershell
git clone https://github.com/vanitoo/RCKangaroo.git
Set-Location RCKangaroo
git checkout feature/sprint1-dp-export

cmake -S . -B build `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DCUDAToolkit_ROOT="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8"

cmake --build build --config Release --parallel
```

The main executable will normally be under:

```text
build/bin/Release/rckangaroo.exe
```

Copy the cubin files into that directory:

```powershell
Copy-Item kernel_sm89.cubin build/bin/Release/
Copy-Item kernel_sm120.cubin build/bin/Release/
```

## CPU-only export-format test

This validates the Sprint 1 binary format independently from CUDA.

Linux after CMake build:

```bash
./build/bin/dp_export_smoketest
python3 tools/inspect_dp.py dp-export-smoke.bin --show 2
```

Or directly with GCC, even on a machine without CUDA:

```bash
g++ -std=c++20 -Wall -Wextra -Werror -I. \
  DPExport.cpp tools/dp_export_smoketest.cpp \
  -o dp_export_smoketest
./dp_export_smoketest
python3 tools/inspect_dp.py dp-export-smoke.bin --show 2
```

Expected key values:

```text
format: RCKDP01
worker_id: 7
seed: 4660
range_bits: 40
dp_bits: 14
records: 2
```

## Baseline GPU smoke test

Run a bounded benchmark with **no** `RCK_*` worker variables set. This exercises the original RNG path.

Linux:

```bash
cd build/bin
env -u RCK_DP_OUT -u RCK_WORKER_ID -u RCK_SEED -u RCK_RANGE -u RCK_DP_BITS \
  ./rckangaroo -range 32 -dp 14 -max 0.01
```

Windows PowerShell: open a fresh shell, or remove any previously set `RCK_*` environment variables, then run:

```powershell
Set-Location build/bin/Release
.\rckangaroo.exe -range 32 -dp 14 -max 0.01
```

Expected signs of a healthy baseline:

- CUDA device is detected;
- the correct GPU and compute capability are printed;
- a kernel file is loaded;
- `GPUs started...` appears;
- the bounded run stops with `Operations limit reached` or solves the generated point first;
- no `CUDA error`, `illegal memory access`, or `FATAL ERROR` appears.

## Worker DP export smoke test

The upstream RCKangaroo CLI is intentionally unchanged in Sprint 1. Use the worker launcher; arguments after `--` are passed directly to RCKangaroo.

Linux from repository root:

```bash
python3 tools/rck_worker.py \
  --exe build/bin/rckangaroo \
  --worker 1 \
  --seed 1001 \
  --dpout build/bin/worker-1.dp \
  --range-meta 32 \
  --dp-meta 14 \
  -- -range 32 -dp 14 -max 0.05

python3 tools/inspect_dp.py build/bin/worker-1.dp
```

Windows from repository root:

```powershell
python tools/rck_worker.py `
  --exe build/bin/Release/rckangaroo.exe `
  --worker 1 `
  --seed 1001 `
  --dpout build/bin/Release/worker-1.dp `
  --range-meta 32 `
  --dp-meta 14 `
  -- -range 32 -dp 14 -max 0.05

python tools/inspect_dp.py build/bin/Release/worker-1.dp
```

Expected startup includes:

```text
DP export enabled: ...worker-1.dp (worker=1, seed=1001)
```

Expected inspection includes:

```text
format: RCKDP01
version: 1
worker_id: 1
seed: 1001
range_bits: 32
dp_bits: 14
records: greater than 0
```

A very short run can legitimately produce zero records. Increase `-max` while keeping `-dp 14`.

## Direct environment-variable launch

The Python launcher is only convenience. The binary also understands the Sprint 1 configuration through environment variables.

Linux:

```bash
export RCK_DP_OUT="$PWD/worker-1.dp"
export RCK_WORKER_ID=1
export RCK_SEED=1001
export RCK_RANGE=32
export RCK_DP_BITS=14
./rckangaroo -range 32 -dp 14 -max 0.05
```

`RCK_SEED` is important: when present, GPU start/restart distances use deterministic worker-local RNG streams. The fixed RNG used to build the Kangaroo jump tables is not changed, preserving compatibility with the upstream algorithm and tame files.

## Two-worker independence test

Run sequentially on one GPU:

```bash
python3 tools/rck_worker.py --exe build/bin/rckangaroo --worker 1 --seed 1001 --dpout worker-1.dp --range-meta 32 --dp-meta 14 -- -range 32 -dp 14 -max 0.05
python3 tools/rck_worker.py --exe build/bin/rckangaroo --worker 2 --seed 2002 --dpout worker-2.dp --range-meta 32 --dp-meta 14 -- -range 32 -dp 14 -max 0.05
python3 tools/inspect_dp.py worker-1.dp --show 5
python3 tools/inspect_dp.py worker-2.dp --show 5
```

Verify:

- headers contain different worker IDs and seeds;
- early exported records are not identical between worker 1 and worker 2;
- both runs remain valid and produce no collision/CUDA errors.

Then verify reproducibility by repeating worker 1 with the same seed into another file:

```bash
python3 tools/rck_worker.py --exe build/bin/rckangaroo --worker 1 --seed 1001 --dpout worker-1-repeat.dp --range-meta 32 --dp-meta 14 -- -range 32 -dp 14 -max 0.05
python3 tools/inspect_dp.py worker-1-repeat.dp --show 5
```

For a fixed GPU model/count, range, DP and seed, the start-distance stream is deterministic. Full DP output may still differ if execution stops at a timing-dependent boundary (`-max`) or thread scheduling changes when multiple GPUs are active; compare a sufficiently early prefix rather than requiring whole-file equality.

## Regression check against `main`

Build `main` into a separate directory and run the same bounded benchmark:

```bash
git checkout main
cmake -S . -B build-main -DCMAKE_BUILD_TYPE=Release -DCUDAToolkit_ROOT=/usr/local/cuda-12.8
cmake --build build-main --parallel
cp kernel_sm89.cubin kernel_sm120.cubin build-main/bin/
./build-main/bin/rckangaroo -range 32 -dp 14 -max 0.01

git checkout feature/sprint1-dp-export
env -u RCK_DP_OUT -u RCK_WORKER_ID -u RCK_SEED -u RCK_RANGE -u RCK_DP_BITS \
  ./build/bin/rckangaroo -range 32 -dp 14 -max 0.01
```

Compare:

- GPU detection;
- kernel selection;
- absence of CUDA errors;
- successful bounded exit;
- approximate speed. Small benchmark variation is normal.

## Troubleshooting

### `kernel_sm89.cubin` or `kernel_sm120.cubin` cannot be loaded

Copy the cubin file beside the executable and run from that directory.

### CUDA compiler not found

Pass the actual Toolkit path:

```bash
cmake -S . -B build -DCUDAToolkit_ROOT=/usr/local/cuda-12.8
```

### Unsupported host compiler

Use a GCC version supported by your CUDA Toolkit, or configure `CMAKE_CUDA_HOST_COMPILER` explicitly.

### No supported GPUs detected

Check `nvidia-smi`, the driver installation, and whether the GPU compute capability is at least 6.0.

### Export file has zero records

Use `-dp 14` and increase `-max`, for example to `0.1`.

### Different workers produce identical headers

Make sure both `--worker` and `--seed` differ. `worker_id` is accounting metadata; `seed` is what changes deterministic GPU start-distance streams.
