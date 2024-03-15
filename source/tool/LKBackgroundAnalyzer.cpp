#include "LKBackgroundAnalyzer.h"

using namespace std;

ClassImp(LKBackgroundAnalyzer)

bool LKBackgroundAnalyzer::Init()
{
    fUseFPNData = true;
    fFluctuationThreshold = 100;
    fChannelArray = new TObjArray();
    Clear();
    return true;
}

void LKBackgroundAnalyzer::Clear(Option_t *option)
{
    fChannelArray -> Clear();
    memset(fFPNData, 0, sizeof(double)*512);
    fNumFPNData = 0;
    //fChannelRef = nullptr;
}

void LKBackgroundAnalyzer::Add(LKChannelBuffer* channel)
{
    fChannelArray -> Add(channel);
    //auto channelID = channel -> GetChan();
    //if (channelID==11) AddFPNData(channel);
    //if (channelID==22) AddFPNData(channel);
    //if (channelID==45) AddFPNData(channel);
    //if (channelID==56) AddFPNData(channel);
}

void LKBackgroundAnalyzer::AddFPNData(LKChannelBuffer* channel)
{
    auto buffer = channel -> GetBuffer();
    for (auto tb=0; tb<512; ++tb)
        fFPNData[tb] += (buffer[tb] + fFPNData[tb]*fNumFPNData)/(fNumFPNData+1);
    fNumFPNData++;
}

bool LKBackgroundAnalyzer::Analyze()
{
    fChannelRef = nullptr;

    int numChannels = fChannelArray -> GetEntries();

    if (numChannels-fNumFPNData<2)
        return false;

    int iChannelRef = -1;
    double fluctuationMax = 0;

    for (auto iChannel=0; iChannel<numChannels; ++iChannel)
    {
        auto channel = (LKChannelBuffer*) fChannelArray -> At(iChannel);

        /*
        auto channelID = channel -> GetChan();
        if (channelID==11) continue;
        if (channelID==22) continue;
        if (channelID==45) continue;
        if (channelID==56) continue;
        */

        if (fUseFPNData)
            channel -> SubtractBuffer(GetFPNData());

        auto fluctuation = channel -> CalculateGroupFluctuation(8, 512);
        lk_debug << iChannel << " >fluctuation-> " << fluctuation << endl;
        channel -> SetTime(-1);
        channel -> SetEnergy(-1);
        channel -> SetPedestal(fluctuation);
        if (fluctuation<fFluctuationThreshold && fluctuation>fluctuationMax) {
            iChannelRef = iChannel;
            fluctuationMax = fluctuation;
        }
    }

    if (iChannelRef<0)
        return false;

    fChannelRef = (LKChannelBuffer*) fChannelArray -> At(iChannelRef);
    double* bufferRef = fChannelRef -> GetBuffer();
    for (auto iChannel=0; iChannel<numChannels; ++iChannel)
    {
        auto channel = (LKChannelBuffer*) fChannelArray -> At(iChannel);
        auto scale = channel -> GetScale(bufferRef);
        channel -> SetNoiseScale(scale);
        channel -> SubtractBuffer(bufferRef, scale);
    }

    return true;
}
