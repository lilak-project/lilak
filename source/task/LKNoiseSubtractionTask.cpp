#include "LKRun.h"
#include "LKNoiseSubtractionTask.h"
#include "GETChannel.h"

ClassImp(LKNoiseSubtractionTask)

LKNoiseSubtractionTask::LKNoiseSubtractionTask()
{
    fName = "LKNoiseSubtractionTask";
}

bool LKNoiseSubtractionTask::Init()
{
    fChannelArray = fRun -> GetBranchA("RawData");
    //fChannelArray = fRun -> KeepBranchA("RawData");
    //fChannelArray = fRun -> RegisterBranchA("NSData","LKChannelD",100);
    fAna = new LKNoiseAnalyzer();
    fPar -> UpdatePar(fFluctuationThreshold, "LKNoiseSubtractionTask/FluctuationThreshold 200 # if meanStdDv of channel > [FluctuationThreshold], channel is assumed to have signal");

    return true;
}

void LKNoiseSubtractionTask::Exec(Option_t *option)
{
    int numChannels = fChannelArray -> GetEntriesFast();
    
    if (numChannels<2)
        return;

    for (auto iChannel=0; iChannel<numChannels; ++iChannel)
    {
        auto channel = (GETChannel *) fChannelArray -> At(iChannel);
        fAna -> Add(channel);
    }

    if (fAna -> TestValidity())
    {
        fAna -> Clear();
        fAna -> Analyze();
        lk_info << "Good!" << endl;
    }
    else {
        lk_info << "Bad!" << endl;
    }
}
