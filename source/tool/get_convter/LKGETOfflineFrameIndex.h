#ifndef LKGETOFFLINEFRAMEINDEX_HH
#define LKGETOFFLINEFRAMEINDEX_HH

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

/**
 * Compact sortable index for offline GET frames.
 *
 * Only the source number, byte offset, and event identifier are retained.
 * The waveform payload remains in the input file and is read again when the
 * corresponding event is requested.
 */
class LKGETOfflineFrameIndex
{
  public:
    struct FileHeader
    {
        char magic[8];
        uint32_t version;
        uint32_t sourceCount;
        uint64_t frameCount;
        uint64_t eventCount;
        uint64_t auxiliaryFrameCount;
        uint64_t sourceTableOffset;
        uint64_t frameTableOffset;
        uint64_t eventTableOffset;
        uint64_t auxiliaryTableOffset;
    };

    struct SourceRecord
    {
        uint64_t fileSize;
        uint64_t modificationTimeNs;
    };

    struct FrameRef
    {
        FrameRef() = default;
        FrameRef(uint64_t position, uint32_t event, uint32_t source)
        : filePosition(position), eventId(event), sourceId(source) {}

        uint64_t filePosition;
        uint32_t eventId;
        uint32_t sourceId;
    };

    struct EventRef
    {
        uint64_t firstFrameIndex;
        uint32_t eventId;
        uint32_t frameCount;
    };

    struct AuxiliaryFrameRef
    {
        uint64_t filePosition;
        uint32_t sourceId;
        uint32_t frameType;
    };

    void Add(uint32_t sourceId, uint64_t filePosition, uint32_t eventId)
    {
        fFrames.emplace_back(filePosition, eventId, sourceId);
    }

    void Sort()
    {
        std::sort(fFrames.begin(), fFrames.end(),
                [](const FrameRef& lhs, const FrameRef& rhs) {
                    if (lhs.eventId != rhs.eventId)
                        return lhs.eventId < rhs.eventId;
                    if (lhs.sourceId != rhs.sourceId)
                        return lhs.sourceId < rhs.sourceId;
                    return lhs.filePosition < rhs.filePosition;
                });
    }

    const std::vector<FrameRef>& Frames() const { return fFrames; }
    std::vector<FrameRef>& Frames() { return fFrames; }
    size_t Size() const { return fFrames.size(); }
    size_t CapacityBytes() const { return fFrames.capacity() * sizeof(FrameRef); }
    uint64_t EventCount() const
    {
        uint64_t count = 0;
        for (size_t first = 0; first < fFrames.size();) {
            ++count;
            const auto eventId = fFrames[first].eventId;
            do { ++first; }
            while (first < fFrames.size() && fFrames[first].eventId == eventId);
        }
        return count;
    }

    bool Write(const std::string& path, const std::vector<SourceRecord>& sources,
            const std::vector<AuxiliaryFrameRef>& auxiliaryFrames) const
    {
        FileHeader header = {};
        std::memcpy(header.magic, "LKGVIX2", 7);
        header.version = 2;
        header.sourceCount = sources.size();
        header.frameCount = fFrames.size();
        header.auxiliaryFrameCount = auxiliaryFrames.size();
        header.sourceTableOffset = sizeof(FileHeader);
        header.frameTableOffset = header.sourceTableOffset
            + sources.size() * sizeof(SourceRecord);
        header.eventTableOffset = header.frameTableOffset
            + header.frameCount * sizeof(FrameRef);
        header.eventCount = EventCount();
        header.auxiliaryTableOffset = header.eventTableOffset
            + header.eventCount * sizeof(EventRef);

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            return false;
        output.write(reinterpret_cast<const char*>(&header), sizeof(header));
        if (!sources.empty())
            output.write(reinterpret_cast<const char*>(sources.data()),
                    sources.size() * sizeof(SourceRecord));
        if (!fFrames.empty())
            output.write(reinterpret_cast<const char*>(fFrames.data()),
                    fFrames.size() * sizeof(FrameRef));
        for (size_t first = 0; first < fFrames.size();) {
            size_t end = first + 1;
            while (end < fFrames.size() && fFrames[end].eventId == fFrames[first].eventId)
                ++end;
            EventRef event = {first, fFrames[first].eventId,
                static_cast<uint32_t>(end - first)};
            output.write(reinterpret_cast<const char*>(&event), sizeof(event));
            first = end;
        }
        if (!auxiliaryFrames.empty())
            output.write(reinterpret_cast<const char*>(auxiliaryFrames.data()),
                    auxiliaryFrames.size() * sizeof(AuxiliaryFrameRef));
        return output.good();
    }

