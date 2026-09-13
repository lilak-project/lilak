#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
using namespace std;

#include "LKCoboFrameMergerTask.h"

#include "GETChannel.h"
#include "LKEventHeader.h"
#include "get_convter/LKGETOfflineFrameIndex.h"

#include <sys/socket.h> /* for socket(), connect(), recv() */
#include <sys/file.h>
#include <sys/stat.h>
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <fcntl.h>
#include <unistd.h>     /* for close() */
#include <cstring>

ClassImp(LKCoboFrameMergerTask);

namespace {
// Size in bytes of the frame whose 8-byte primary header starts at hdr.
// Mirrors LKGETFrameParser: low nibble of byte 0 is the log2 block size and
// bytes 1..3 hold the frame size in blocks (big endian) for CoBo frames.
uint64_t PeekFrameSize(const uint8_t* hdr)
{
    auto p2Block = uint8_t(hdr[0] & 0x0F);
    uint64_t blockSize = (p2Block == 0 ? 1ULL : (1ULL << p2Block));
    uint64_t sizeInBlocks = (uint32_t(hdr[1]) << 16) | (uint32_t(hdr[2]) << 8) | uint32_t(hdr[3]);
    return sizeInBlocks * blockSize;
}

std::string FindMFMFamilyBase(const std::vector<TString>& files)
{
    if (files.empty())
        return "";
    std::string base = files.front().Data();
    auto dot = base.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < base.size()
            && std::all_of(base.begin() + dot + 1, base.end(), ::isdigit)) {
        auto candidate = base.substr(0, dot);
        if (::access(candidate.c_str(), F_OK) == 0)
            base = candidate;
    }
    for (size_t i = 0; i < files.size(); ++i) {
        const std::string expected = i == 0 ? base : base + "." + std::to_string(i);
        if (std::string(files[i].Data()) != expected)
            return "";
    }
    return base;
}

std::string FileNameOnly(const std::string& path)
{
    auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string JsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (unsigned char ch : value) {
        if (ch == '\\' || ch == '"') output << '\\' << ch;
        else if (ch == '\n') output << "\\n";
        else if (ch == '\r') output << "\\r";
        else if (ch == '\t') output << "\\t";
        else if (ch < 0x20) {
            const char* digits = "0123456789abcdef";
            output << "\\u00" << digits[ch >> 4] << digits[ch & 0x0f];
        }
        else output << ch;
    }
    return output.str();
}

uint64_t MTimeNs(const struct stat& status)
{
#if defined(__APPLE__)
    return uint64_t(status.st_mtimespec.tv_sec) * 1000000000ULL + status.st_mtimespec.tv_nsec;
#else
    return uint64_t(status.st_mtim.tv_sec) * 1000000000ULL + status.st_mtim.tv_nsec;
#endif
}

bool ReadSourceRecords(const std::vector<TString>& files,
        std::vector<LKGETOfflineFrameIndex::SourceRecord>& records)
{
    records.clear();
    for (const auto& file : files) {
        struct stat status = {};
        if (::stat(file.Data(), &status) != 0) {
            records.clear();
            return false;
        }
        records.push_back({static_cast<uint64_t>(status.st_size), MTimeNs(status)});
    }
    return true;
}

bool SameSourceRecords(const std::vector<LKGETOfflineFrameIndex::SourceRecord>& lhs,
        const std::vector<LKGETOfflineFrameIndex::SourceRecord>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i].fileSize != rhs[i].fileSize
                || lhs[i].modificationTimeNs != rhs[i].modificationTimeNs)
            return false;
    }
    return true;
}

