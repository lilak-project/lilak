#include "LKSiHitPairMatchingTask.h"

#include "LKLogger.h"
#include "LKRun.h"
#include "LKSiChannel.h"
#include "LKSiDetector.h"
#include "LKSiliconArray.h"
#include "SKSiHit.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

ClassImp(LKSiHitPairMatchingTask)

LKSiHitPairMatchingTask::LKSiHitPairMatchingTask()
    : LKTask("LKSiHitPairMatchingTask", "LKSiHitPairMatchingTask")
{
}

bool LKSiHitPairMatchingTask::Init()
{
    fSiliconArray = (LKSiliconArray *) fRun->FindDetectorPlane("LKSiliconArray");
    if (fSiliconArray == nullptr) {
        lk_error << "LKSiliconArray detector plane is not found. Add STARK or "
                 << "LKSiliconArray before LKSiHitPairMatchingTask." << endl;
        return false;
    }

    fHitArray = fRun->KeepBranchA("SiHit", "SKSiHit");
    if (fHitArray == nullptr) {
        lk_error << "Branch SiHit does not exist. Run LKEnergyRestorationTask "
                 << "before LKSiHitPairMatchingTask." << endl;
        return false;
    }

    fSiChannelArray = fRun->GetBranchA("SiChannel", "LKSiChannel");
    if (fSiChannelArray == nullptr) {
        lk_error << "Branch SiChannel does not exist. TB matching requires the "
                 << "SiChannel branch." << endl;
        return false;
    }

    fPar->UpdatePar(fKeepSingleSideHits,
            "LKSiHitPairMatchingTask/keepSingleSideHits false # Keep unmatched junction or ohmic signals as single-side SiHit objects.");
    if (!LoadSingleSideChannelPatterns())
        return false;
    TString modeMessage = fKeepSingleSideHits ? "enabled" : "disabled";
    if (fKeepSingleSideHits && !fSingleSideChannelPatterns.empty())
        modeMessage += Form(" for %zu channel pattern(s)",
                fSingleSideChannelPatterns.size());
    lk_info << "Single-side SiHit mode: " << modeMessage << endl;

    return true;
}

bool LKSiHitPairMatchingTask::LoadSingleSideChannelPatterns()
{
    fSingleSideChannelPatterns.clear();
    TString parName = "LKSiHitPairMatchingTask/singleSideChannels";
    if (!fPar->CheckPar(parName))
        return true;

    auto numPatterns = fPar->GetParN(parName);
    for (auto iPattern=0; iPattern<numPatterns; ++iPattern) {
        TString text = fPar->GetParString(parName, iPattern);
        text = text.Strip(TString::kBoth);
        if (text.IsNull())
            continue;

        std::array<int,4> pattern = {{-1, -1, -1, -1}};
        std::stringstream stream(text.Data());
        std::string component;
        int index = 0;
        bool valid = true;
        while (std::getline(stream, component, '/')) {
            if (index >= 4 || component.empty()) {
                valid = false;
                break;
            }
            if (component == "a" || component == "A" || component == "*") {
                pattern[index++] = -1;
                continue;
            }

            char *end = nullptr;
            auto value = std::strtol(component.c_str(), &end, 10);
            if (end == component.c_str() || *end != '\0' || value < 0) {
                valid = false;
                break;
            }
            pattern[index++] = (int) value;
        }

        if (!valid || index == 0) {
            lk_error << "Invalid single-side channel pattern '" << text
                     << "'. Use CoBo/AsAd/AGET/Channel with a as wildcard." << endl;
            return false;
        }
        fSingleSideChannelPatterns.push_back(pattern);
    }
    return true;
}

