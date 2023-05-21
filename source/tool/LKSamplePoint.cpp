#include "LKSamplePoint.h"

#include <iostream>

ClassImp(LKSamplePoint);

LKSamplePoint::LKSamplePoint()
{
    Init(-999, 0, 1);
}

LKSamplePoint::LKSamplePoint(LKSamplePoint &sample)
{
    Init(sample.fValue, sample.fRMS, sample.fWeightSum);
}

LKSamplePoint::LKSamplePoint(Double_t v, Double_t rms, Double_t w)
{
    Init(v, rms, w);
}

LKSamplePoint::~LKSamplePoint()
{
}

    void 
LKSamplePoint::Print()
{
    cout << " LKSamplePoint " << fValue << " | " << fRMS << " | " << fWeightSum << endl;
}

    void 
LKSamplePoint::Init(Double_t v, Double_t rms, Double_t w)
{
    fValue = v;
    fRMS = rms;
    fWeightSum = w;
}

    void 
LKSamplePoint::Init(string line)
{
    istringstream ss(line);
    ss >> fValue >> fRMS >> fWeightSum;
}

    void 
LKSamplePoint::Update(Double_t v, Double_t w)
{
    fValue = (fWeightSum * fValue + w * v) / (fWeightSum + w);
    fRMS = (fWeightSum * fRMS) / (fWeightSum + w)
        + w * (fValue - v) * (fValue - v) / fWeightSum;

    fWeightSum += w;
}

    TString
LKSamplePoint::GetSummary()
{
    return Form("%.6g %.6g %.6g", fValue, fRMS, fWeightSum);
}
