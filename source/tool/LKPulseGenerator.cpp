#include "LKLogger.hpp"
#include "LKRun.hpp"
#include "LKPulseGenerator.hpp"
#include "TSystem.h"
#include <fstream>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
using namespace std;

ClassImp(LKPulseGenerator);

LKPulseGenerator* LKPulseGenerator::fInstance = nullptr;

LKPulseGenerator* LKPulseGenerator::GetPulseGenerator(LKParameterContainer *par) {
    if (fInstance == nullptr) {
        TString fileName;
        if (par -> CheckPar("pulserData"))
            fileName = par -> GetParString("pulserData");
        fInstance = new LKPulseGenerator(fileName);
    }
    return fInstance;
}

LKPulseGenerator* LKPulseGenerator::GetPulseGenerator(TString fileName) {
    if (fInstance == nullptr)
        fInstance = new LKPulseGenerator(fileName);
    return fInstance;
}

LKPulseGenerator::LKPulseGenerator(TString fileName)
{
    Initialize(fileName);
}

bool LKPulseGenerator::Initialize(TString fileName)
{
    if (fileName=="deltaFunction") {
        fIsDeltaFunction = true;
        return true;
    }

    if (fileName.IsNull())
        fileName = "pulser_464ns.dat";
    TString fileNameConfigured = LKRun::ConfigureDataPath(fileName,true,"$(KEBIPATH)/input/",false);
    if (fileNameConfigured.IsNull()) {
        lx_info << "Input pulser file: " << fileName << " is not found!" << endl; 
        return false;
    }

    lx_info << "Using pulser file: " << fileNameConfigured << endl;
    ifstream file(fileNameConfigured);
    string line;

    while (getline(file, line) && line.find("#") == 0) {}
    istringstream ss(line);
    ss >> fShapingTime >> fNumDataPoints >> fStepSize >> fNumAscending >> fNDFTbs;

    if (fNumDataPoints < 20 || fStepSize > 1) {
        lx_error << "Number of data points (" << fNumDataPoints << ") should be >= 20, fStepSize (" << fStepSize << " should be < 1." << endl;
        lx_error << "Check file: " << fileName << endl;
        return false;
    }

    fPulseData = new LKSamplePoint[fNumDataPoints];

    Double_t max = 0;
    for (Int_t iData = 0; iData < fNumDataPoints; iData++)
    {
        getline(file, line);
        if (line.find("#") == 0) {
            iData--;
            continue;
        }

        fPulseData[iData].Init(line);
        Double_t value = fPulseData[iData].fValue;
        if (iData == 0)
            fThresholdRatio = value;

        if (value > max) {
            max = value;
            fTbAtMax = iData * fStepSize;
        }
    }

    Double_t c = 1./max;
    Double_t valuePre = 0, valueCur = 0;
    fTbAtThreshold = 0;
    fTbAtTail = 0;

    for (Int_t iData = 0; iData < fNumDataPoints; iData++)
    {
        fPulseData[iData].fValue = c * fPulseData[iData].fValue;

        valuePre = valueCur;
        valueCur = fPulseData[iData].fValue;

        if (fTbAtThreshold == 0 && valueCur > fThresholdRatio)
        {
            fTbAtThreshold = iData * fStepSize;
            Int_t next = iData + 1/fStepSize;
            fThresholdTbStep = fPulseData[next].fValue - fPulseData[iData].fValue;
        }

        if (fTbAtTail == 0 && valueCur < valuePre && valueCur < 0.1)
            fTbAtTail = iData * fStepSize;
    }

    file.close();

    return true;
}

    Double_t 
LKPulseGenerator::Pulse(Double_t x, Double_t amp, Double_t tb0)
{
    if (fIsDeltaFunction)
    {
        if (x>=tb0-1 && x<tb0) {
            auto val = amp * (x - tb0 + 1);
            //return val;
            return amp;
        }
        else if (x>=tb0 && x<tb0+1) {
            auto val = - amp * (x - tb0 - 1);
            //return val;
            return amp;
        }
        else
            return 0;
    }

    Double_t tb = x - tb0;
    if (tb < 0) 
        return 0;

    Int_t tbInStep = tb / fStepSize;
    if (tbInStep > fNumDataPoints - 2) 
        return 0;

    Double_t r = (tb / fStepSize) - tbInStep;
    Double_t val = r * fPulseData[tbInStep + 1].fValue + (1 - r) * fPulseData[tbInStep].fValue;

    return amp * val;
}

    Double_t 
LKPulseGenerator::PulseF1(Double_t *x, Double_t *par)
{
    if (fIsDeltaFunction)
    {
        if (x[0]>=par[1]-1 && x[0]<par[1]) {
            auto val = par[0] * (x[0] - par[1] + 1);
            //return val;
            return par[0];
        }
        else if (x[0]>=par[1] && x[0]<par[1]+1) {
            auto val = - par[0] * (x[0] - par[1] - 1);
            //return val;
            return par[0];
        }
        else
            return 0;
    }

    Double_t tb = x[0] - par[1];
    if (tb < 0) 
        return 0;

    Int_t tbInStep = tb / fStepSize;
    if (tbInStep > fNumDataPoints - 2)
        return 0;

    Double_t r = (tb / fStepSize) - tbInStep;
    Double_t val = r * fPulseData[tbInStep + 1].fValue + (1 - r) * fPulseData[tbInStep].fValue;

    return par[0] * val;
}

    TF1*