bool LKSiHitPairMatchingTask::MatchesSingleSideChannelPattern(
        const LKSiChannel *channel) const
{
    if (channel == nullptr)
        return false;
    if (fSingleSideChannelPatterns.empty())
        return true;

    const std::array<int,4> address = {{
        channel->GetCobo(), channel->GetAsad(), channel->GetAget(), channel->GetChan()
    }};
    for (const auto &pattern : fSingleSideChannelPatterns) {
        bool matches = true;
        for (auto i=0; i<4; ++i) {
            if (pattern[i] >= 0 && pattern[i] != address[i]) {
                matches = false;
                break;
            }
        }
        if (matches)
            return true;
    }
    return false;
}

bool LKSiHitPairMatchingTask::IsSingleSideHitAllowed(
        const SKSiHit *hit, bool ohmicSide) const
{
    if (!fKeepSingleSideHits || hit == nullptr || fSiChannelArray == nullptr)
        return false;
    if (fSingleSideChannelPatterns.empty())
        return true;

    auto strip = ohmicSide ? hit->GetOhmicStrip() : hit->GetJunctionStrip();
    for (auto iChannel=0; iChannel<fSiChannelArray->GetEntriesFast(); ++iChannel) {
        auto channel = (LKSiChannel *) fSiChannelArray->At(iChannel);
        if (channel == nullptr || channel->GetDetID() != hit->GetDetID()
                || channel->GetSide() != ohmicSide || channel->GetStrip() != strip
                || channel->GetEnergy() <= 0)
            continue;
        if (MatchesSingleSideChannelPattern(channel))
            return true;
    }
    return false;
}

bool LKSiHitPairMatchingTask::IsOhmicOnly(const SKSiHit *hit) const
{
    return hit != nullptr && hit->GetJunctionStrip() < 0
        && hit->GetOhmicStrip() >= 0 && hit->GetEnergyOhmic() > 0;
}

int LKSiHitPairMatchingTask::FindPairDetectorID(const LKSiDetector *detector) const
{
    if (detector == nullptr)
        return -1;

    for (auto iDetector=0; iDetector<fSiliconArray->GetNumSiDetectors(); ++iDetector) {
        auto candidate = fSiliconArray->GetSiDetector(iDetector);
        if (candidate == nullptr || candidate == detector)
            continue;
        if (candidate->GetDetTypeName() == detector->GetDetTypeName()
                && candidate->GetRow() == detector->GetRow()
                && candidate->IsEDetector() != detector->IsEDetector())
            return candidate->GetDetIndex();
    }
    return -1;
}

int LKSiHitPairMatchingTask::FindExpectedOhmicStrip(
        const SKSiHit *junctionHit, const LKSiDetector *detector) const
{
    if (junctionHit == nullptr || detector == nullptr)
        return -1;

    auto numOhmicStrips = detector->GetNumOhmicStrips();
    if (detector->GetUseJunctionUD() && numOhmicStrips > 0
            && junctionHit->GetRelativeZ() >= -1
            && junctionHit->GetRelativeZ() <= 1) {
        // X6 convention: relativeZ=-1 is the low end and +1 is the high end.
        auto expectedStrip = (int) std::floor(
                0.5 * (1-junctionHit->GetRelativeZ()) * numOhmicStrips);
        if (expectedStrip < 0)
            expectedStrip = 0;
        if (expectedStrip >= numOhmicStrips)
            expectedStrip = numOhmicStrips-1;
        return expectedStrip;
    }
    return -1;
}

double LKSiHitPairMatchingTask::FindHitTime(const SKSiHit *hit, bool ohmicSide) const
{
    if (hit == nullptr || fSiChannelArray == nullptr)
        return std::numeric_limits<double>::quiet_NaN();

    auto strip = ohmicSide ? hit->GetOhmicStrip() : hit->GetJunctionStrip();
    double timeSum = 0;
    int numTimes = 0;
    for (auto iChannel=0; iChannel<fSiChannelArray->GetEntriesFast(); ++iChannel) {
        auto channel = (LKSiChannel *) fSiChannelArray->At(iChannel);
        if (channel == nullptr || channel->GetDetID() != hit->GetDetID()
                || channel->GetSide() != ohmicSide || channel->GetStrip() != strip
                || channel->GetEnergy() <= 0 || channel->GetTime() < 0)
            continue;
        timeSum += channel->GetTime();
        ++numTimes;
    }
    if (numTimes == 0)
        return std::numeric_limits<double>::quiet_NaN();
    return timeSum / numTimes;
}

