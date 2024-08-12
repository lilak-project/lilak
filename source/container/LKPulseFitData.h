#ifndef LKPULSEFITDATA_HH
#define LKPULSEFITDATA_HH

#include "LKContainer.h"

class LKPulseFitData : public LKContainer
{
    public:
        LKPulseFitData();
        virtual ~LKPulseFitData() {}

        virtual void Clear(Option_t *option = "");

        double GetChi2NDF() const { return fChi2/fNDF; }

    public:
        int fHitIndex; ///< hit index of hit TClones array

        bool fIsSaturated; ///< saturated pulse?
        int fNumHitsInChannel; ///< number of reconstructed hits in same buffer including this

        int fNDF; ///< number of degree of freedom
        double fChi2; ///< chi square of fit
        double fFitRange1; ///< fit range 1
        double fFitRange2; ///< fit range 2

        double fTb; ///< reconstructed time bucket
        double fWidth; ///< reconstructed slope
        double fIntegral; ///< reconstructed integral
        double fAmplitude; ///< reconstructed amplitude

        double fSlope; ///< reconstructed slope
        double fSlopePar0; ///< extra slope parameters
        double fSlopePar1; ///< extra slope parameters
        double fSlopeAmplitude; ///< reconstructed amplitude from slope correlation

    ClassDef(LKPulseFitData, 1)
};

#endif
