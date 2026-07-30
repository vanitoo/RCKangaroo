#pragma once

#include <cstdio>
#include <cstdint>

#include "defs.h"

#pragma pack(push, 1)
struct DPExportHeader
{
    char magic[8];
    u16 version;
    u16 header_size;
    u32 record_size;
    u32 worker_id;
    u64 seed;
    u32 range_bits;
    u32 dp_bits;
};

struct DPExportRecord
{
    u32 worker_id;
    u64 sequence;
    u8 x[12];
    u8 distance[22];
    u8 type;
    u8 reserved[5];
};
#pragma pack(pop)

class DPExporter
{
public:
    DPExporter();
    ~DPExporter();

    bool Open(const char* file_name, u32 worker_id, u64 seed, u32 range_bits, u32 dp_bits);
    bool Write(const u8 x[12], const u8 distance[22], u8 type);
    void Close();
    bool IsOpen() const;
    u64 GetRecordCount() const;

private:
    FILE* file_;
    u32 worker_id_;
    u64 sequence_;
};
