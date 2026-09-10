#ifndef LKSIHITPAIRMATCHINGTASK_HH
#define LKSIHITPAIRMATCHINGTASK_HH

#include "LKTask.h"
#include "TClonesArray.h"

#include <array>
#include <vector>

class LKSiliconArray;
class LKSiDetector;
class LKSiChannel;
class SKSiHit;

/**
 * Match the detector-local SiHit objects produced by
 * LKEnergyRestorationTask.
 *
 * The task first attaches an ohmic hit to each junction hit in the same
 * detector. Multiple hits are matched one-to-one in increasing TB distance.
 * For X6, the reconstructed junction position selects the corresponding
 * ohmic segment before the TB comparison. It then combines matched 12-ring
 * dE and E detector hits.
 */
class LKSiHitPairMatchingTask : public LKTask
{
    public:
        LKSiHitPairMatchingTask();
        virtual ~LKSiHitPairMatchingTask() {}

        bool Init() override;
        void Exec(Option_t *) override;

    private:
        bool IsOhmicOnly(const SKSiHit *hit) const;
        bool LoadSingleSideChannelPatterns();
        bool IsSingleSideHitAllowed(const SKSiHit *hit, bool ohmicSide) const;
        bool MatchesSingleSideChannelPattern(const LKSiChannel *channel) const;
        int FindPairDetectorID(const LKSiDetector *detector) const;
        int FindExpectedOhmicStrip(const SKSiHit *junctionHit, const LKSiDetector *detector) const;
        double FindHitTime(const SKSiHit *hit, bool ohmicSide) const;
        void AttachOhmicHits();
        void ClassifyJunctionHits();
        void MatchdEEPairs();

    private:
        LKSiliconArray *fSiliconArray = nullptr;
        TClonesArray *fSiChannelArray = nullptr;
        TClonesArray *fHitArray = nullptr;
        bool fKeepSingleSideHits = false;
        std::vector<std::array<int,4>> fSingleSideChannelPatterns; //!

    ClassDefOverride(LKSiHitPairMatchingTask, 3)
};

#endif
