# Build and smoke-test guide

## Scope

These steps build the Sprint 1 branch and verify that the original benchmark still starts and that the optional distinguished-point exporter produces a readable file.

The CUDA assembler kernels `kernel_sm89.cubin` and `kernel_sm120.cubin` must be present beside the executable at runtime for RTX 40xx and RTX 50xx cards respectively.

## Linux prerequisites

- NVIDIA driver compatible with CUDA 12.8;
- CUDA Toolkit 12.8 installed at `/usr/local/cuda-12.8`, or pass another path to CMake;
- CMake 3.17 or newer;
- GCC/G++ supported by the installed CUDA Toolkit;
- Python 3.9 or newer for the inspection script.

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

The executable is created at:

```text
build/bin/rckangaroo
```

Copy the matching cubin files next to it:

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

The executable will normally be under:

```text
build/bin/Release/rckangaroo.exe
```

Copy the matching cubin files into that directory:

```powershell
Copy-Item kernel_sm89.cubin build/bin/Release/
Copy-Item kernel_sm120.cubin build/bin/Release/
```

## Baseline smoke test

Run a bounded benchmark so the process exits by itself:

Linux:

```bash
cd build/bin
./rckangaroo -range 32 -dp 14 -max 0.01
```

Windows:

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

## DP export smoke test

Linux:

```bash
./rckangaroo \
  -range 32 \
  -dp 14 \
  -max 0.05 \
  -worker 1 \
  -seed 1001 \
  -dpout worker-1.dp

python3 ../../tools/inspect_dp.py worker-1.dp
```

Windows:

```powershell
.\rckangaroo.exe `
  -range 32 `
  -dp 14 `
  -max 0.05 `
  -worker 1 `
  -seed 1001 `
  -dpout worker-1.dp

python ..\..\..\tools\inspect_dp.py worker-1.dp
```

Expected inspection output includes:

```text
format: RCKDP01
version: 1
worker_id: 1
seed: 1001
record_size: ...
records: greater than 0
```

A very short run can legitimately produce zero records. Increase `-max` or reduce `-dp` to 14.

## Two-worker metadata test

Run sequentially on one GPU for Sprint 1 validation:

```bash
./rckangaroo -range 32 -dp 14 -max 0.05 -worker 1 -seed 1001 -dpout worker-1.dp
./rckangaroo -range 32 -dp 14 -max 0.05 -worker 2 -seed 2002 -dpout worker-2.dp
python3 ../../tools/inspect_dp.py worker-1.dp
python3 ../../tools/inspect_dp.py worker-2.dp
```

Verify that the headers show different worker IDs and seeds. Sprint 1 does not yet merge these files or search cross-worker collisions.

## Regression check against `main`

Build `main` into a separate directory and run the same bounded benchmark:

```bash
git checkout main
cmake -S . -B build-main -DCMAKE_BUILD_TYPE=Release -DCUDAToolkit_ROOT=/usr/local/cuda-12.8
cmake --build build-main --parallel
cp kernel_sm89.cubin kernel_sm120.cubin build-main/bin/
./build-main/bin/rckangaroo -range 32 -dp 14 -max 0.01

git checkout feature/sprint1-dp-export
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