void LKSiHitPairMatchingTask::AttachOhmicHits()
{
    struct Candidate {
        SKSiHit *junctionHit = nullptr;
        SKSiHit *ohmicHit = nullptr;
        int junctionIndex = -1;
        int ohmicIndex = -1;
        int stripDistance = 0;
        double timeDistance = std::numeric_limits<double>::max();
        bool hasTime = false;
    };

    auto numHits = fHitArray->GetEntriesFast();
    std::vector<Candidate> candidates;
    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto junctionHit = (SKSiHit *) fHitArray->At(iHit);
        if (junctionHit == nullptr || IsOhmicOnly(junctionHit)
                || junctionHit->GetEnergy() <= 0)
            continue;

        auto detector = fSiliconArray->GetSiDetector(junctionHit->GetDetID());
        if (detector == nullptr || detector->GetNumSides() < 2)
            continue;
        auto expectedStrip = FindExpectedOhmicStrip(junctionHit, detector);
        auto junctionTime = FindHitTime(junctionHit, false);

        for (auto jHit=0; jHit<numHits; ++jHit) {
            auto ohmicHit = (SKSiHit *) fHitArray->At(jHit);
            if (!IsOhmicOnly(ohmicHit)
                    || ohmicHit->GetDetID() != junctionHit->GetDetID())
                continue;

            auto ohmicTime = FindHitTime(ohmicHit, true);
            Candidate candidate;
            candidate.junctionHit = junctionHit;
            candidate.ohmicHit = ohmicHit;
            candidate.junctionIndex = iHit;
            candidate.ohmicIndex = jHit;
            candidate.stripDistance = expectedStrip < 0 ? 0
                    : std::abs(ohmicHit->GetOhmicStrip()-expectedStrip);
            candidate.hasTime = std::isfinite(junctionTime) && std::isfinite(ohmicTime);
            if (candidate.hasTime)
                candidate.timeDistance = std::abs(junctionTime-ohmicTime);
            candidates.push_back(candidate);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.stripDistance != b.stripDistance)
            return a.stripDistance < b.stripDistance;
        if (a.hasTime != b.hasTime)
            return a.hasTime;
        if (a.timeDistance != b.timeDistance)
            return a.timeDistance < b.timeDistance;
        if (a.ohmicHit->GetEnergyOhmic() != b.ohmicHit->GetEnergyOhmic())
            return a.ohmicHit->GetEnergyOhmic() > b.ohmicHit->GetEnergyOhmic();
        if (a.junctionIndex != b.junctionIndex)
            return a.junctionIndex < b.junctionIndex;
        return a.ohmicIndex < b.ohmicIndex;
    });

    std::set<SKSiHit *> matchedJunctionHits;
    std::set<SKSiHit *> matchedOhmicHits;
    for (const auto &candidate : candidates) {
        if (matchedJunctionHits.count(candidate.junctionHit) != 0
                || matchedOhmicHits.count(candidate.ohmicHit) != 0)
            continue;
        candidate.junctionHit->SetOhmicStrip(candidate.ohmicHit->GetOhmicStrip());
        candidate.junctionHit->SetEnergyOhmic(candidate.ohmicHit->GetEnergyOhmic());
        matchedJunctionHits.insert(candidate.junctionHit);
        matchedOhmicHits.insert(candidate.ohmicHit);
    }

    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto hit = (SKSiHit *) fHitArray->At(iHit);
        if (hit == nullptr)
            continue;

        if (IsOhmicOnly(hit)) {
            if (matchedOhmicHits.count(hit) != 0
                    || !IsSingleSideHitAllowed(hit, true))
                fHitArray->RemoveAt(iHit);
            else
                hit->SetJunctionStrip(-1);
            continue;
        }

        auto detector = fSiliconArray->GetSiDetector(hit->GetDetID());
        if (detector != nullptr
                && detector->GetNumSides() >= 2
                && matchedJunctionHits.count(hit) == 0
                && !IsSingleSideHitAllowed(hit, false))
            fHitArray->RemoveAt(iHit);
    }
    fHitArray->Compress();
}

