#include "LKNoiseAnalyzer.h"

using namespace std;

ClassImp(LKNoiseAnalyzer)

bool LKNoiseAnalyzer::Init()
{
    fUseFPNData = true;
    fFluctuationThreshold = 100;
    fChannelArray = new TObjArray();
    Clear();
    return true;
}

void LKNoiseAnalyzer::Clear(Option_t *option)
{
    fChannelArray -> Clear();
    memset(fFPNData, 0, sizeof(double)*512);
    fNumFPNData = 0;
    fChannelRef = nullptr;
}

void LKNoiseAnalyzer::Add(GETChannel* channel)
{
    fChannelArray -> Add(channel);
}

void LKNoiseAnalyzer::AddFPNData(GETChannel* channel)
{
    auto buffer = channel -> GetBufferArray();
    for (auto tb=0; tb<512; ++tb)
        fFPNData[tb] += (buffer[tb] + fFPNData[tb]*fNumFPNData)/(fNumFPNData+1);
    fNumFPNData++;
}

int LKNoiseAnalyzer::FindRefChannelID()
{
    int iChannelRef = -1;
    double fluctuationMax = 0;
    int numChannels = fChannelArray -> GetEntries();
    for (auto iChannel=0; iChannel<numChannels; ++iChannel)
    {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);

        if (fUseFPNData && fNumFPNData>0)
            channel -> GetBuffer().SubtractArray(fFPNData);

        auto fluctuation = channel -> GetBuffer().CalculateGroupFluctuation(8, 512);
        if (fluctuation<fFluctuationThreshold && fluctuation>fluctuationMax) {
            iChannelRef = iChannel;
            fluctuationMax = fluctuation;
        }
    }

    return iChannelRef;
}

bool LKNoiseAnalyzer::Analyze()
{
    int numChannels = fChannelArray -> GetEntries();
    int iChannelRef = FindRefChannelID();

    fChannelRef = (GETChannel*) fChannelArray -> At(iChannelRef);
    auto bufferRef = fChannelRef -> GetBufferArray();
    for (auto iChannel=0; iChannel<numChannels; ++iChannel)
    {
        auto channel = (GETChannel*) fChannelArray -> At(iChannel);
        auto scale = channel -> GetScale(bufferRef);
        channel -> SetNoiseScale(scale);
        channel -> SubtractArray(bufferRef, scale);
    }

    return true;
    //if (currentCobo!=cobo) { fCoboGroupChIdx.push_back(iChannel); fAsadGroupChIdx.push_back(iChannel); fAgetGroupChIdx.push_back(iChannel); }
    //if (currentAsad!=asad) { fAsadGroupChIdx.push_back(iChannel); fAgetGroupChIdx.push_back(iChannel); }
}
