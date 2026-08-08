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

// Sprint 1 integration hook. It is deliberately opt-in so the original
// execution path remains unchanged unless RCK_DP_OUT is set.
// Environment variables:
//   RCK_DP_OUT      output filename (required to enable export)
//   RCK_WORKER_ID   uint32 worker id (default 0)
//   RCK_SEED        uint64 metadata seed (default 0; trajectory seeding is separate)
//   RCK_RANGE       range bits stored in the header (default 0)
//   RCK_DP_BITS     DP bits stored in the header (default 0)
void DPExportMaybeWrite(const u8* db_record);
void DPExportShutdown();
u64 DPExportRecordCount();