LKPulseGenerator::GetPulseFunction(TString name)
{
    if (name.IsNull()) 
        name = Form("STPulse_%d", fNumF1++);

    if (fIsDeltaFunction)
    {
        //TF1* f1 = new TF1(name, "(x>=[1]-1 && x<[1])*[0]*(x-[1]+1) + (x>=[1] && x<[1]+1)*(-[0])*(x-[1]-1)", 0, 512);
        TF1* f1 = new TF1(name, "(x>=[1]-1 && x<[1])*[0] + (x>=[1] && x<[1]+1)*(-[0])", 0, 512);
        f1 -> SetNpx(512);
        return f1;
    }

    TF1* f1 = new TF1(name, this, &LKPulseGenerator::PulseF1, 0, 512, 2, "LKPulseGenerator", "PulseF1");
    return f1;
}

Int_t  LKPulseGenerator::GetShapingTime()     { return fShapingTime;     }
Double_t  LKPulseGenerator::GetTbAtThreshold()   { return fTbAtThreshold;   }
Double_t  LKPulseGenerator::GetTbAtTail()        { return fTbAtTail;        }
Double_t  LKPulseGenerator::GetTbAtMax()         { return fTbAtMax;         }
Int_t  LKPulseGenerator::GetNumAscending()    { return fNumAscending;    }
Double_t  LKPulseGenerator::GetThresholdTbStep() { return fThresholdTbStep; }
Int_t  LKPulseGenerator::GetNumDataPoints()   { return fNumDataPoints;   }
Double_t  LKPulseGenerator::GetStepSize()        { return fStepSize;        }
Int_t  LKPulseGenerator::GetNDFTbs()          { return fNDFTbs;          }

LKSamplePoint **LKPulseGenerator::GetPulseData()  { return &fPulseData; }

    void
LKPulseGenerator::Print()
{
    if (fIsDeltaFunction) {
        lx_info << "[LKPulseGenerator INFO] DeltaFunction" << endl;
        return;
    }

    lx_info << "[LKPulseGenerator INFO]" << endl;
    lx_info << " == Shaping time : " << fShapingTime << " ns" << endl;
    lx_info << " == Number of data points : " << fNumDataPoints << endl;
    lx_info << " == Step size between data points : " << fStepSize << endl;
    lx_info << " == Threshold for one timebucket step : " << fThresholdTbStep << endl; 
    lx_info << " == Number of timebucket while rising : " << fNumAscending << endl;
    lx_info << " == Timebucket at threshold (" << setw(3) << fThresholdRatio << " of peak) : " << fTbAtThreshold << endl; 
    lx_info << " == Timebucket at peak : " << fTbAtMax << endl; 
    lx_info << " == Timebucket difference from threshold to peak : " << fTbAtMax - fTbAtThreshold << endl; 
    lx_info << " == Number of degree of freedom : " << fNDFTbs << endl;
}

void LKPulseGenerator::SavePulseData(TString name, Bool_t smoothTail)
{
    Double_t max = 0;
    for (Int_t iData = 0; iData < fNumDataPoints; iData++) {
        if (fPulseData[iData].fValue > max)
            max = fPulseData[iData].fValue;
    }

    Double_t c = 1/max;
    if (max != 1) {
        for (Int_t iData = 0; iData < fNumDataPoints; iData++)
            fPulseData[iData].fValue = c * fPulseData[iData].fValue;
    }

    if (smoothTail) {
        if (fTailFunction == nullptr)
            fTailFunction = new TF1("tail", "x*landau(0)", fTbAtTail - 1, fNumDataPoints - 1);

        fTailFunction -> SetParameters(1, 4, 1);

        if (fTailGraph == nullptr)
            fTailGraph = new TGraph();

        fTailGraph -> Clear();
        fTailGraph -> Set(0);

        Int_t iTailStart = (Int_t)(fTbAtTail/fStepSize);
        for (Int_t iData = iTailStart; iData < fNumDataPoints; iData++)
            fTailGraph -> SetPoint(fTailGraph -> GetN(), iData * fStepSize, fPulseData[iData].fValue);

        fTailGraph -> Fit(fTailFunction, "R");

        for (Int_t iData = iTailStart; iData < fNumDataPoints; iData++)
            fPulseData[iData].fValue = fTailFunction -> Eval(iData * fStepSize);
    }

    ofstream file(name.Data());
    file << "#(shaping time) (number of data points) (step size) (rising tb threshold number) (default ndf)" << endl;
    file << fShapingTime << " " << fNumDataPoints << " " << fStepSize << " " << fNumAscending << " " << fNDFTbs << endl;

    file << "#(value) (rms) (total weight)" << endl;
    for (Int_t iData = 0; iData < fNumDataPoints; iData++) {
        LKSamplePoint sample = fPulseData[iData];
        file << sample.GetSummary() << endl;
    }

    file.close();
}
