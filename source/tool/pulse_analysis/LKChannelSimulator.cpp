#include "LKChannelSimulator.h"
#include "LKChannelSimulator.h"
#include "TRandom.h"
#include "LKCompiled.h"
#include "LKLogger.h"
#include <iostream>
using namespace std;

ClassImp(LKChannelSimulator);

LKChannelSimulator::LKChannelSimulator()
{
    fPulse = new LKPulse(TString(LILAK_PATH)+"/common/pulseReference.root");

    fBackGroundLevel = fPulse -> GetBackGroundLevel();
    fPedestalFluctuationLevel = fPulse -> GetFluctuationLevel();
    fFloorRatio = fPulse -> GetFloorRatio();
}

void LKChannelSimulator::Print(Option_t *option) const
{
    e_info << "LKChannelSimulator" << endl;
    e_info << "Buffer" << endl;
    e_cout << "      y-Max:             " << fYMax << endl;
    e_cout << "      tb-Max:            " << fTbMax << endl;
    e_info << "General Background (BG)" << endl;
    e_cout << "      BG uevel:          " << fBackGroundLevel << endl;
    e_info << "Pedestal Fluctuation (PF)" << endl;
    e_cout << "      PF level:          " << fPedestalFluctuationLevel << endl;
    e_cout << "      PF scale:          " << fPedestalFluctuationScale << endl;
    e_cout << "      PF length:         " << fPedestalFluctuationLength << endl;
    e_cout << "      PF length-error:   " << fPedestalFluctuationLengthError << endl;
    e_info << "Smoothing" << endl;
    e_cout << "      number of smoothing group: " << fNumSmoothing << endl;
    e_cout << "      smoothing Length:  " << fSmoothingLength << endl;
    e_info << "Pulse" << endl;
    e_cout << "      pulse error scale: " << fPulseErrorScale << endl;
    if (fPulse!=nullptr)
        fPulse -> Print();
}

void LKChannelSimulator::SetPulse(const char* fileName)
{
    fPulse = new LKPulse(fileName);

    fBackGroundLevel = fPulse -> GetBackGroundLevel();
    fPedestalFluctuationLevel = fPulse -> GetFluctuationLevel();
    fFloorRatio = fPulse -> GetFloorRatio();
}

void LKChannelSimulator::AddPedestal()
{
    double pedestalFluctuationLevel = fPedestalFluctuationScale * fPedestalFluctuationLevel;
    for (auto tb=0; tb<fTbMax; ++tb)
        fBuffer[tb] = gRandom -> Gaus(0, pedestalFluctuationLevel);

    Smoothing(fTbMax,fSmoothingLength,fNumSmoothing);
}

void LKChannelSimulator::AddFluctuatingPedestal()
{
    int pmFluctuation = 1;
    double pedestalFluctuationLevel = fPedestalFluctuationScale * fPedestalFluctuationLevel;
    int valuePointer = gRandom -> Gaus(0, pedestalFluctuationLevel);
    valuePointer = fBackGroundLevel + pmFluctuation*(valuePointer);
    pmFluctuation = -pmFluctuation;

    int tbPointer = 0;
    fBuffer[tbPointer++] = valuePointer;

    while (tbPointer<fTbMax)
    {
        int tbFlucLength = gRandom -> Gaus(fPedestalFluctuationLength,fPedestalFluctuationLength*fPedestalFluctuationLengthError);
        tbFlucLength = abs(tbFlucLength);
        if (tbFlucLength==0) tbFlucLength = 1;

        int valueTarget = gRandom -> Gaus(0, pedestalFluctuationLevel);
        valueTarget = fBackGroundLevel + pmFluctuation*(valueTarget);
        pmFluctuation = -pmFluctuation;

        int dValueTotal = valueTarget - valuePointer;
        int dValuePerLength = dValueTotal/tbFlucLength;
        if (tbPointer+tbFlucLength>fTbMax)
            tbFlucLength = fTbMax - tbPointer;

        for (int iTb=0; iTb<tbFlucLength-1; ++iTb)
        {
            int dValue = gRandom -> Gaus(dValuePerLength,0.5*dValuePerLength);
            valuePointer = valuePointer + dValue;
            fBuffer[tbPointer++] = valuePointer;
        }

        int dValueLast = valueTarget - valuePointer;
        valuePointer = valuePointer + dValueLast;
        fBuffer[tbPointer++] = valuePointer;

        //lk_debug << tbPointer << " " << tbFlucLength << " " << valueTarget << endl;
    }

    Smoothing(fTbMax,fSmoothingLength,fNumSmoothing);
}

void LKChannelSimulator::AddHit(double tb0, double amplitude)
{
    for (auto tb=0; tb<fTbMax; ++tb)
    {
        double value = fPulse -> EvalTb(tb, tb0, amplitude);
        if (value>amplitude*fFloorRatio && fPulseErrorScale>0)
        {
            double error = gRandom -> Gaus(0, fPulse->Error0Tb(tb,tb0,fPulseErrorScale*amplitude));
            value = value + error;
        }
        fBuffer[tb] = fBuffer[tb] + value;
    }

    for (auto tb=0; tb<fTbMax; ++tb) {
        if (fBuffer[tb] > fYMax)
            fBuffer[tb] = fYMax;
    }

    if (fCutBelow0)
        for (auto tb=0; tb<fTbMax; ++tb) {
            if (fBuffer[tb] < 0)
                fBuffer[tb] = 0;
        }
}

void LKChannelSimulator::Smoothing(int n, int smoothingLevel, int numSmoothing)
{
    for (int it=0; it<numSmoothing; ++it)
        for (int i=0; i<n; i++)
        {
            double sum = 0.;
            int count = 0;

            for (int j = i-smoothingLevel; j<=i+smoothingLevel; j++) {
                if (j>=0 && j<n) {
                    sum += fBuffer[j];
                    count++;
                }
            }
            fBuffer[i] = sum / count;
        }
}

void LKChannelSimulator::FillHist(TH1* hist)
{
    for (auto tb=0; tb<fTbMax; ++tb)
        hist -> SetBinContent(tb+1,fBuffer[tb]);
}

TH1D* LKChannelSimulator::GetHist(TString name)
{
    auto hist = new TH1D(name,"",fTbMax,0,fTbMax);
    FillHist(hist);
    return hist;
}
