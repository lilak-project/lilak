#include <iostream>
using namespace std;

#include "LKCoboFrameMerger.h"

#include "LKLogger.h"

#include <limits>
#include <utility>

ClassImp(LKCoboFrameMerger);

LKCoboFrameMerger::LKCoboFrameMerger()
{
}

void LKCoboFrameMerger::Clear()
{
    fPending.clear();
    for (Int_t i = 0; i < fNumSources; ++i) {
        fSourceOpen[i] = true;
        fSourceSeen[i] = false;
        fSourceHighWater[i] = 0;
    }
}

void LKCoboFrameMerger::SetNumSources(Int_t numSources)
{
    fNumSources = numSources < 0 ? 0 : numSources;
    fSourceOpen.assign(fNumSources, true);
    fSourceSeen.assign(fNumSources, false);
    fSourceHighWater.assign(fNumSources, 0);
    fPending.clear();
}

void LKCoboFrameMerger::AddFrame(Int_t sourceId, const LKGETRawFrame& frame)
{
    if (sourceId < 0 || sourceId >= fNumSources) {
        lk_error << "Invalid source id " << sourceId << " (have " << fNumSources << " sources)" << endl;
        return;
    }

    uint32_t eventId = frame.eventIdx;
    fPending[eventId].push_back(frame);

    fSourceSeen[sourceId] = true;
    if (eventId > fSourceHighWater[sourceId])
        fSourceHighWater[sourceId] = eventId;
}

void LKCoboFrameMerger::CloseSource(Int_t sourceId)
{
    if (sourceId < 0 || sourceId >= fNumSources)
        return;
    fSourceOpen[sourceId] = false;
}

bool LKCoboFrameMerger::PopReadyEvent(LKGETRawFrame& merged)
{
    if (fPending.empty())
        return false;

    bool anyOpen = false;
    bool anyOpenUnseen = false;
    uint32_t lowWater = std::numeric_limits<uint32_t>::max();
    for (Int_t i = 0; i < fNumSources; ++i) {
        if (!fSourceOpen[i])
            continue;
        anyOpen = true;
        if (!fSourceSeen[i]) {
            anyOpenUnseen = true;
            break;
        }
        if (fSourceHighWater[i] < lowWater)
            lowWater = fSourceHighWater[i];
    }

    const uint32_t eventId = fPending.begin()->first;

    if (anyOpen) {
        // An open source has not produced any frame yet: its event position is
        // unknown, so nothing can be declared final.
        if (anyOpenUnseen)
            return false;
        // Only events strictly below the slowest source are guaranteed final.
        if (!(eventId < lowWater))
            return false;
    }
    // else: every source closed, flush the remaining events in ascending order.

    BuildLayeredFrame(eventId, fPending.begin()->second, merged);
    fPending.erase(fPending.begin());
    return true;
}

void LKCoboFrameMerger::BuildLayeredFrame(uint32_t eventId, std::vector<LKGETRawFrame>& children, LKGETRawFrame& merged) const
{
    merged = LKGETRawFrame();
    merged.frameType = 0xFF01;
    merged.isLayered = true;
    merged.isBlob = false;
    merged.eventIdx = eventId;
    merged.itemCount = (uint32_t) children.size();

    uint64_t totalBytes = 0;
    for (const auto& child : children)
        totalBytes += child.bytes.size();
    merged.frameSizeBytes = totalBytes;

    merged.children = std::move(children);
}
