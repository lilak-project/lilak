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
#include "get_convter/LKGETFrameFileIndex.h"

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

}

LKCoboFrameMergerTask::LKCoboFrameMergerTask()
: LKTask("LKCoboFrameMergerTask","")
{
}

bool LKCoboFrameMergerTask::Init()
{
    fEventHeaderArray = fRun->RegisterBranchA("FrameHeader", "LKEventHeader", 1);
    fChannelArray = fRun->RegisterBranchA("RawData", "GETChannel", 1000);

    // Read every parameter before the input checks below: make_meta and collect_par run Init()
    // without input, and a parameter read after an early return never reaches their template.
    fPar->Require("LKCoboFrameMergerTask/watcherIP", "ip0", "online: one watcher IP per source", "/");
    fPar->Require("LKCoboFrameMergerTask/watcherPort", "port0", "online: one watcher port per source", "/");
    fPar->UpdatePar(fSleepBeforeConnect,
            "LKCoboFrameMergerTask/sleepBeforeConnect 1 # online: seconds to wait before connecting");
    fPar->UpdatePar(fBreakAfterFailNumber,
            "LKCoboFrameMergerTask/breakAfterFailNumber 10 # online: stop after this many failed reads");
    fPar->UpdatePar(fSaveMFMEventIndex,
            "LKCoboFrameMergerTask/save_mfm_event_index true # Save each raw file's frame index beside it (.<file>.scope_index, shared with Scope)");
    fPar->UpdatePar(fUseMFMEventIndex,
            "LKCoboFrameMergerTask/use_mfm_event_index true # Use a raw file's saved frame index when it is still valid");
    fPar->UpdatePar(fMFMEventIndexOnly,
            "LKCoboFrameMergerTask/mfm_event_index_only false # Stop after creating or loading the frame indexes");

    fConverter = new LKGETRawConverter();
    fConverter->SetPar(fPar);
    TString zeroSuppressionMode = "auto";
    fPar->UpdatePar(zeroSuppressionMode,
            "LKCoboFrameMergerTask/zero_suppression_mode auto # auto | none | linear_interpolation");

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

    // Each source keeps its frame table beside it (".<name>.scope_index", shared with the Scope
    // viewer), so any combination of CoBo files is indexed once.  Frames are not in event order
    // inside a CoBo file (the writer groups them by AsAd), so the merge needs every frame's
    // position first; only compact references stay in memory, not the payloads.
    auto readFrameRecords = [&](const std::string& fileName,
            std::vector<LKGETFrameFileIndex::Record>& records) -> bool {
        const auto indexPath = LKGETFrameFileIndex::PathFor(fileName);
        uint64_t size = 0, mtimeNs = 0;
        if (!LKGETFrameFileIndex::Fingerprint(fileName, size, mtimeNs)) {
            lk_error << "Could not read size and modification time of " << fileName << endl;
            return false;
        }
        if (fUseMFMEventIndex && LKGETFrameFileIndex::Load(fileName, size, mtimeNs, records)) {
            lk_info << "Loaded frame index " << indexPath << " (" << records.size() << " frames)" << endl;
            return true;
        }
        uint64_t cutAt = 0;
        std::string error;
        if (!LKGETFrameFileIndex::Scan(fileName, records, cutAt, error)) {
            lk_error << "Could not index " << fileName << ": " << error << endl;
            return false;
        }
        if (cutAt > 0) {
            lk_warning << fileName << ": last frame cut off at byte " << cutAt << " of " << size
                       << " (file still being written?); using the whole frames, frame index not saved" << endl;
            return true;
        }
        uint64_t sizeAfter = 0, mtimeAfter = 0;
        if (!fSaveMFMEventIndex)
            return true;
        if (!LKGETFrameFileIndex::Fingerprint(fileName, sizeAfter, mtimeAfter)
                || sizeAfter != size || mtimeAfter != mtimeNs)
            lk_warning << fileName << " changed while indexing; frame index not saved" << endl;
        else if (LKGETFrameFileIndex::Save(fileName, size, mtimeNs, records))
            lk_info << "Saved frame index " << indexPath << " (" << records.size() << " frames)" << endl;
        else
            lk_warning << "Could not save frame index " << indexPath << " (folder not writable?)" << endl;
        return true;
    };

    LKGETOfflineFrameIndex frameIndex;
    std::vector<std::pair<int, LKGETFrameFileIndex::Record>> auxiliaryFrames;
    for (int i = 0; i < nSources; ++i) {
        std::vector<LKGETFrameFileIndex::Record> records;
        if (!readFrameRecords(fTriggerInputFileNameArray[i].Data(), records)) {
            fRun->SignalEndOfRun();
            return;
        }
        Long64_t nCobo = 0;
        for (const auto& r : records) {
            if (r.frameType == 1 || r.frameType == 2) {
                frameIndex.Add(static_cast<uint32_t>(i), r.offset, r.eventId);
                ++nCobo;
            }
            else if (r.frameType == 0x7 || r.frameType == 0x8)
                auxiliaryFrames.push_back({i, r});
        }
        lk_info << "Source " << i << ": " << nCobo << " CoBo frame(s)" << endl;
    }

    // CoBo topology (0x7) and MuTanT (0x8) frames go to the converter before the events.
    for (const auto& aux : auxiliaryFrames) {
        auto& input = inputs[aux.first];
        input.clear();
        input.seekg(static_cast<std::streamoff>(aux.second.offset), ios::beg);
        LKGETRawFrame frame;
        if (!input.good() || !fParser.ReadNextFrame(input, frame) || frame.frameType != aux.second.frameType) {
            lk_error << "Could not re-read frame type " << aux.second.frameType << " in source " << aux.first
                     << " at byte " << aux.second.offset << endl;
            fRun->SignalEndOfRun();
            return;
        }
        fConverter->ProcessFrame(frame);
    }

    lk_info << "Sorting " << frameIndex.Size() << " compact frame reference(s); index uses "
            << frameIndex.CapacityBytes() / (1024 * 1024) << " MiB" << endl;
    frameIndex.Sort();

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
