#include "LKSiHitPairMatchingTask.h"

#include "LKLogger.h"
#include "LKRun.h"
#include "LKSiDetector.h"
#include "LKSiliconArray.h"
#include "SKSiHit.h"

#include <cmath>
#include <limits>

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

    return true;
}

bool LKSiHitPairMatchingTask::IsOhmicOnly(const SKSiHit *hit) const
{
    return hit != nullptr && hit->GetEnergyOhmic() > 0
        && hit->GetEnergyLeft() <= 0 && hit->GetEnergyRight() <= 0;
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

SKSiHit *LKSiHitPairMatchingTask::FindOhmicHit(
        const SKSiHit *junctionHit, LKSiDetector *detector) const
{
    if (junctionHit == nullptr || detector == nullptr)
        return nullptr;

    int expectedStrip = -1;
    auto numOhmicStrips = detector->GetNumOhmicStrips();
    if (detector->GetUseJunctionUD() && numOhmicStrips > 0
            && junctionHit->GetRelativeZ() >= -1
            && junctionHit->GetRelativeZ() <= 1) {
        // X6 convention: relativeZ=-1 is the low end and +1 is the high end.
        expectedStrip = (int) std::floor(
                0.5 * (1-junctionHit->GetRelativeZ()) * numOhmicStrips);
        if (expectedStrip < 0)
            expectedStrip = 0;
        if (expectedStrip >= numOhmicStrips)
            expectedStrip = numOhmicStrips-1;
    }

    SKSiHit *bestHit = nullptr;
    double bestScore = std::numeric_limits<double>::max();
    for (auto iHit=0; iHit<fHitArray->GetEntriesFast(); ++iHit) {
        auto hit = (SKSiHit *) fHitArray->At(iHit);
        if (!IsOhmicOnly(hit) || hit->GetDetID() != junctionHit->GetDetID())
            continue;

        double score = -hit->GetEnergyOhmic();
        if (expectedStrip >= 0)
            score = 1.e9*std::abs(hit->GetOhmicStrip()-expectedStrip)
                  - hit->GetEnergyOhmic();
        if (score < bestScore) {
            bestScore = score;
            bestHit = hit;
        }
    }
    return bestHit;
}

void LKSiHitPairMatchingTask::AttachOhmicHits()
{
    auto numHits = fHitArray->GetEntriesFast();
    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto junctionHit = (SKSiHit *) fHitArray->At(iHit);
        if (junctionHit == nullptr || IsOhmicOnly(junctionHit)
                || junctionHit->GetEnergy() <= 0)
            continue;

        auto detector = fSiliconArray->GetSiDetector(junctionHit->GetDetID());
        auto ohmicHit = FindOhmicHit(junctionHit, detector);
        if (ohmicHit != nullptr) {
            junctionHit->SetOhmicStrip(ohmicHit->GetOhmicStrip());
            junctionHit->SetEnergyOhmic(ohmicHit->GetEnergyOhmic());
        }
    }

    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto hit = (SKSiHit *) fHitArray->At(iHit);
        if (IsOhmicOnly(hit))
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
                || !eHit->IsEDetector() || eHit->GetEnergy() <= 0)
            continue;

        SKSiHit *bestdEHit = nullptr;
        double bestDistance = std::numeric_limits<double>::max();
        for (auto jHit=0; jHit<numHits; ++jHit) {
            auto dEHit = (SKSiHit *) fHitArray->At(jHit);
            if (dEHit == nullptr || dEHit->IsEDetector() || dEHit->IsGrabbed())
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
