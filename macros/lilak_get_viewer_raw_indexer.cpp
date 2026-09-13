#include <cstdint>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "LKGETOfflineFrameIndex.h"

namespace {
uint64_t MTimeNs(const struct stat& status)
{
#if defined(__APPLE__)
    return uint64_t(status.st_mtimespec.tv_sec) * 1000000000ULL + status.st_mtimespec.tv_nsec;
#else
    return uint64_t(status.st_mtim.tv_sec) * 1000000000ULL + status.st_mtim.tv_nsec;
#endif
}

uint16_t ReadBE16(const unsigned char* p) { return (uint16_t(p[0]) << 8) | p[1]; }
uint16_t ReadLE16(const unsigned char* p) { return (uint16_t(p[1]) << 8) | p[0]; }
uint32_t ReadBE24(const unsigned char* p) { return (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2]; }
uint32_t ReadLE24(const unsigned char* p) { return (uint32_t(p[2]) << 16) | (uint32_t(p[1]) << 8) | p[0]; }
uint32_t ReadBE32(const unsigned char* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint32_t ReadLE32(const unsigned char* p) { return (uint32_t(p[3]) << 24) | (uint32_t(p[2]) << 16) | (uint32_t(p[1]) << 8) | p[0]; }

bool IsKnownBE(uint16_t type)
{
    return type == 0x0001 || type == 0x0002 || type == 0x0007
        || type == 0x0008 || type == 0xFF01 || type == 0xFF02;
}

uint16_t ResolveFrameType(const unsigned char* data)
{
    const auto typeBE = ReadBE16(data + 5);
    if (IsKnownBE(typeBE)) return typeBE;
    const auto typeLE = ReadLE16(data + 5);
    if (IsKnownBE(typeLE) || typeLE == 0xFF11) return typeLE;
    return typeBE;
}

bool IndexSource(const std::string& path, uint32_t sourceId, LKGETOfflineFrameIndex& index,
        LKGETOfflineFrameIndex::SourceRecord& sourceRecord,
        std::vector<LKGETOfflineFrameIndex::AuxiliaryFrameRef>& auxiliaryFrames)
{
    struct stat initialStatus = {};
    if (::stat(path.c_str(), &initialStatus) != 0) {
        std::cerr << "cannot stat source: " << path << '\n';
        return false;
    }
    sourceRecord = {static_cast<uint64_t>(initialStatus.st_size), MTimeNs(initialStatus)};
    std::ifstream input(path, std::ios::binary);
    if (!input) { std::cerr << "cannot open source: " << path << '\n'; return false; }
    input.seekg(0, std::ios::end);
    const auto endPosition = input.tellg();
    if (endPosition < 0) { std::cerr << "cannot determine source size: " << path << '\n'; return false; }
    const uint64_t fileSize = static_cast<uint64_t>(endPosition);
    input.seekg(0, std::ios::beg);

    uint64_t position = 0;
    uint64_t count = 0;
    uint32_t minimumEvent = std::numeric_limits<uint32_t>::max();
    uint32_t maximumEvent = 0;
    while (position < fileSize) {
        unsigned char header[28] = {};
        input.read(reinterpret_cast<char*>(header), sizeof(header));
        const auto bytesRead = input.gcount();
        if (bytesRead == 0) break;
        if (bytesRead < 8) {
            std::cerr << "incomplete final header in " << path << " at byte " << position << '\n';
            break;
        }
        const uint32_t blockPower = header[0] & 0x0F;
        const uint64_t blockSize = blockPower == 0 ? 1ULL : (1ULL << blockPower);
        const uint16_t frameType = ResolveFrameType(header);
        const uint64_t blocks = frameType == 0xFF11 ? ReadLE24(header + 1) : ReadBE24(header + 1);
        const uint64_t frameSize = blocks * blockSize;
        if (frameSize < 8 || frameSize > fileSize - position) {
            std::cerr << "incomplete or invalid final frame in " << path << " at byte " << position << '\n';
            break;
        }
        if ((frameType == 1 || frameType == 2) && bytesRead >= 26) {
            const uint32_t eventId = ReadBE32(header + 22);
            index.Add(sourceId, position, eventId);
            minimumEvent = std::min(minimumEvent, eventId);
            maximumEvent = std::max(maximumEvent, eventId);
            ++count;
        } else if (frameType == 0x0007 || frameType == 0x0008) {
            auxiliaryFrames.push_back({position, sourceId, frameType});
        } else if ((frameType == 0xFF01 || frameType == 0xFF02) && bytesRead >= 20) {
            const uint64_t headerBE = uint64_t(ReadBE16(header + 8)) * blockSize;
            const uint64_t headerLE = uint64_t(ReadLE16(header + 8)) * blockSize;
            const bool littleEndian = !(headerBE >= 20 && headerBE <= frameSize)
                && headerLE >= 20 && headerLE <= frameSize;
            const uint32_t eventId = littleEndian
                ? ReadLE32(header + 16) : ReadBE32(header + 16);
            index.Add(sourceId, position, eventId);
            minimumEvent = std::min(minimumEvent, eventId);
            maximumEvent = std::max(maximumEvent, eventId);
            ++count;
        }
        position += frameSize;
        input.clear();
        input.seekg(static_cast<std::streamoff>(position), std::ios::beg);
        if (!input) { std::cerr << "seek failed in " << path << " at byte " << position << '\n'; return false; }
    }
    struct stat finalStatus = {};
    if (::stat(path.c_str(), &finalStatus) != 0
            || static_cast<uint64_t>(finalStatus.st_size) != sourceRecord.fileSize
            || MTimeNs(finalStatus) != sourceRecord.modificationTimeNs) {
        std::cerr << "source changed while indexing: " << path << '\n';
        return false;
    }
    std::cerr << "indexed source " << sourceId << ": " << count << " frame(s)\n";
    std::cout << "LKGET_SOURCE " << sourceId << ' ' << count << ' ';
    if (count == 0)
        std::cout << "- -\n";
    else
        std::cout << minimumEvent << ' ' << maximumEvent << '\n';
    return true;
}
}

int main(int argc, char** argv)
{
    if (argc < 3) { std::cerr << "usage: " << argv[0] << " INDEX_FILE SOURCE...\n"; return 2; }
    LKGETOfflineFrameIndex index;
    std::vector<LKGETOfflineFrameIndex::SourceRecord> sources(argc - 2);
    std::vector<LKGETOfflineFrameIndex::AuxiliaryFrameRef> auxiliaryFrames;
    for (int source = 0; source < argc - 2; ++source)
        if (!IndexSource(argv[source + 2], static_cast<uint32_t>(source), index,
                    sources[source], auxiliaryFrames)) return 1;
    index.Sort();

    const auto& frames = index.Frames();
    uint64_t eventCount = 0;
    for (size_t first = 0; first < frames.size();) {
        ++eventCount;
        const auto eventId = frames[first].eventId;
        do { ++first; } while (first < frames.size() && frames[first].eventId == eventId);
    }
    if (!index.Write(argv[1], sources, auxiliaryFrames)) {
        std::cerr << "failed while writing index: " << argv[1] << '\n';
        return 1;
    }
    std::cerr << "wrote " << index.Size() << " frame reference(s), "
              << eventCount << " event(s), " << auxiliaryFrames.size()
              << " auxiliary frame reference(s)\n";
    return 0;
}
