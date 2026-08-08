#include "DPExport.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace
{
DPExporter g_exporter;
bool g_export_init_attempted = false;

u64 ParseU64Env(const char* name, u64 fallback)
{
    const char* value = std::getenv(name);
    if (!value || !*value)
        return fallback;

    errno = 0;
    char* end = nullptr;
    unsigned long long parsed = std::strtoull(value, &end, 0);
    if (errno || end == value || (end && *end != '\0'))
        return fallback;
    return static_cast<u64>(parsed);
}

u32 ParseU32Env(const char* name, u32 fallback)
{
    const u64 value = ParseU64Env(name, fallback);
    if (value > 0xFFFFFFFFull)
        return fallback;
    return static_cast<u32>(value);
}

void EnsureExporterInitialized()
{
    if (g_export_init_attempted)
        return;
    g_export_init_attempted = true;

    const char* file_name = std::getenv("RCK_DP_OUT");
    if (!file_name || !*file_name)
        return;

    const u32 worker_id = ParseU32Env("RCK_WORKER_ID", 0);
    const u64 seed = ParseU64Env("RCK_SEED", 0);
    const u32 range_bits = ParseU32Env("RCK_RANGE", 0);
    const u32 dp_bits = ParseU32Env("RCK_DP_BITS", 0);

    if (!g_exporter.Open(file_name, worker_id, seed, range_bits, dp_bits))
    {
        std::fprintf(stderr, "WARNING: cannot open DP export file '%s'\n", file_name);
        return;
    }

    std::printf("DP export enabled: %s (worker=%u, seed=%llu)\r\n",
        file_name,
        worker_id,
        static_cast<unsigned long long>(seed));
}
}

DPExporter::DPExporter()
    : file_(nullptr), worker_id_(0), sequence_(0)
{
}

DPExporter::~DPExporter()
{
    Close();
}

bool DPExporter::Open(const char* file_name, u32 worker_id, u64 seed, u32 range_bits, u32 dp_bits)
{
    Close();
    file_ = std::fopen(file_name, "wb");
    if (!file_)
        return false;

    DPExportHeader header{};
    const char magic[8] = {'R', 'C', 'K', 'D', 'P', '0', '1', '\0'};
    std::memcpy(header.magic, magic, sizeof(header.magic));
    header.version = 1;
    header.header_size = static_cast<u16>(sizeof(DPExportHeader));
    header.record_size = static_cast<u32>(sizeof(DPExportRecord));
    header.worker_id = worker_id;
    header.seed = seed;
    header.range_bits = range_bits;
    header.dp_bits = dp_bits;

    if (std::fwrite(&header, sizeof(header), 1, file_) != 1)
    {
        Close();
        return false;
    }

    worker_id_ = worker_id;
    sequence_ = 0;
    return true;
}

bool DPExporter::Write(const u8 x[12], const u8 distance[22], u8 type)
{
    if (!file_)
        return false;

    DPExportRecord record{};
    record.worker_id = worker_id_;
    record.sequence = sequence_++;
    std::memcpy(record.x, x, sizeof(record.x));
    std::memcpy(record.distance, distance, sizeof(record.distance));
    record.type = type;

    return std::fwrite(&record, sizeof(record), 1, file_) == 1;
}

void DPExporter::Close()
{
    if (file_)
    {
        std::fflush(file_);
        std::fclose(file_);
        file_ = nullptr;
    }
}

bool DPExporter::IsOpen() const
{
    return file_ != nullptr;
}

u64 DPExporter::GetRecordCount() const
{
    return sequence_;
}

void DPExportMaybeWrite(const u8* db_record)
{
    EnsureExporterInitialized();
    if (!g_exporter.IsOpen() || !db_record)
        return;

    // DBRec layout in RCKangaroo.cpp is packed:
    // x[12] | d[22] | type[1]
    if (!g_exporter.Write(db_record, db_record + 12, db_record[34]))
        std::fprintf(stderr, "WARNING: DP export write failed\n");
}

void DPExportShutdown()
{
    g_exporter.Close();
}

u64 DPExportRecordCount()
{
    return g_exporter.GetRecordCount();
}
