#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>
using namespace std;

#include "LKCoboFrameMergerTask.h"

#include "GETChannel.h"
#include "LKEventHeader.h"

#include <sys/socket.h> /* for socket(), connect(), recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <unistd.h>     /* for close() */
#include <cstring>

ClassImp(LKCoboFrameMergerTask);

namespace {
// Offline files can contain tens of millions of frames.  Keep only enough
// information to find each frame again instead of retaining its waveform
// payload in LKCoboFrameMerger::fPending.
struct OfflineFrameRef
{
    OfflineFrameRef(uint64_t position, uint32_t event, uint32_t source)
    : filePosition(position), eventId(event), sourceId(source) {}

    uint64_t filePosition;
    uint32_t eventId;
    uint32_t sourceId;
};

static_assert(sizeof(OfflineFrameRef) == 16,
        "OfflineFrameRef must stay compact; large runs contain many millions of frames");

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

    // Frames are not stored in strictly increasing event-id order inside a raw
    // CoBo file (the writer groups them by AsAd), so every frame must first be
    // indexed.  Store compact file references just like cobo-frame-merge does;
    // retaining LKGETRawFrame here would keep the complete raw payload in RAM.
    std::vector<OfflineFrameRef> frameIndex;
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
                // CoBo topology / MuTanT frames carry no waveform: let the
                // converter accumulate their meta data (scaler, 2p timing).
                fConverter->ProcessFrame(frame);
            }
            else if (frame.frameType == 1 || frame.frameType == 2) {
                frameIndex.emplace_back(
                        static_cast<uint64_t>(static_cast<std::streamoff>(framePosition)),
                        frame.eventIdx,
                        static_cast<uint32_t>(i));
                ++nCobo;
            }
            // any other frame type (blob, ...) is skipped
        }
        inputs[i].clear();
        lk_info << "Source " << i << ": indexed " << nCobo << " CoBo frame(s)" << endl;
    }

    lk_info << "Sorting " << frameIndex.size() << " compact frame reference(s); index uses "
            << (frameIndex.capacity() * sizeof(OfflineFrameRef)) / (1024 * 1024)
            << " MiB" << endl;
    std::sort(frameIndex.begin(), frameIndex.end(),
            [](const OfflineFrameRef& lhs, const OfflineFrameRef& rhs) {
                if (lhs.eventId != rhs.eventId)
                    return lhs.eventId < rhs.eventId;
                if (lhs.sourceId != rhs.sourceId)
                    return lhs.sourceId < rhs.sourceId;
                return lhs.filePosition < rhs.filePosition;
            });

    // Re-read and merge one event at a time.  Only that event's payload is
    // resident, while the compact index remains in memory.
    size_t indexPosition = 0;
    while (indexPosition < frameIndex.size() && fContinueEvent) {
        const uint32_t eventId = frameIndex[indexPosition].eventId;
        LKGETRawFrame merged;
        merged.frameType = 0xFF01;
        merged.isLayered = true;
        merged.eventIdx = eventId;

        uint64_t mergedSize = 0;
        do {
            const auto& ref = frameIndex[indexPosition];
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
        while (indexPosition < frameIndex.size() && frameIndex[indexPosition].eventId == eventId);

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
