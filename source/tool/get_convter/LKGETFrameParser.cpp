#include "LKGETFrameParser.h"

#include <vector>

uint16_t LKGETFrameParser::ReadBE16(const uint8_t* data)
{
    return (uint16_t(data[0]) << 8) | uint16_t(data[1]);
}

uint32_t LKGETFrameParser::ReadBE24(const uint8_t* data)
{
    return (uint32_t(data[0]) << 16) | (uint32_t(data[1]) << 8) | uint32_t(data[2]);
}

uint32_t LKGETFrameParser::ReadBE32(const uint8_t* data)
{
    return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | uint32_t(data[3]);
}

uint64_t LKGETFrameParser::ReadBE48(const uint8_t* data)
{
    uint64_t value = 0;
    for (int i = 0; i < 6; ++i)
        value = (value << 8) | uint64_t(data[i]);
    return value;
}

bool LKGETFrameParser::ReadNextFrame(std::istream& input, LKGETRawFrame& frame)
{
    uint8_t header[8];
    input.read(reinterpret_cast<char*>(header), 8);
    if (input.gcount() == 0)
        return false;
    if (input.gcount() != 8)
        return false;

    auto p2Block = uint8_t(header[0] & 0x0F);
    uint64_t blockSize = (p2Block == 0 ? 1ULL : (1ULL << p2Block));
    uint64_t frameSizeBytes = uint64_t(ReadBE24(header + 1)) * blockSize;
    if (frameSizeBytes < 8)
        return false;

    std::vector<uint8_t> bytes(frameSizeBytes);
    for (size_t i = 0; i < 8; ++i)
        bytes[i] = header[i];

    input.read(reinterpret_cast<char*>(bytes.data() + 8), frameSizeBytes - 8);
    if (size_t(input.gcount()) != frameSizeBytes - 8)
        return false;

    return ParseFrameBytes(bytes.data(), bytes.size(), frame);
}

bool LKGETFrameParser::ParseFrameBytes(const uint8_t* data, size_t size, LKGETRawFrame& frame) const
{
    if (size < 8)
        return false;

    frame = LKGETRawFrame();
    frame.bytes.assign(data, data + size);
    frame.metaType = data[0];
    frame.dataSource = data[4];
    frame.frameType = ReadBE16(data + 5);
    frame.revision = data[7];
    auto p2Block = uint8_t(data[0] & 0x0F);
    frame.blockSize = (p2Block == 0 ? 1ULL : (1ULL << p2Block));
    frame.frameSizeBytes = uint64_t(ReadBE24(data + 1)) * frame.blockSize;
    frame.isBlob = (frame.frameType == 0x7 || frame.frameType == 0x8);
    frame.isLayered = (frame.frameType == 65281 || frame.frameType == 65282);

    if (frame.frameSizeBytes != size)
        frame.frameSizeBytes = size;

    if (frame.isLayered) {
        if (size < 20)
            return false;
        frame.headerSizeBytes = uint64_t(ReadBE16(data + 8)) * frame.blockSize;
        frame.itemSizeBytes = ReadBE16(data + 10);
        frame.itemCount = ReadBE32(data + 12);
        frame.eventIdx = ReadBE32(data + 16);

        size_t offset = size_t(frame.headerSizeBytes);
        frame.children.reserve(frame.itemCount);
        for (uint32_t iChild = 0; iChild < frame.itemCount; ++iChild) {
            if (offset + 8 > size)
                return false;
            auto childBlockP2 = uint8_t(data[offset] & 0x0F);
            uint64_t childBlockSize = (childBlockP2 == 0 ? 1ULL : (1ULL << childBlockP2));
            uint64_t childFrameSize = uint64_t(ReadBE24(data + offset + 1)) * childBlockSize;
            if (offset + childFrameSize > size)
                return false;

            LKGETRawFrame child;
            if (!ParseFrameBytes(data + offset, size_t(childFrameSize), child))
                return false;
            frame.children.push_back(std::move(child));
            offset += size_t(childFrameSize);
        }
        return true;
    }

    if (frame.frameType == 1 || frame.frameType == 2) {
        if (size < 88)
            return false;
        frame.headerSizeBytes = uint64_t(ReadBE16(data + 8)) * frame.blockSize;
        frame.itemSizeBytes = ReadBE16(data + 10);
        frame.itemCount = ReadBE32(data + 12);
        frame.eventTime = ReadBE48(data + 16);
        frame.eventIdx = ReadBE32(data + 22);
        frame.coboIdx = data[26];
        frame.asadIdx = data[27];
        return true;
    }

    frame.headerSizeBytes = 8;
    return true;
}
