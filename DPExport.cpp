#include "DPExport.h"

#include <cstring>

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
