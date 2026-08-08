#include "DPExport.h"

#include <cstdio>
#include <cstring>

int main()
{
    DPExporter exporter;
    if (!exporter.Open("dp-export-smoke.bin", 7, 0x1234, 40, 14))
    {
        std::fprintf(stderr, "open failed\n");
        return 1;
    }

    u8 x[12]{};
    u8 distance[22]{};
    for (int i = 0; i < 12; ++i)
        x[i] = static_cast<u8>(i + 1);
    for (int i = 0; i < 22; ++i)
        distance[i] = static_cast<u8>(0xA0 + i);

    if (!exporter.Write(x, distance, TAME))
        return 2;
    x[0] ^= 0x55;
    if (!exporter.Write(x, distance, WILD))
        return 3;

    exporter.Close();
    std::printf("records=%llu\n", static_cast<unsigned long long>(exporter.GetRecordCount()));
    return exporter.GetRecordCount() == 2 ? 0 : 4;
}