bool ExistingIndexIsCurrent(const std::string& indexPath,
        const std::vector<LKGETOfflineFrameIndex::SourceRecord>& sources,
        size_t frameCount)
{
    struct stat indexStatus = {};
    if (::stat(indexPath.c_str(), &indexStatus) != 0)
        return false;
    std::ifstream input(indexPath, std::ios::binary);
    LKGETOfflineFrameIndex::FileHeader header = {};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!input || std::memcmp(header.magic, "LKGVIX2", 7) != 0
            || header.version != 2 || header.sourceCount != sources.size()
            || header.frameCount != frameCount
            || header.sourceTableOffset != sizeof(header)
            || header.frameTableOffset != header.sourceTableOffset
                + header.sourceCount * sizeof(LKGETOfflineFrameIndex::SourceRecord)
            || header.eventTableOffset != header.frameTableOffset
                + header.frameCount * sizeof(LKGETOfflineFrameIndex::FrameRef)
            || header.auxiliaryTableOffset != header.eventTableOffset
                + header.eventCount * sizeof(LKGETOfflineFrameIndex::EventRef)
            || uint64_t(indexStatus.st_size) != header.auxiliaryTableOffset
                + header.auxiliaryFrameCount
                    * sizeof(LKGETOfflineFrameIndex::AuxiliaryFrameRef))
        return false;
    std::vector<LKGETOfflineFrameIndex::SourceRecord> storedSources(header.sourceCount);
    if (!storedSources.empty())
        input.read(reinterpret_cast<char*>(storedSources.data()),
                storedSources.size() * sizeof(storedSources.front()));
    if (!input || !SameSourceRecords(storedSources, sources))
            return false;
    return true;
}

bool WriteEventRanges(const std::string& path, const std::string& indexPath,
        const std::vector<TString>& files, const std::vector<uint64_t>& frameCounts,
        const std::vector<uint32_t>& minimumEvents,
        const std::vector<uint32_t>& maximumEvents,
        const LKGETOfflineFrameIndex& index)
{
    std::ofstream output(path, std::ios::trunc);
    if (!output)
        return false;
    output << "{\n  \"eventCount\": " << index.EventCount()
           << ",\n  \"format\": \"lilak-mfm-event-ranges\""
           << ",\n  \"frameCount\": " << index.Size()
           << ",\n  \"indexFile\": \"" << JsonEscape(FileNameOnly(indexPath)) << "\""
           << ",\n  \"sources\": [\n";
    for (size_t i = 0; i < files.size(); ++i) {
        struct stat status = {};
        if (::stat(files[i].Data(), &status) != 0)
            return false;
        output << "    {\n      \"frameCount\": " << frameCounts[i]
               << ",\n      \"maxEvent\": ";
        if (frameCounts[i] == 0) output << "null";
        else output << maximumEvents[i];
        output << ",\n      \"minEvent\": ";
        if (frameCounts[i] == 0) output << "null";
        else output << minimumEvents[i];
        output << ",\n      \"mtimeNs\": " << MTimeNs(status)
               << ",\n      \"name\": \"" << JsonEscape(FileNameOnly(files[i].Data())) << "\""
               << ",\n      \"size\": " << uint64_t(status.st_size) << "\n    }";
        if (i + 1 != files.size()) output << ',';
        output << '\n';
    }
    output << "  ],\n  \"version\": 1\n}\n";
    return output.good();
}

bool SaveMFMEventIndex(const std::vector<TString>& files,
        const std::vector<LKGETOfflineFrameIndex::SourceRecord>& sources,
        const std::vector<uint64_t>& frameCounts,
        const std::vector<uint32_t>& minimumEvents,
        const std::vector<uint32_t>& maximumEvents,
        const LKGETOfflineFrameIndex& index,
        const std::vector<LKGETOfflineFrameIndex::AuxiliaryFrameRef>& auxiliaryFrames)
{
    const auto base = FindMFMFamilyBase(files);
    if (base.empty())
        return false;
    const auto indexPath = base + ".mfm_event_index";
    const auto rangesPath = base + ".mfm_event_ranges.json";
    const auto lockPath = indexPath + ".lock";
    const int lockFd = ::open(lockPath.c_str(), O_CREAT | O_RDWR, 0664);
    if (lockFd < 0 || ::flock(lockFd, LOCK_EX) != 0) {
        if (lockFd >= 0) ::close(lockFd);
        return false;
    }

    const auto suffix = ".tmp." + std::to_string(::getpid());
    const auto temporaryIndex = indexPath + suffix;
    const auto temporaryRanges = rangesPath + suffix;
    bool success = true;
    if (!ExistingIndexIsCurrent(indexPath, sources, index.Size())) {
        success = index.Write(temporaryIndex, sources, auxiliaryFrames);
        std::vector<LKGETOfflineFrameIndex::SourceRecord> currentSources;
        success = success && ReadSourceRecords(files, currentSources)
            && SameSourceRecords(sources, currentSources);
        success = success && ::rename(temporaryIndex.c_str(), indexPath.c_str()) == 0;
        if (success) ::chmod(indexPath.c_str(), 0664);
    }
    if (success) {
        success = WriteEventRanges(temporaryRanges, indexPath, files, frameCounts,
                minimumEvents, maximumEvents, index)
            && ::rename(temporaryRanges.c_str(), rangesPath.c_str()) == 0;
        if (success) ::chmod(rangesPath.c_str(), 0664);
    }
    ::unlink(temporaryIndex.c_str());
    ::unlink(temporaryRanges.c_str());
    ::flock(lockFd, LOCK_UN);
    ::close(lockFd);
    return success;
}

