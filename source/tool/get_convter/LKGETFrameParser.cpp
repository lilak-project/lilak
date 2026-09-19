#include "LKGETFrameParser.h"

#include <vector>

namespace {
uint16_t ReadLE16Local(const uint8_t* data)
{
    return (uint16_t(data[1]) << 8) | uint16_t(data[0]);
}

uint32_t ReadLE24Local(const uint8_t* data)
{
    return (uint32_t(data[2]) << 16) | (uint32_t(data[1]) << 8) | uint32_t(data[0]);
}

uint32_t ReadLE32Local(const uint8_t* data)
{
    return (uint32_t(data[3]) << 24) | (uint32_t(data[2]) << 16) | (uint32_t(data[1]) << 8) | uint32_t(data[0]);
}

bool LayeredHeaderIsLittleEndian(const uint8_t* data, uint64_t blockSize)
{
    struct Candidate {
        bool valid;
        uint64_t headerSize;
    };

    auto makeCandidate = [data, blockSize](bool littleEndian) {
        uint64_t frameSize = uint64_t(littleEndian ? ReadLE24Local(data + 1)
                                                  : ((uint32_t(data[1]) << 16)
                                                     | (uint32_t(data[2]) << 8)
                                                     | uint32_t(data[3]))) * blockSize;
        uint64_t headerSize = uint64_t(littleEndian ? ReadLE16Local(data + 8)
                                                    : ((uint16_t(data[8]) << 8)
                                                       | uint16_t(data[9]))) * blockSize;
        uint32_t itemCount = littleEndian ? ReadLE32Local(data + 12)
                                          : ((uint32_t(data[12]) << 24)
                                             | (uint32_t(data[13]) << 16)
                                             | (uint32_t(data[14]) << 8)
                                             | uint32_t(data[15]));
        bool valid = headerSize >= 20 && headerSize <= frameSize
                     && uint64_t(itemCount) <= (frameSize - headerSize) / 8;
        return Candidate{valid, headerSize};
    };

    auto big = makeCandidate(false);
    auto little = makeCandidate(true);
    if (big.valid != little.valid)
        return little.valid;
    if (big.valid && big.headerSize != little.headerSize)
        return little.headerSize < big.headerSize;
    return false;
}

uint64_t FrameSizeBytes(const uint8_t* data, uint16_t frameType, uint64_t blockSize)
{
    if (frameType == 0xFF11)
        return uint64_t(ReadLE24Local(data + 1)) * blockSize;
    if ((frameType == 0xFF01 || frameType == 0xFF02)
            && LayeredHeaderIsLittleEndian(data, blockSize))
        return uint64_t(ReadLE24Local(data + 1)) * blockSize;
    return uint64_t((uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | uint32_t(data[3])) * blockSize;
}

uint16_t ResolveFrameType(const uint8_t* data)
{
    auto typeBE = (uint16_t(data[0]) << 8) | uint16_t(data[1]);
    switch (typeBE) {
        case 0x0001:
        case 0x0002:
        case 0x0007:
        case 0x0008:
        case 0xFF01:
        case 0xFF02:
            return typeBE;
        default:
            break;
    }

    auto typeLE = ReadLE16Local(data);
    switch (typeLE) {
        case 0x0001:
        case 0x0002:
        case 0x0007:
        case 0x0008:
        case 0xFF01:
        case 0xFF02:
        case 0xFF11:
            return typeLE;
        default:
            return typeBE;
    }
}
}

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
    uint8_t header[20];
    input.read(reinterpret_cast<char*>(header), 8);
    if (input.gcount() == 0)
        return false;
    if (input.gcount() != 8)
        return false;

    auto p2Block = uint8_t(header[0] & 0x0F);
    uint64_t blockSize = (p2Block == 0 ? 1ULL : (1ULL << p2Block));
    auto frameType = ResolveFrameType(header + 5);
    size_t headerBytes = 8;
    if (frameType == 0xFF01 || frameType == 0xFF02) {
        input.read(reinterpret_cast<char*>(header + 8), 12);
        if (input.gcount() != 12)
            return false;
        headerBytes = 20;
    }
    uint64_t frameSizeBytes = FrameSizeBytes(header, frameType, blockSize);
    if (frameSizeBytes < headerBytes)
        return false;

    std::vector<uint8_t> bytes(frameSizeBytes);
    for (size_t i = 0; i < headerBytes; ++i)
        bytes[i] = header[i];

    input.read(reinterpret_cast<char*>(bytes.data() + headerBytes), frameSizeBytes - headerBytes);
    if (size_t(input.gcount()) != frameSizeBytes - headerBytes)
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
    frame.frameType = ResolveFrameType(data + 5);
    frame.revision = data[7];
    auto p2Block = uint8_t(data[0] & 0x0F);
    frame.blockSize = (p2Block == 0 ? 1ULL : (1ULL << p2Block));
    frame.isBlob = (frame.frameType == 0x7 || frame.frameType == 0x8 || frame.frameType == 0xFF11);
    frame.isLayered = (frame.frameType == 65281 || frame.frameType == 65282);
    if (frame.isLayered && size < 20)
        return false;
    frame.frameSizeBytes = FrameSizeBytes(data, frame.frameType, frame.blockSize);

    if (frame.frameSizeBytes != size)
        frame.frameSizeBytes = size;

    if (frame.isLayered) {
        auto headerSizeBE = uint64_t(ReadBE16(data + 8)) * frame.blockSize;
        auto headerSizeLE = uint64_t(ReadLE16Local(data + 8)) * frame.blockSize;
        bool useLittleEndianHeader = LayeredHeaderIsLittleEndian(data, frame.blockSize);

        if (useLittleEndianHeader) {
            frame.headerSizeBytes = headerSizeLE;
            frame.itemSizeBytes = ReadLE16Local(data + 10);
            frame.itemCount = ReadLE32Local(data + 12);
            frame.eventIdx = ReadLE32Local(data + 16);
        }
        else {
            frame.headerSizeBytes = headerSizeBE;
            frame.itemSizeBytes = ReadBE16(data + 10);
            frame.itemCount = ReadBE32(data + 12);
            frame.eventIdx = ReadBE32(data + 16);
        }

        size_t offset = size_t(frame.headerSizeBytes);
        frame.children.reserve(frame.itemCount);
        for (uint32_t iChild = 0; iChild < frame.itemCount; ++iChild) {
            if (offset + 8 > size)
                return false;
            auto childBlockP2 = uint8_t(data[offset] & 0x0F);
            uint64_t childBlockSize = (childBlockP2 == 0 ? 1ULL : (1ULL << childBlockP2));
            auto childFrameType = ResolveFrameType(data + offset + 5);
            if ((childFrameType == 0xFF01 || childFrameType == 0xFF02) && offset + 20 > size)
                return false;
            uint64_t childFrameSize = FrameSizeBytes(data + offset, childFrameType, childBlockSize);
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
