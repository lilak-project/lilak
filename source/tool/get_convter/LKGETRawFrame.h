#ifndef LKGETRAWFRAME_HH
#define LKGETRAWFRAME_HH

#include <cstddef>
#include <cstdint>
#include <vector>

struct LKGETRawFrame
{
    uint8_t metaType = 0;
    uint8_t dataSource = 0;
    uint16_t frameType = 0;
    uint8_t revision = 0;

    uint64_t blockSize = 1;
    uint64_t frameSizeBytes = 0;
    uint64_t headerSizeBytes = 8;
    uint64_t itemSizeBytes = 0;
    uint32_t itemCount = 0;

    uint64_t eventTime = 0;
    uint32_t eventIdx = 0;
    uint8_t coboIdx = 0;
    uint8_t asadIdx = 0;

    bool isBlob = false;
    bool isLayered = false;

    std::vector<uint8_t> bytes;
    std::vector<LKGETRawFrame> children;

    const uint8_t* Data() const { return bytes.data(); }
    const uint8_t* Payload() const { return bytes.data() + headerSizeBytes; }
    size_t PayloadSize() const { return bytes.size() > headerSizeBytes ? bytes.size() - headerSizeBytes : 0; }
};

#endif
