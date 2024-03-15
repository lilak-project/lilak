#include "LKRun.h"
#include "LKBackgroundSubtractionTask.h"
#include "GETChannel.h"
#include "LKChannelBuffer.h"

ClassImp(LKBackgroundSubtractionTask)

LKBackgroundSubtractionTask::LKBackgroundSubtractionTask()
{
    fName = "LKBackgroundSubtractionTask";
}

bool LKBackgroundSubtractionTask::Init()
{
    //fChannelArray = fRun -> GetBranchA("RawData");
    //fSubtractedArray = fRun -> RegisterBranchA("BSChannel","LKChannelBuffer",100);
    //fAna = new LKBackgroundAnalyzer();
    //fPar -> UpdatePar(fFluctuationThreshold, "LKBackgroundSubtractionTask/FluctuationThreshold 200 # if meanStdDv of channel > [FluctuationThreshold], channel is assumed to have signal");

    return true;
}

void LKBackgroundSubtractionTask::Exec(Option_t *option)
{
    /*
    int numChannels = fChannelArray -> GetEntriesFast();
    
    if (numChannels<2)
        return;

    int currentCobo = -1;
    int currentAsad = -1;
    int currentAget = -1;

    fAgetGroupChIdx.clear();
    fCoboGroupChIdx.clear();
    fAsadGroupChIdx.clear();

    fCoboGroupChIdx.push_back(0);
    fAsadGroupChIdx.push_back(0);
    fAgetGroupChIdx.push_back(0);

    fNumCoboGroup = 0;
    fNumAsadGroup = 0;
    fNumAgetGroup = 0;

    fAgetGroupIsAnalyzed.clear();

    for (auto iChannel=0; iChannel<numChannels; ++iChannel)
    {
        auto channel = (GETChannel *) fChannelArray -> At(iChannel);
        auto cobo = channel -> GetCobo();
        auto asad = channel -> GetAsad();
        auto aget = channel -> GetAget();

        //if (currentCobo!=cobo) { fCoboGroupChIdx.push_back(iChannel); fAsadGroupChIdx.push_back(iChannel); fAgetGroupChIdx.push_back(iChannel); }
        //if (currentAsad!=asad) { fAsadGroupChIdx.push_back(iChannel); fAgetGroupChIdx.push_back(iChannel); }
        if (currentCobo!=cobo) {
            fAgetGroupChIdx.push_back(iChannel);
            fCoboGroupChIdx.push_back(cobo);
            fAsadGroupChIdx.push_back(asad);
            ++fNumCoboGroup;
            ++fNumAsadGroup;
            ++fNumAgetGroup;
        }
        if (currentAsad!=asad) {
            fAgetGroupChIdx.push_back(iChannel);
            fAsadGroupChIdx.push_back(asad);
            ++fNumAsadGroup;
            ++fNumAgetGroup;
        }
        if (currentAget!=aget) {
            fAgetGroupChIdx.push_back(iChannel);
            ++fNumAgetGroup;
        }

        auto channelBS = (LKChannelBuffer *) fSubtractedArray -> ConstructedAt(iChannel);
        channelBS -> SetID(iChannel);
        channelBS -> SetChannelID(channel->GetCAAC());
        channelBS -> SetBuffer(channel->GetWaveformY());
    }

    for (auto iGroup=0; iGroup<fNumAgetGroup; ++iGroup)
    {
        fAna -> Clear();
        auto iChannel1 = fAgetGroupChIdx[iGroup];
        auto iChannel2 = fAgetGroupChIdx[iGroup+1];
        for (auto iChannel=iChannel1; iChannel<iChannel2; ++iChannel)
        {
            auto channelBS = (LKChannelBuffer *) fSubtractedArray -> At(iChannel);
            fAna -> Add(channelBS);
        }
        auto analyzed = fAna -> Analyze();
        fAgetGroupIsAnalyzed.push_back(analyzed);
    }

    for (auto iGroup=0; iGroup<fNumAgetGroup; ++iGroup)
    {
        if (fAgetGroupIsAnalyzed[iGroup]==false)
    }


    if (currentCobo!=cobo);
    if (currentAsad!=asad);
    if (currentAget!=aget);
    */

    /*
       if (currentCobo!=cobo || currentAsad!=asad || currentAget!=aget) // group end
       {
       bool analyzed = fAna -> Analyze();
       if (analyzed) {
       fAna -> Clear();
       }
       else {
       if (currentCobo!=cobo);
       if (currentAsad!=asad);
       if (currentAget!=aget);
       }
       currentCobo = cobo;
       currentAsad = asad;
       currentAget = aget;
       }

       fAna -> Add(channelBS);
     */
    //fAna -> Analyze();
}