bool LoadMFMEventIndex(const std::vector<TString>& files,
        const std::vector<LKGETOfflineFrameIndex::SourceRecord>& sources,
        LKGETOfflineFrameIndex& index,
        std::vector<LKGETOfflineFrameIndex::AuxiliaryFrameRef>& auxiliaryFrames)
{
    const auto base = FindMFMFamilyBase(files);
    if (base.empty())
        return false;
    return index.Load(base + ".mfm_event_index", sources, auxiliaryFrames);
}
}

LKCoboFrameMergerTask::LKCoboFrameMergerTask()
: LKTask("LKCoboFrameMergerTask","")
{
}

bool LKCoboFrameMergerTask::Init()
{
    fEventHeaderArray = fRun->RegisterBranchA("FrameHeader", "LKEventHeader", 1);
    fChannelArray = fRun->RegisterBranchA("RawData", "GETChannel", 1000);

    if (fPar->CheckPar("LKCoboFrameMergerTask/InputFileName")) {
        auto numInputs = fPar->GetParN("LKCoboFrameMergerTask/InputFileName");
        auto firstInput = fPar->GetParString("LKCoboFrameMergerTask/InputFileName", 0);
        if (firstInput == "online") {
            auto check1 = fPar->CheckPar("LKCoboFrameMergerTask/watcherIP");
            auto check2 = fPar->CheckPar("LKCoboFrameMergerTask/watcherPort");
            if (!check1 || !check2) {
                lk_error << "Must set watcherIP and watcherPort for online run" << endl;
                return false;
            }
            auto ips = fPar->GetParVString("LKCoboFrameMergerTask/watcherIP");
            auto numPorts = fPar->GetParN("LKCoboFrameMergerTask/watcherPort");
            if ((int) ips.size() != numPorts) {
                lk_error << "watcherIP and watcherPort must list the same number of sources" << endl;
                return false;
            }
            for (size_t i = 0; i < ips.size(); ++i) {
                fWatcherIP.push_back(string(ips[i].Data()));
                fWatcherPort.push_back(fPar->GetParInt("LKCoboFrameMergerTask/watcherPort", (int) i));
                lk_info << "Source " << i << " watcher " << fWatcherIP[i] << ":" << fWatcherPort[i] << endl;
            }
            fPar->UpdatePar(fSleepBeforeConnect, "LKCoboFrameMergerTask/sleepBeforeConnect");
            fPar->UpdatePar(fBreakAfterFailNumber, "LKCoboFrameMergerTask/breakAfterFailNumber");
        }
        else {
            if (fTriggerInputFileNameArray.size() > 0) {
                lk_error << "Cannot use two different inputs from LKCoboFrameMergerTask/InputFileName and LKRun" << endl;
                lk_error << "Using input from LKCoboFrameMergerTask/InputFileName ..." << endl;
            }
            fTriggerInputFileNameArray.clear();
            for (int i = 0; i < numInputs; ++i)
                fTriggerInputFileNameArray.push_back(fPar->GetParString("LKCoboFrameMergerTask/InputFileName", i));
        }
    }
    else if (fTriggerInputFileNameArray.size() > 0) {
        lk_info << "Using " << fTriggerInputFileNameArray.size() << " input source(s) from LKRun" << endl;
    }
    else {
        lk_error << "No input!" << endl;
        return false;
    }

    fPar->UpdatePar(fSaveMFMEventIndex,
            "LKCoboFrameMergerTask/save_mfm_event_index true # Save viewer-compatible MFM event index beside raw files");
    fPar->UpdatePar(fUseMFMEventIndex,
            "LKCoboFrameMergerTask/use_mfm_event_index true # Load a valid MFM event index when available");
    fPar->UpdatePar(fMFMEventIndexOnly,
            "LKCoboFrameMergerTask/mfm_event_index_only false # Stop after creating or loading the MFM event index");

    fConverter = new LKGETRawConverter();
    fConverter->SetPar(fPar);
    TString zeroSuppressionMode = "auto";
    fPar->UpdatePar(zeroSuppressionMode,
            "LKCoboFrameMergerTask/zero_suppression_mode auto # auto | none | linear_interpolation");
    if (!fConverter->SetZeroSuppressionMode(zeroSuppressionMode))
        return false;
    fConverter->SetChannelArray(fChannelArray);
    fConverter->SetEventHeaderArray(fEventHeaderArray);
    if (!fConverter->Init()) {
        lk_error << "LKGETRawConverter cannot be initialized!" << endl;
        return false;
    }

    fMerger = new LKCoboFrameMerger();

    return true;
}

