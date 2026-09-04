#ifndef LKSIHITPAIRMATCHINGTASK_HH
#define LKSIHITPAIRMATCHINGTASK_HH

#include "LKTask.h"
#include "TClonesArray.h"

class LKSiliconArray;
class LKSiDetector;
class SKSiHit;

/**
 * Match the detector-local SiHit objects produced by
 * LKEnergyRestorationTask.
 *
 * The task first attaches an ohmic hit to each junction hit in the same
 * detector.  For X6, the reconstructed junction position selects the
 * corresponding ohmic segment.  It then combines matched 12-ring dE and E
 * detector hits without discarding unmatched junction hits.
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
        int FindPairDetectorID(const LKSiDetector *detector) const;
        SKSiHit *FindOhmicHit(const SKSiHit *junctionHit, LKSiDetector *detector) const;
        void AttachOhmicHits();
        void ClassifyJunctionHits();
        void MatchdEEPairs();

    private:
        LKSiliconArray *fSiliconArray = nullptr;
        TClonesArray *fHitArray = nullptr;

    ClassDefOverride(LKSiHitPairMatchingTask, 1)
};

#endif
