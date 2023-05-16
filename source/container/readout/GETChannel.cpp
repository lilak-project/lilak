#include "GETChannel.hpp"

ClassImp(GETChannel)

void GETChannel::Clear(Option_t *option)
{
    LKContainer::Clear(option);
    memset(fWaveform, 0, sizeof(Int_t)*512);
}

TH1D *GETChannel::GetHist(TString name)
{
    if (name.IsNull())
        name = Form("channel_%d_%d_%d_%d",fCobo, fAsad, fAget, fChan);

    auto hist = new TH1D(name,";Time-bucket;ADC",512,0,512);
    for (auto iTb = 0; iTb < 512; ++iTb)
        hist -> SetBinContent(iTb+1,fWaveform[iTb]);

    return hist;
}

void GETChannel::SetWaveform(Int_t *buffer) {
    memcpy(fWaveform, buffer, sizeof(Int_t)*512);
}
