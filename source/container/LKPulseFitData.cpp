#include "LKPulseFitData.h"

LKPulseFitData::LKPulseFitData()
{
}

void LKPulseFitData::Clear(Option_t *option)
{
    fHitIndex = -1;

    fIsSaturated = false;
    fNumHitsInChannel = 0;

    fNDF = 0;
    fChi2 = 0;
    fFitRange1 = -1;
    fFitRange2 = -1;

    fTb = -1;
    fWidth = -1;
    fIntegral = -1;
    fAmplitude = -1;

    fSlope = -1;
    fSlopePar0 = -1;
    fSlopePar1 = -1;
    fSlopeAmplitude = -1;
}