void LKSiHitPairMatchingTask::ClassifyJunctionHits()
{
    for (auto iHit=0; iHit<fHitArray->GetEntriesFast(); ++iHit) {
        auto hit = (SKSiHit *) fHitArray->At(iHit);
        if (hit == nullptr)
            continue;

        auto detector = fSiliconArray->GetSiDetector(hit->GetDetID());
        if (detector == nullptr)
            continue;

        auto pairID = FindPairDetectorID(detector);
        hit->SetIsEPairDetector(pairID >= 0);
        if (detector->IsEDetector()) {
            hit->SetIsEDetector(true);
            if (pairID >= 0)
                hit->SetdEDetID(pairID);
            continue;
        }

        auto dEDetID = hit->GetDetID();
        hit->SetIsEDetector(false);
        hit->SetdEDetID(dEDetID);
        hit->SetDetID(pairID);
        hit->SetdE(hit->GetEnergy());
        hit->SetdEOhmic(hit->GetEnergyOhmic());
        hit->SetRelativeZdE(hit->GetRelativeZ());
        hit->SetEnergy(0);
        hit->SetEnergyOhmic(0);
    }
}

void LKSiHitPairMatchingTask::MatchdEEPairs()
{
    auto numHits = fHitArray->GetEntriesFast();
    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto eHit = (SKSiHit *) fHitArray->At(iHit);
        if (eHit == nullptr || !eHit->IsEPairDetector()
                || !eHit->IsEDetector() || eHit->GetEnergy() <= 0
                || eHit->GetJunctionStrip() < 0)
            continue;

        SKSiHit *bestdEHit = nullptr;
        double bestDistance = std::numeric_limits<double>::max();
        for (auto jHit=0; jHit<numHits; ++jHit) {
            auto dEHit = (SKSiHit *) fHitArray->At(jHit);
            if (dEHit == nullptr || dEHit->IsEDetector() || dEHit->IsGrabbed()
                    || dEHit->GetJunctionStrip() < 0)
                continue;
            if (dEHit->GetDetID() != eHit->GetDetID()
                    || dEHit->GetdEDetID() != eHit->GetdEDetID())
                continue;

            auto distance = std::abs(eHit->GetRelativeZ()-dEHit->GetRelativeZdE());
            if (distance < bestDistance) {
                bestDistance = distance;
                bestdEHit = dEHit;
            }
        }

        if (bestdEHit == nullptr)
            continue;

        eHit->SetdE(bestdEHit->GetdE());
        eHit->SetdEOhmic(bestdEHit->GetdEOhmic());
        eHit->SetRelativeZdE(bestdEHit->GetRelativeZdE());
        bestdEHit->Grab();
    }

    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto hit = (SKSiHit *) fHitArray->At(iHit);
        if (hit != nullptr && hit->IsGrabbed())
            fHitArray->RemoveAt(iHit);
    }
    fHitArray->Compress();
}

void LKSiHitPairMatchingTask::Exec(Option_t *)
{
    auto numHitsBefore = fHitArray->GetEntriesFast();
    AttachOhmicHits();
    ClassifyJunctionHits();
    MatchdEEPairs();
    lk_info << "Matched SiHit objects: " << numHitsBefore << " -> "
            << fHitArray->GetEntriesFast() << endl;
}
