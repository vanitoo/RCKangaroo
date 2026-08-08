#include "Ec.h"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <mutex>
#include <random>

namespace
{
constexpr int kMaxWorkerGpus = 32;
std::array<std::mt19937_64, kMaxWorkerGpus> g_worker_rng;
std::array<bool, kMaxWorkerGpus> g_worker_rng_initialized{};
std::mutex g_worker_rng_mutex;

struct WorkerSeedConfig
{
    bool enabled;
    u64 seed;
};

WorkerSeedConfig ReadWorkerSeed()
{
    const char* text = std::getenv("RCK_SEED");
    if (!text || !*text)
        return {false, 0};

    errno = 0;
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 0);
    if (errno || end == text || (end && *end != '\0'))
        return {false, 0};
    return {true, static_cast<u64>(value)};
}

const WorkerSeedConfig& GetWorkerSeedConfig()
{
    static const WorkerSeedConfig config = ReadWorkerSeed();
    return config;
}

u64 MixSeed(u64 x)
{
    // splitmix64 finalizer: deterministic separation between GPU streams.
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

void FillRandomBits(EcInt& value, int bits, std::mt19937_64& rng)
{
    value.SetZero();
    if (bits <= 0)
        return;
    if (bits > 256)
        bits = 256;

    const int limbs = (bits + 63) / 64;
    for (int i = 0; i < limbs; ++i)
        value.data[i] = rng();

    const int remainder = bits % 64;
    if (remainder)
        value.data[limbs - 1] &= (1ull << remainder) - 1;
    value.data[4] = 0;
}
}

void EcInt::RndMaxWithWorker(EcInt& max, int cuda_index)
{
    const WorkerSeedConfig& config = GetWorkerSeedConfig();
    if (!config.enabled || cuda_index < 0 || cuda_index >= kMaxWorkerGpus)
    {
        RndMax(max);
        return;
    }

    int top = 3;
    while (top >= 0 && !max.data[top])
        --top;
    if (top < 0)
    {
        SetZero();
        return;
    }

    u64 top_value = max.data[top];
    int leading_zeros = 0;
    while ((top_value & 0x8000000000000000ull) == 0)
    {
        top_value <<= 1;
        ++leading_zeros;
    }
    const int bits = 64 * top + (64 - leading_zeros);

    std::lock_guard<std::mutex> lock(g_worker_rng_mutex);
    if (!g_worker_rng_initialized[cuda_index])
    {
        const u64 stream_seed = MixSeed(config.seed ^ (0xD1B54A32D192ED03ull * static_cast<u64>(cuda_index + 1)));
        g_worker_rng[cuda_index].seed(stream_seed);
        g_worker_rng_initialized[cuda_index] = true;
    }

    do
    {
        FillRandomBits(*this, bits, g_worker_rng[cuda_index]);
    }
    while (!IsLessThanU(max));
}