    bool Load(const std::string& path, const std::vector<SourceRecord>& expectedSources,
            std::vector<AuxiliaryFrameRef>& auxiliaryFrames)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return false;
        input.seekg(0, std::ios::end);
        const auto endPosition = input.tellg();
        input.seekg(0, std::ios::beg);
        FileHeader header = {};
        input.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!input || std::memcmp(header.magic, "LKGVIX2", 7) != 0
                || header.version != 2 || header.sourceCount != expectedSources.size()
                || header.sourceTableOffset != sizeof(FileHeader)
                || header.frameTableOffset != header.sourceTableOffset
                    + header.sourceCount * sizeof(SourceRecord)
                || header.eventTableOffset != header.frameTableOffset
                    + header.frameCount * sizeof(FrameRef)
                || header.auxiliaryTableOffset != header.eventTableOffset
                    + header.eventCount * sizeof(EventRef)
                || endPosition < 0
                || uint64_t(endPosition) != header.auxiliaryTableOffset
                    + header.auxiliaryFrameCount * sizeof(AuxiliaryFrameRef))
            return false;

        std::vector<SourceRecord> storedSources(header.sourceCount);
        if (!storedSources.empty())
            input.read(reinterpret_cast<char*>(storedSources.data()),
                    storedSources.size() * sizeof(SourceRecord));
        if (!input || storedSources.size() != expectedSources.size())
            return false;
        for (size_t i = 0; i < storedSources.size(); ++i) {
            if (storedSources[i].fileSize != expectedSources[i].fileSize
                    || storedSources[i].modificationTimeNs
                        != expectedSources[i].modificationTimeNs)
                return false;
        }

        try {
            fFrames.resize(header.frameCount);
            auxiliaryFrames.resize(header.auxiliaryFrameCount);
        }
        catch (...) {
            fFrames.clear();
            auxiliaryFrames.clear();
            return false;
        }
        if (!fFrames.empty())
            input.read(reinterpret_cast<char*>(fFrames.data()),
                    fFrames.size() * sizeof(FrameRef));
        input.seekg(static_cast<std::streamoff>(header.auxiliaryTableOffset),
                std::ios::beg);
        if (!auxiliaryFrames.empty())
            input.read(reinterpret_cast<char*>(auxiliaryFrames.data()),
                    auxiliaryFrames.size() * sizeof(AuxiliaryFrameRef));
        if (!input) {
            fFrames.clear();
            auxiliaryFrames.clear();
            return false;
        }
        return true;
    }

  private:
    std::vector<FrameRef> fFrames;
};

static_assert(sizeof(LKGETOfflineFrameIndex::FrameRef) == 16,
        "Offline GET frame references must stay compact");
static_assert(sizeof(LKGETOfflineFrameIndex::FileHeader) == 72,
        "Offline GET index header layout changed");
static_assert(sizeof(LKGETOfflineFrameIndex::EventRef) == 16,
        "Offline GET event references must stay compact");
static_assert(sizeof(LKGETOfflineFrameIndex::SourceRecord) == 16,
        "Offline GET source records must stay compact");
static_assert(sizeof(LKGETOfflineFrameIndex::AuxiliaryFrameRef) == 16,
        "Offline GET auxiliary frame references must stay compact");

#endif