void LKCoboFrameMergerTask::Run(Long64_t numEvents)
{
    fNumEvents = numEvents;
    fCountEvents = 0;
    fContinueEvent = true;
    fMergedBufferLast = 0;

    const int nSources = (int) fTriggerInputFileNameArray.size();
    if (nSources <= 0) {
        lk_error << "No input source to merge!" << endl;
        fRun->SignalEndOfRun();
        return;
    }

    e_cout << "===============================================================================================================" << endl;
    std::vector<std::ifstream> inputs(nSources);
    for (int i = 0; i < nSources; ++i) {
        auto fileName = fTriggerInputFileNameArray[i];
        lk_info << "Opening source " << i << ": " << fileName << endl;
        inputs[i].open(fileName.Data(), ios::binary | ios::in);
        if (!inputs[i].is_open()) {
            lk_error << "Could not open " << fileName << endl;
            fRun->SignalEndOfRun();
            return;
        }
    }

    std::vector<LKGETOfflineFrameIndex::SourceRecord> initialSourceRecords;
    if (!ReadSourceRecords(fTriggerInputFileNameArray, initialSourceRecords)) {
        lk_error << "Could not read input file size and modification time" << endl;
        fRun->SignalEndOfRun();
        return;
    }

    // Frames are not stored in strictly increasing event-id order inside a raw
    // CoBo file (the writer groups them by AsAd), so every frame must first be
    // indexed.  Store compact file references just like cobo-frame-merge does;
    // retaining LKGETRawFrame here would keep the complete raw payload in RAM.
    LKGETOfflineFrameIndex frameIndex;
    std::vector<LKGETOfflineFrameIndex::AuxiliaryFrameRef> auxiliaryFrames;
    std::vector<uint64_t> sourceFrameCounts(nSources, 0);
    std::vector<uint32_t> sourceMinimumEvents(
            nSources, std::numeric_limits<uint32_t>::max());
    std::vector<uint32_t> sourceMaximumEvents(nSources, 0);
    const auto cacheBase = FindMFMFamilyBase(fTriggerInputFileNameArray);
    const auto cacheIndexPath = cacheBase.empty()
        ? std::string() : cacheBase + ".mfm_event_index";
    const auto cacheRangesPath = cacheBase.empty()
        ? std::string() : cacheBase + ".mfm_event_ranges.json";
    struct stat cacheStatus = {};
    const bool cacheExists = !cacheIndexPath.empty()
        && ::stat(cacheIndexPath.c_str(), &cacheStatus) == 0;
    if (cacheBase.empty()) {
        lk_warning << "MFM event-index cache is unavailable: inputs are not one complete numbered file family" << endl;
    }
    else {
        lk_info << "MFM event-index cache file: " << cacheIndexPath << endl;
        if (!fUseMFMEventIndex)
            lk_info << "MFM event-index cache loading is disabled" << endl;
        else if (cacheExists)
            lk_info << "Found MFM event-index cache (" << uint64_t(cacheStatus.st_size)
                    << " bytes); validating against " << nSources << " source file(s)" << endl;
        else
            lk_info << "MFM event-index cache not found; raw sources will be indexed" << endl;
    }
    const bool loadedIndex = fUseMFMEventIndex
        && LoadMFMEventIndex(fTriggerInputFileNameArray, initialSourceRecords,
                frameIndex, auxiliaryFrames);
    if (loadedIndex) {
        lk_info << "Loaded MFM event-index cache: " << frameIndex.Size()
                << " frame reference(s), " << frameIndex.EventCount()
                << " event(s), " << auxiliaryFrames.size()
                << " auxiliary frame(s)" << endl;
        for (const auto& ref : auxiliaryFrames) {
            if (ref.sourceId >= inputs.size()) {
                lk_error << "Invalid auxiliary source in MFM event index" << endl;
                fRun->SignalEndOfRun();
                return;
            }
            auto& input = inputs[ref.sourceId];
            input.clear();
            input.seekg(static_cast<std::streamoff>(ref.filePosition), ios::beg);
            LKGETRawFrame frame;
            if (!input.good() || !fParser.ReadNextFrame(input, frame)
                    || frame.frameType != ref.frameType
                    || (frame.frameType != 0x7 && frame.frameType != 0x8)) {
                lk_error << "Invalid auxiliary frame in MFM event index at byte "
                         << ref.filePosition << endl;
                fRun->SignalEndOfRun();
                return;
            }
            fConverter->ProcessFrame(frame);
        }
    }
    else {
        if (fUseMFMEventIndex && cacheExists)
            lk_warning << "MFM event-index cache is stale or invalid; rebuilding from raw sources" << endl;
        for (int i = 0; i < nSources && fContinueEvent; ++i) {
            Long64_t nCobo = 0;
            while (true) {
                auto framePosition = inputs[i].tellg();
                if (framePosition == std::streampos(-1)) {
                    lk_error << "Could not get input position for source " << i << endl;
                    fRun->SignalEndOfRun();
                    return;
                }

                LKGETRawFrame frame;
                if (!fParser.ReadNextFrame(inputs[i], frame))
                    break;
                if (frame.frameType == 0x7 || frame.frameType == 0x8) {
                    auxiliaryFrames.push_back({
                            static_cast<uint64_t>(static_cast<std::streamoff>(framePosition)),
                            static_cast<uint32_t>(i), frame.frameType});
                    fConverter->ProcessFrame(frame);
                }
                else if (frame.frameType == 1 || frame.frameType == 2) {
                    frameIndex.Add(static_cast<uint32_t>(i),
                            static_cast<uint64_t>(static_cast<std::streamoff>(framePosition)),
                            frame.eventIdx);
                    sourceMinimumEvents[i] = std::min(sourceMinimumEvents[i], frame.eventIdx);
                    sourceMaximumEvents[i] = std::max(sourceMaximumEvents[i], frame.eventIdx);
                    ++nCobo;
                }
            }
            inputs[i].clear();
            sourceFrameCounts[i] = nCobo;
            lk_info << "Source " << i << ": indexed " << nCobo << " CoBo frame(s)" << endl;
        }

        lk_info << "Sorting " << frameIndex.Size() << " compact frame reference(s); index uses "
                << frameIndex.CapacityBytes() / (1024 * 1024)
                << " MiB" << endl;
        frameIndex.Sort();
        if (fSaveMFMEventIndex) {
            if (!cacheIndexPath.empty()) {
                lk_info << "Saving MFM event-index cache: " << cacheIndexPath << endl;
                lk_info << "Cache contents: " << frameIndex.Size()
                        << " frame reference(s), " << frameIndex.EventCount()
                        << " event(s), " << auxiliaryFrames.size()
                        << " auxiliary frame(s)" << endl;
            }
            std::vector<LKGETOfflineFrameIndex::SourceRecord> finalSourceRecords;
            if (!ReadSourceRecords(fTriggerInputFileNameArray, finalSourceRecords)
                    || !SameSourceRecords(initialSourceRecords, finalSourceRecords)) {
                lk_warning << "Raw input changed while indexing; MFM event index was not saved" << endl;
            }
            else if (SaveMFMEventIndex(fTriggerInputFileNameArray, initialSourceRecords,
                        sourceFrameCounts, sourceMinimumEvents, sourceMaximumEvents,
                        frameIndex, auxiliaryFrames)) {
                struct stat savedIndexStatus = {};
                struct stat savedRangesStatus = {};
                const bool haveIndexSize = ::stat(cacheIndexPath.c_str(), &savedIndexStatus) == 0;
                const bool haveRangesSize = ::stat(cacheRangesPath.c_str(), &savedRangesStatus) == 0;
                lk_info << "Saved MFM event-index cache: " << cacheIndexPath
                        << " (" << (haveIndexSize ? uint64_t(savedIndexStatus.st_size) : 0)
                        << " bytes)" << endl;
                lk_info << "Saved MFM event-range metadata: " << cacheRangesPath
                        << " (" << (haveRangesSize ? uint64_t(savedRangesStatus.st_size) : 0)
                        << " bytes)" << endl;
            }
            else {
                lk_warning << "Could not save MFM event index; inputs must be one complete numbered file family in a writable directory" << endl;
            }
        }
        else {
            lk_info << "MFM event-index cache saving is disabled" << endl;
        }
    }

    if (fMFMEventIndexOnly) {
        lk_info << "MFM event index-only mode complete; skipping event reconstruction" << endl;
        for (auto& input : inputs)
            input.close();
        fRun->SignalEndOfRun();
        return;
    }
    const auto& indexedFrames = frameIndex.Frames();

    // Re-read and merge one event at a time.  Only that event's payload is
    // resident, while the compact index remains in memory.
    size_t indexPosition = 0;
    while (indexPosition < indexedFrames.size() && fContinueEvent) {
        const uint32_t eventId = indexedFrames[indexPosition].eventId;
        LKGETRawFrame merged;
        merged.frameType = 0xFF01;
        merged.isLayered = true;
        merged.eventIdx = eventId;

        uint64_t mergedSize = 0;
        do {
            const auto& ref = indexedFrames[indexPosition];
            auto& input = inputs[ref.sourceId];
            input.clear();
            input.seekg(static_cast<std::streamoff>(ref.filePosition), ios::beg);

            LKGETRawFrame child;
            if (!input.good() || !fParser.ReadNextFrame(input, child)) {
                lk_error << "Could not re-read source " << ref.sourceId
                         << " at byte " << ref.filePosition << endl;
                fRun->SignalEndOfRun();
                return;
            }
            if ((child.frameType != 1 && child.frameType != 2) || child.eventIdx != eventId) {
                lk_error << "Indexed frame changed in source " << ref.sourceId
                         << " at byte " << ref.filePosition << endl;
                fRun->SignalEndOfRun();
                return;
            }

            mergedSize += child.bytes.size();
            merged.children.push_back(std::move(child));
            ++indexPosition;
        }
        while (indexPosition < indexedFrames.size() && indexedFrames[indexPosition].eventId == eventId);

        merged.itemCount = static_cast<uint32_t>(merged.children.size());
        merged.frameSizeBytes = mergedSize;
        fLastMergedSize = static_cast<Int_t>(mergedSize);
        fConverter->ProcessFrame(merged);
        SignalNextEvent();
    }

    for (auto& input : inputs)
        input.close();
}

