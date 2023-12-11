#include "GETChannel.h"

ClassImp(GETChannel);

GETChannel::GETChannel()
{
    Clear();
}

void GETChannel::Clear(Option_t *option)
{
    LKChannel::Clear(option);
    fDetType = -1;
    fFrameNo = -1;
    fDecayNo = -1;
    fCobo = -1;
    fAsad = -1;
    fAget = -1;
    fChan = -1;
    fChan2 = -1;
    fTime = -1;
    fEnergy = -1;
    for (auto i=0; i<512; ++i) fWaveformX[i] = -1;
    for (auto i=0; i<512; ++i) fWaveformY[i] = -1;
}

void GETChannel::Print(Option_t *option) const
{
    // You will probability need to modify here
    e_info << "GETChannel container" << std::endl;
    e_info << "fDetType : " << fDetType << std::endl;
    e_info << "fFrameNo : " << fFrameNo << std::endl;
    e_info << "fDecayNo : " << fDecayNo << std::endl;
    e_info << "fCobo : " << fCobo << std::endl;
    e_info << "fAsad : " << fAsad << std::endl;
    e_info << "fAget : " << fAget << std::endl;
    e_info << "fChan : " << fChan << std::endl;
    e_info << "fChan2 : " << fChan2 << std::endl;
    e_info << "fTime : " << fTime << std::endl;
    e_info << "fEnergy : " << fEnergy << std::endl;
    for (auto i=0; i<512; ++i) e_cout << fWaveformX[i] << " "; e_cout << std::endl;
    for (auto i=0; i<512; ++i) e_cout << fWaveformY[i] << " "; e_cout << std::endl;
}

void GETChannel::Copy(TObject &object) const
{
    // You should copy data from this container to objCopy
    LKChannel::Copy(object);
    auto objCopy = (GETChannel &) object;
    objCopy.SetDetType(fDetType);
    objCopy.SetFrameNo(fFrameNo);
    objCopy.SetDecayNo(fDecayNo);
    objCopy.SetCobo(fCobo);
    objCopy.SetAsad(fAsad);
    objCopy.SetAget(fAget);
    objCopy.SetChan(fChan);
    objCopy.SetChan2(fChan2);
    objCopy.SetTime(fTime);
    objCopy.SetEnergy(fEnergy);
    objCopy.SetWaveformX(fWaveformX);
    objCopy.SetWaveformY(fWaveformY);
}

GETChannel* GETChannel::CloneChannel() const
{
    auto clone = new GETChannel();
    clone -> SetDetType(fDetType);
    clone -> SetFrameNo(fFrameNo);
    clone -> SetDecayNo(fDecayNo);
    clone -> SetCobo(fCobo);
    clone -> SetAsad(fAsad);
    clone -> SetAget(fAget);
    clone -> SetChan(fChan);
    clone -> SetChan2(fChan2);
    clone -> SetTime(fTime);
    clone -> SetEnergy(fEnergy);
    clone -> SetWaveformX(fWaveformX);
    clone -> SetWaveformY(fWaveformY);
    return clone;
}

void GETChannel::Draw(Option_t *option)
{
    if (fHist==nullptr)
        fHist = new TH1D("hist","",512,0,512);
    fHist -> Reset();
    fHist -> SetTitle(Form("%d %d %d %d", fCobo, fAsad, fAget, fChan));
    for (Int_t i=0; i<512; ++i)
        fHist -> SetBinContent(i+1,fWaveformY[i]);
    fHist -> Draw(option);
}

void GETChannel::SetWaveformY(const UInt_t *waveform)
{
    for (Int_t i=0; i<512; ++i)
        fWaveformY[i] = int(waveform[i]);
}

void GETChannel::SetWaveformY(std::vector<unsigned int> waveform)
{
    for (Int_t i=0; i<512; ++i)
        fWaveformY[i] = int(waveform[i]);
}
