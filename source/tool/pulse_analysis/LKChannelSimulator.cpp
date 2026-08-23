#include "LKChannelSimulator.h"
#include "TRandom.h"
#include "LKLogger.h"
#include "LKParameterContainer.h"
#include <iostream>
using namespace std;

ClassImp(LKChannelSimulator);

LKChannelSimulator::LKChannelSimulator()
{
    fPulse = new LKPulse();
    fFloorRatio = fPulse -> GetFloorRatio();
}

void LKChannelSimulator::Print(Option_t *option) const
{
    e_info << "LKChannelSimulator" << endl;
    e_info << "Buffer" << endl;
    e_cout << "      y-Max:             " << fYMax << endl;
    e_cout << "      tb-Max:            " << fTbMax << endl;
    e_info << "General Background (BG)" << endl;
    e_cout << "      BG level:          " << fBackGroundLevel << endl;
    e_cout << "      BG sigma:          " << fBackGroundLevelSigma << endl;
    e_cout << "      BG range:          " << fBackGroundLevelMin << " - " << fBackGroundLevelMax << endl;
    e_info << "Pedestal Fluctuation (PF)" << endl;
    e_cout << "      PF level:          " << fPedestalFluctuationLevel << endl;
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
    SetPulse(new LKPulse(fileName));
}

void LKChannelSimulator::SetPulse(LKParameterContainer* par, TString groupName)
{
    SetPulse(new LKPulse(par, groupName));
}

void LKChannelSimulator::SetPulse(LKPulse* pulse)
{
    if (pulse==nullptr)
        return;

    fPulse = pulse;

    SetBackGroundLevel(fPulse -> GetBackGroundLevel());
    fPedestalFluctuationLevel = fPulse -> GetFluctuationLevel();
    fFloorRatio = fPulse -> GetFloorRatio();
}

void LKChannelSimulator::SetBackGroundLevel(double mean, double sigma, double min, double max)
{
    fBackGroundLevel = mean;
    fBackGroundLevelSigma = sigma;
    fBackGroundLevelMin = min;
    fBackGroundLevelMax = max;
}

void LKChannelSimulator::SetUsePulseFunction(bool value)
{
    if (fPulse==nullptr)
        fPulse = new LKPulse();
    fPulse -> SetUsePulseFunction(value);
    fFloorRatio = fPulse -> GetFloorRatio();
}

void LKChannelSimulator::SetPulseFunctionParameters(double baseline, double peak, double t0, double alpha, double tau)
{
    if (fPulse==nullptr)
        fPulse = new LKPulse();
    fPulse -> SetPulseFunctionParameters(baseline, peak, t0, alpha, tau);
    fPulse -> SetUsePulseFunction(true);
    fFloorRatio = fPulse -> GetFloorRatio();
}

void LKChannelSimulator::SetPulseFunctionRange(double tbMin, double tbMax)
{
    if (fPulse==nullptr)
        fPulse = new LKPulse();
    fPulse -> SetPulseFunctionRange(tbMin, tbMax);
    fPulse -> SetUsePulseFunction(true);
    fFloorRatio = fPulse -> GetFloorRatio();
}