void LKCoboFrameMergerTask::DrainReadyEvents()
{
    LKGETRawFrame merged;
    while (fContinueEvent && fMerger->PopReadyEvent(merged)) {
        fLastMergedSize = (Int_t) merged.frameSizeBytes;
        fConverter->ProcessFrame(merged);
        SignalNextEvent();
    }
}

void LKCoboFrameMergerTask::RunOnline(Long64_t numEvents)
{
    fRunOnline = true;
    fNumEvents = numEvents;
    fCountEvents = 0;
    fContinueEvent = true;
    fMergedBufferLast = 0;

    const int nSources = (int) fWatcherIP.size();
    if (nSources <= 0) {
        lk_error << "RunOnline needs at least one watcher source" << endl;
        return;
    }

    fMerger->SetNumSources(nSources);
    fOnlineBuffer.assign(nSources, string());

    std::vector<int> countConnectFail(nSources, 0);
    std::vector<int> countBufferZero(nSources, 0);
    std::vector<int> countConnect(nSources, 0);

    while (fContinueEvent) {
        bool anyAlive = false;

        for (int i = 0; i < nSources && fContinueEvent; ++i) {
            if (countConnectFail[i] > fBreakAfterFailNumber || countBufferZero[i] > fBreakAfterFailNumber) {
                fMerger->CloseSource(i);
                continue;
            }
            anyAlive = true;

            int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sock <= 0) {
                countConnectFail[i]++;
                lk_error << "socket call failed for source " << i << endl;
                continue;
            }

            struct sockaddr_in servAddr;
            memset(&servAddr, 0, sizeof(servAddr));
            servAddr.sin_family = AF_INET;
            servAddr.sin_addr.s_addr = inet_addr(fWatcherIP[i].c_str());
            servAddr.sin_port = htons(fWatcherPort[i]);
            if (countConnect[i] > 0)
                sleep(fSleepBeforeConnect);
            countConnect[i]++;
            if (connect(sock, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0) {
                countConnectFail[i]++;
                close(sock);
                continue;
            }

            int sizeBuffer = 0;
            recv(sock, &sizeBuffer, sizeof(int), 0);
            if (sizeBuffer <= 4) {
                countBufferZero[i]++;
                close(sock);
                continue;
            }

            std::vector<char> buffer(sizeBuffer);
            int totalReceived = 0;
            while (totalReceived < sizeBuffer) {
                int received = recv(sock, buffer.data() + totalReceived, sizeBuffer - totalReceived, 0);
                if (received <= 0)
                    break;
                totalReceived += received;
            }
            close(sock);

            if (totalReceived > 0) {
                fOnlineBuffer[i].append(buffer.data(), totalReceived);
                ConsumeOnlineBuffer(i);
                DrainReadyEvents();
            }
        }

        if (!anyAlive)
            break;
    }

    // drain the tail once every source has been closed
    DrainReadyEvents();
}

