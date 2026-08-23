#include "LKPulseShapeAnalysisTask.h"
//#include "LKEventHeader.h"
#include "GETChannel.h"
#include "LKHit.h"
#include "LKChannelAnalyzer.h"
#include "TSystem.h"

ClassImp(LKPulseShapeAnalysisTask);

LKPulseShapeAnalysisTask::LKPulseShapeAnalysisTask()
{
    fName = "LKPulseShapeAnalysisTask";
}

bool LKPulseShapeAnalysisTask::Init()
{
    lk_info << "Initializing LKPulseShapeAnalysisTask" << std::endl;

    fPar -> UpdatePar(fPulseParameterFile, "LKPulseShapeAnalysisTask/pulseParameterFile # LKPulse parameter file. Leave empty to use $LILAK_PATH/common/pulseReference.mac.");
    fPar -> UpdatePar(fFixPulseFunctionAlpha, "LKPulseShapeAnalysisTask/fixPulseFunctionAlpha false # Fix LKPulse function alpha during pulse fitting.");
    fPar -> UpdatePar(fFixPulseFunctionTau, "LKPulseShapeAnalysisTask/fixPulseFunctionTau false # Fix LKPulse function tau during pulse fitting.");
    fPar -> UpdatePar(fUseRootPulseFit, "LKPulseShapeAnalysisTask/useRootPulseFit true # Use TH1::Fit with LKPulse TF1; fallback to manual chi2 if it fails.");

    if (fChannelAnalyzer==nullptr)
    {
        if (fRun->GetDetectorSystem()->GetNumPlanes()==0) {
            lk_warning << "Channel analyzer sould be set!" << endl;
            lk_warning << "Using default channel analyzer" << endl;
            fChannelAnalyzer = new LKChannelAnalyzer();
        }
        else {
            fDetectorPlane = fRun -> GetDetectorPlane();
            if (fDetectorPlane!=nullptr)
                fChannelAnalyzer = fDetectorPlane -> GetChannelAnalyzer();
        }
    }

    if (fChannelAnalyzer==nullptr)
        fChannelAnalyzer = new LKChannelAnalyzer();

    if (fDetectorPlane!=nullptr)
        fUsingDetectorPlane = true;

    if (fPulseParameterFile.IsNull())
        fPulseParameterFile = TString(gSystem -> Getenv("LILAK_PATH")) + "/common/pulseReference.mac";
    fChannelAnalyzer -> SetPulse(fPulseParameterFile);
    fChannelAnalyzer -> FixPulseFunctionAlpha(fFixPulseFunctionAlpha);
    fChannelAnalyzer -> FixPulseFunctionTau(fFixPulseFunctionTau);
    fChannelAnalyzer -> SetUseRootPulseFit(fUseRootPulseFit);

    fChannelAnalyzer -> Print();

    fChannelArray = fRun -> GetBranchA("RawData","GETChannel");
    fHitArray = fRun -> RegisterBranchA("Hit","LKHit",100);

    return true;
}

void LKPulseShapeAnalysisTask::Exec(Option_t *option)
{
    fHitArray -> Clear("C");

    int countHits = 0;

    int numChannel = fChannelArray -> GetEntriesFast();
    for (int iChannel = 0; iChannel < numChannel; ++iChannel)
    {
        auto channel = (GETChannel *) fChannelArray -> At(iChannel);
        auto channelID = channel -> GetChannelID();
        if (channelID<0)
            channelID = iChannel;
        auto chDetType = channel -> GetDetType();
        auto padID = channel -> GetPadID();
        auto cobo = channel -> GetCobo();
        auto asad = channel -> GetAsad();
        auto aget = channel -> GetAget();
        auto chan = channel -> GetChan();
        auto data = channel -> GetWaveformY();
        if (fUsingDetectorPlane&&padID<0)
            padID = fDetectorPlane -> FindPadID(cobo,asad,aget,chan);

        fChannelAnalyzer -> Analyze(data);

        auto numRecoHits = fChannelAnalyzer -> GetNumHits();
        for (auto iHit=0; iHit<numRecoHits; ++iHit)
        {
            auto tb        = fChannelAnalyzer -> GetTbHit(iHit);
            auto amplitude = fChannelAnalyzer -> GetAmplitude(iHit);
            auto chi2NDF   = fChannelAnalyzer -> GetChi2NDF(iHit);
            auto ndf       = fChannelAnalyzer -> GetNDF(iHit);
            auto pedestal  = fChannelAnalyzer -> GetPedestal();

            LKHit* hit = (LKHit*) fHitArray -> ConstructedAt(countHits);
            hit -> SetHitID(countHits);
            hit -> SetChannelID(channelID);
            hit -> SetPadID(padID);
            hit -> SetPositionError(TVector3(0,0,0));
            hit -> SetCharge(amplitude);
            hit -> SetPedestal(pedestal);
            hit -> SetTb(tb);

            if (fUsingDetectorPlane&&padID>=0) {
                TVector3 posReco;
                TVector3 posError;
                double driftLengthDummy;
                fDetectorPlane -> DriftElectronBack(padID, tb, posReco, driftLengthDummy);
                posError = fDetectorPlane -> GetPositionError(padID);
                hit -> SetPosition(posReco);
                hit -> SetPositionError(posError);
            }

            countHits++;
        }
    }

    lk_info << "Found " << countHits << " hits" << endl;
}

bool LKPulseShapeAnalysisTask::EndOfRun()
{
    return true;
}