void LKChannelSimulator::UpdatePulseFunctionParameters(LKParameterContainer* par, TString groupName)
{
    if (par==nullptr)
        return;

    if (fPulse==nullptr)
        fPulse = new LKPulse();

    fPulse -> UpdatePulseFunctionParameters(par, "LKPulse");

    bool usePulseFunction = fPulse -> UsesPulseFunction();
    double baseline = fPulse -> GetPulseFunctionBaseline();
    double peak = fPulse -> GetPulseFunctionPeak();
    double t0 = fPulse -> GetPulseFunctionT0();
    double alpha = fPulse -> GetPulseFunctionAlpha();
    double tau = fPulse -> GetPulseFunctionTau();
    double tbMin = fPulse -> GetPulseFunctionTbMin();
    double tbMax = fPulse -> GetPulseFunctionTbMax();

    par -> UpdatePar(usePulseFunction, groupName+"/UsePulseFunction true # If true, AddHit uses LKPulse analytic pulse function; if false, use LKPulse reference graph.");
    par -> UpdatePar(baseline, groupName+"/PulseFunctionBaseline 0.000247812 # Normalized baseline offset from pulse-reference fit; subtracted during simulation.");
    par -> UpdatePar(peak, groupName+"/PulseFunctionPeak 1.00117 # Normalized peak height of the fitted gamma-like pulse.");
    par -> UpdatePar(t0, groupName+"/PulseFunctionT0 -3.35106 # (tb) Function start time relative to hit time.");
    par -> UpdatePar(alpha, groupName+"/PulseFunctionAlpha 3.4317 # Shape exponent of the fitted gamma-like pulse.");
    par -> UpdatePar(tau, groupName+"/PulseFunctionTau 4.54546 # (tb) Decay time scale of the fitted gamma-like pulse.");
    par -> UpdatePar(tbMin, groupName+"/PulseFunctionTbMin -20. # (tb) Lower valid range relative to hit time.");
    par -> UpdatePar(tbMax, groupName+"/PulseFunctionTbMax 81. # (tb) Upper valid range relative to hit time.");

    fPulse -> SetUsePulseFunction(usePulseFunction);
    fPulse -> SetPulseFunctionParameters(baseline, peak, t0, alpha, tau);
    fPulse -> SetPulseFunctionRange(tbMin, tbMax);
    fFloorRatio = fPulse -> GetFloorRatio();
}

TF1* LKChannelSimulator::GetPulseFunction(TString name, bool subtractBaseline)
{
    if (fPulse==nullptr)
        fPulse = new LKPulse();
    return fPulse -> GetPulseFunction(name, subtractBaseline);
}

void LKChannelSimulator::AddPedestal()
{
    double backGroundLevel = SampleBackGroundLevel();
    double pedestalFluctuationLevel = fPedestalFluctuationLevel;
    for (auto tb=0; tb<fTbMax; ++tb)
        fBuffer[tb] = gRandom -> Gaus(backGroundLevel, pedestalFluctuationLevel);

    Smoothing(fTbMax,fSmoothingLength,fNumSmoothing);
}

void LKChannelSimulator::AddFluctuatingPedestal()
{
    int pmFluctuation = 1;
    double backGroundLevel = SampleBackGroundLevel();
    double pedestalFluctuationLevel = fPedestalFluctuationLevel;
    int valuePointer = gRandom -> Gaus(0, pedestalFluctuationLevel);
    valuePointer = backGroundLevel + pmFluctuation*(valuePointer);
    pmFluctuation = -pmFluctuation;

    int tbPointer = 0;
    fBuffer[tbPointer++] = valuePointer;

    while (tbPointer<fTbMax)
    {
        int tbFlucLength = gRandom -> Gaus(fPedestalFluctuationLength,fPedestalFluctuationLength*fPedestalFluctuationLengthError);
        tbFlucLength = abs(tbFlucLength);
        if (tbFlucLength==0) tbFlucLength = 1;

        int valueTarget = gRandom -> Gaus(0, pedestalFluctuationLevel);
        valueTarget = backGroundLevel + pmFluctuation*(valueTarget);
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

double LKChannelSimulator::SampleBackGroundLevel()
{
    if (fBackGroundLevelSigma<0)
        return fBackGroundLevel;

    bool useRange = (fBackGroundLevelMin < fBackGroundLevelMax);
    for (auto iTry=0; iTry<10000; ++iTry)
    {
        double value = gRandom -> Gaus(fBackGroundLevel, fBackGroundLevelSigma);
        if (!useRange || (value>=fBackGroundLevelMin && value<=fBackGroundLevelMax))
            return value;
    }

    if (fBackGroundLevel < fBackGroundLevelMin)
        return fBackGroundLevelMin;
    if (fBackGroundLevel > fBackGroundLevelMax)
        return fBackGroundLevelMax;
    return fBackGroundLevel;
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
