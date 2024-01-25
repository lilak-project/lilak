#include "LKPulseShapeAnalysisTask.h"
//#include "LKEventHeader.h"
#include "GETChannel.h"
#include "LKHit.h"

ClassImp(LKPulseShapeAnalysisTask);

LKPulseShapeAnalysisTask::LKPulseShapeAnalysisTask()
{
    fName = "LKPulseShapeAnalysisTask";
}

bool LKPulseShapeAnalysisTask::Init()
{
    lk_info << "Initializing LKPulseShapeAnalysisTask" << std::endl;

    fDetectorPlane = fRun -> GetDetectorPlane();
    fChannelAnalyzer = fDetectorPlane -> GetChannelAnalyzer();

    fChannelArray = fRun -> GetBranchA("RawData");
    fHitArrayCenter = fRun -> RegisterBranchA("Hit","LKHit",100);
    fEventHeaderHolder = fRun -> KeepBranchA("EventHeader");

    return true;
}

void LKPulseShapeAnalysisTask::Exec(Option_t *option)
{
    fHitArrayCenter -> Clear("C");

    //auto eventHeader = (LKEventHeader*) fEventHeaderHolder -> At(0);
    //if (eventHeader->IsGoodEvent()==false)
    //    return;

    int countHits = 0;

    int numChannel = fChannelArray -> GetEntriesFast();
    for (int iChannel = 0; iChannel < numChannel; ++iChannel)
    {
        auto channel = (GETChannel *) fChannelArray -> At(iChannel);
        auto chDetType = channel -> GetDetType();
        auto cobo = channel -> GetCobo();
        auto asad = channel -> GetAsad();
        auto aget = channel -> GetAget();
        auto chan = channel -> GetChan();
        auto padID = channel -> GetChan2();
        auto data = channel -> GetWaveformY();
        if (padID<0)
        {
            auto padID2 = fDetectorPlane -> FindPadID(cobo,asad,aget,chan);
            padID = padID2;
        }
        if (padID<0)
            continue;

        for (auto tb=0; tb<512; ++tb)
            fBuffer[tb] = double(data[tb]);
        fChannelAnalyzer -> Analyze(fBuffer);

        auto pad = fDetectorPlane -> GetPad(padID);

        auto numRecoHits = fChannelAnalyzer -> GetNumHits();
        for (auto iHit=0; iHit<numRecoHits; ++iHit)
        {
            auto tb        = fChannelAnalyzer -> GetTbHit(iHit);
            auto amplitude = fChannelAnalyzer -> GetAmplitude(iHit);
            auto chi2NDF   = fChannelAnalyzer -> GetChi2NDF(iHit);
            auto ndf       = fChannelAnalyzer -> GetNDF(iHit);
            auto pedestal  = fChannelAnalyzer -> GetPedestal();

            fDetectorPlane -> DriftElectronBack(pad, tb, fPosReco, fDriftLength);

            LKHit* hit = (LKHit*) fHitArrayCenter -> ConstructedAt(countHits);
            hit -> SetHitID(countHits);
            hit -> SetPadID(padID);
            hit -> SetPosition(fPosReco);
            hit -> SetPositionError(TVector3(0,0,0));
            hit -> SetCharge(amplitude);
            hit -> SetPedestal(pedestal);
            hit -> SetTb(tb);

            countHits++;
        }
    }

    lk_info << "Found " << countHits << " hits" << endl;
}

bool LKPulseShapeAnalysisTask::EndOfRun()
{
    return true;
}