void LKCoboFrameMergerTask::ConsumeOnlineBuffer(Int_t sourceId)
{
    string& buf = fOnlineBuffer[sourceId];

    while (buf.size() >= 8) {
        const uint8_t* hdr = reinterpret_cast<const uint8_t*>(buf.data());
        uint64_t frameSize = PeekFrameSize(hdr);
        if (frameSize < 8) {
            // unrecoverable framing error: drop the buffer
            buf.clear();
            return;
        }
        if (buf.size() < frameSize)
            return; // wait for the rest of this frame

        std::istringstream slice(string(buf.data(), frameSize), ios::binary | ios::in);
        LKGETRawFrame frame;
        if (fParser.ReadNextFrame(slice, frame)) {
            if (frame.frameType == 0x7 || frame.frameType == 0x8)
                fConverter->ProcessFrame(frame);
            else if (frame.frameType == 1 || frame.frameType == 2)
                fMerger->AddFrame(sourceId, frame);
        }
        buf.erase(0, frameSize);
    }
}

bool LKCoboFrameMergerTask::EndOfRun()
{
    return true;
}

void LKCoboFrameMergerTask::SignalNextEvent()
{
    fCountEvents++;
    bool printLog = (fRun->CheckMute(fCountEvents) == false);

    if (fRunOnline) {
        if (printLog)
            lk_info << "New merged event! (" << fCountEvents << ")" << endl;
    }
    else if (printLog) {
        lk_info << "New merged event! at merged buffer: " << (fMergedBufferLast + fLastMergedSize)
                << " (" << fLastMergedSize << ")" << endl;
    }

    // The converter already filled FrameHeader (event number / time); add the
    // merged-buffer bookkeeping for consistency with the other trigger tasks.
    auto eventHeader = (LKEventHeader*) fEventHeaderArray->ConstructedAt(0);
    eventHeader->SetBufferStart(fMergedBufferLast);
    eventHeader->SetBufferSize(fLastMergedSize);
    fMergedBufferLast += fLastMergedSize;

    fContinueEvent = fRun->ExecuteNextEvent();
    if (fNumEvents > 0 && fCountEvents >= fNumEvents)
        fContinueEvent = false;
}
