#include "LKGETRawConverter.h"

#include "GETChannel.h"
#include "LKEventHeader.h"
#include "LKLogger.h"

ClassImp(LKGETRawConverter);

LKGETRawConverter::LKGETRawConverter()
{
}

LKGETRawConverter::~LKGETRawConverter()
{
    if (fAsadIsTriggered != nullptr) {
        for (Int_t i = 0; i < fMaxCobo; ++i)
            delete [] fAsadIsTriggered[i];
        delete [] fAsadIsTriggered;
    }

    delete fWaveforms;
    delete fChannelAnalyzer;
}

uint32_t LKGETRawConverter::ReadBE32(const uint8_t* data)
{
    return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | uint32_t(data[3]);
}

uint64_t LKGETRawConverter::ReadBE48(const uint8_t* data)
{
    uint64_t value = 0;
    for (int i = 0; i < 6; ++i)
        value = (value << 8) | uint64_t(data[i]);
    return value;
}

uint32_t LKGETRawConverter::ReadType1Word(const uint8_t* data)
{
    return ReadBE32(data);
}

uint16_t LKGETRawConverter::ReadType2Word(const uint8_t* data)
{
    return (uint16_t(data[0]) << 8) | uint16_t(data[1]);
}

void LKGETRawConverter::SetPar(LKParameterContainer* par)
{
    fPar = par;

    fPar->UpdatePar(fSet2PMode   ,"LKFrameBuilder/Set2PMode   false");
    fPar->UpdatePar(fSetScaler   ,"LKFrameBuilder/SetScaler   false");
    fPar->UpdatePar(fMaxCobo     ,"LKFrameBuilder/MaxCobo     4 # maximum number of CoBo");
    fPar->UpdatePar(fMaxAsad     ,"LKFrameBuilder/MaxAsad     4 # maximum number of AsAd in one CoBo");
    fPar->UpdatePar(fMaxAget     ,"LKFrameBuilder/MaxAget     4 # maximum number of AGET in one AsAd");
    fPar->UpdatePar(fMaxChannels ,"LKFrameBuilder/MaxChannels 68 # maximum number of channels in AGET");
    fPar->UpdatePar(fScalerFile  ,"LKFrameBuilder/ScalerFile  # if exist");
}

bool LKGETRawConverter::Init()
{
    bool missingEssentials = false;
    if (fChannelArray == nullptr) {
        missingEssentials = true;
        lk_error << "Channel array should be set before LKGETRawConverter::Init()" << endl;
    }
    if (fEventHeaderArray == nullptr) {
        missingEssentials = true;
        lk_error << "Event header array should be set before LKGETRawConverter::Init()" << endl;
    }
    if (fPar == nullptr) {
        missingEssentials = true;
        lk_error << "Parameter container should be set before LKGETRawConverter::Init()" << endl;
    }
    if (missingEssentials)
        return false;

    fIsFirstEvent = true;
    fFirstEventIdx = -1;
    fCurrEventIdx = -1;
    fPrevEventIdx = 0;
    fMutantCounter = 0;
    fMultGET = 0;
    fCountEvents = 0;

    if (fAsadIsTriggered != nullptr) {
        for (Int_t i = 0; i < fMaxCobo; ++i)
            delete [] fAsadIsTriggered[i];
        delete [] fAsadIsTriggered;
    }
    fAsadIsTriggered = new Int_t*[fMaxCobo];
    for (Int_t i = 0; i < fMaxCobo; ++i) {
        fAsadIsTriggered[i] = new Int_t[fMaxAsad];
        for (Int_t j = 0; j < fMaxAsad; ++j)
            fAsadIsTriggered[i][j] = 0;
    }

    InitWaveforms();

    delete fChannelAnalyzer;
    fChannelAnalyzer = new LKChannelAnalyzer();

    return true;
}

void LKGETRawConverter::InitWaveforms()
{
    delete fWaveforms;
    fWaveforms = new WaveForms();
    fWaveforms->waveform.resize(fMaxAsad * fMaxAget);
    fWaveforms->hasSignal.resize(fMaxAsad * fMaxAget);
    fWaveforms->hasHit.resize(fMaxAsad * fMaxAget);
    fWaveforms->hasFPN.resize(fMaxAsad * fMaxAget);
    fWaveforms->doneFPN.resize(fMaxAsad * fMaxAget);
    for (Int_t iAsad = 0; iAsad < fMaxAsad; ++iAsad) {
        for (Int_t iAget = 0; iAget < fMaxAget; ++iAget) {
            auto idx = iAsad * fMaxAget + iAget;
            fWaveforms->waveform[idx].resize(fMaxChannels);
            fWaveforms->hasSignal[idx].resize(fMaxChannels);
            for (Int_t iChan = 0; iChan < fMaxChannels; ++iChan)
                fWaveforms->waveform[idx][iChan].resize(fMaxTimeBuckets);
        }
    }
}

void LKGETRawConverter::ResetWaveforms()
{
    for (Int_t iAsad = 0; iAsad < fMaxAsad; ++iAsad) {
        for (Int_t iAget = 0; iAget < fMaxAget; ++iAget) {
            auto idx = iAsad * fMaxAget + iAget;
            if (fWaveforms->hasHit[idx] || fWaveforms->hasFPN[idx]) {
                for (Int_t iChan = 0; iChan < fMaxChannels; ++iChan) {
                    if (fWaveforms->hasSignal[idx][iChan]) {
                        fWaveforms->hasSignal[idx][iChan] = false;
                        std::fill(fWaveforms->waveform[idx][iChan].begin(),
                                  fWaveforms->waveform[idx][iChan].end(), 0);
                    }
                }
            }
            fWaveforms->hasHit[idx] = false;
            fWaveforms->hasFPN[idx] = false;
            fWaveforms->doneFPN[idx] = false;
        }
    }
    fWaveforms->ICenergy = 0;
    fWaveforms->Sienergy = 0;
    fWaveforms->SiX = 0;
    fWaveforms->SiY = 0;
    fWaveforms->SiZ = 0;
    fWaveforms->isRejected = false;
}

void LKGETRawConverter::ProcessFrame(const LKGETRawFrame& frame)
{
    if (frame.frameType == 0x7) {
        DecodeCoBoTopologyFrame(frame);
        return;
    }
    if (frame.frameType == 0x8) {
        DecodeMuTanTFrame(frame);
        return;
    }

    Event(frame);
    ++fCountEvents;
}

void LKGETRawConverter::Event(const LKGETRawFrame& frame)
{
    fWaveforms->frameIdx = 0;
    fWaveforms->decayIdx = 0;
    fChannelArray->Clear("C");
    fCountChannels = 0;

    if (frame.isLayered) {
        for (size_t iFrame = 0; iFrame < frame.children.size(); ++iFrame) {
            const auto& subFrame = frame.children[iFrame];
            if (subFrame.itemCount == 0)
                continue;
            fWaveforms->frameIdx = iFrame;
            fWaveforms->decayIdx = 0;
            UnpackFrame(subFrame);
            WriteChannels();
            ResetWaveforms();
        }
    }
    else if (frame.itemCount > 0) {
        UnpackFrame(frame);
        WriteChannels();
        ResetWaveforms();
    }
}

void LKGETRawConverter::UnpackFrame(const LKGETRawFrame& frame)
{
    Int_t prevWEventIdx = fCurrEventIdx;
    UInt_t coboIdx = frame.coboIdx;
    UInt_t asadIdx = frame.asadIdx;
    fEventTime = UInt_t(frame.eventTime);
    fCurrEventIdx = Int_t(frame.eventIdx);

    if (fIsFirstEvent) {
        fFirstEventIdx = fCurrEventIdx;
        fIsFirstEvent = false;
    }

    if (prevWEventIdx != fCurrEventIdx) {
        if (fMultGET > 0) {
            fGETEventIdx = prevWEventIdx;
            if (fSet2PMode) {
                for (Int_t iEvent = fPrevEventIdx; iEvent < (Int_t) fListOfEventIdx.size(); ++iEvent) {
                    if ((Int_t) fListOfEventIdx.at(iEvent) == fGETEventIdx) {
                        fGETD2PTime = fListOfD2PTime.at(iEvent);
                        fGETTimeStamp = fListOfTimeStamp.at(iEvent);
                        fPrevEventIdx = iEvent;
                    }
                }
            }

            fGETEventIdx = fCurrEventIdx;
            fMultGET = 0;
            for (Int_t iCobo = 0; iCobo < fMaxCobo; ++iCobo)
                for (Int_t iAsad = 0; iAsad < fMaxAsad; ++iAsad)
                    fAsadIsTriggered[iCobo][iAsad] = 0;
        }
    }

    const size_t numSamples = frame.itemCount;
    const size_t numChannels = 68u;
    const size_t numChips = 4u;
    std::vector<uint32_t> chanIdx_(numChips, 0u);
    std::vector<uint32_t> buckIdx_(numChips, 0u);

    if (frame.frameType == 1u) {
        if (fSet2PMode) {
            if (fAsadIsTriggered[coboIdx][asadIdx] > 0) {
                fAsadIsTriggered[coboIdx][asadIdx] = 2;
                fWaveforms->decayIdx = 1;
            }
            else {
                fAsadIsTriggered[coboIdx][asadIdx] = 1;
                fWaveforms->decayIdx = 0;
            }
        }
        else {
            fAsadIsTriggered[coboIdx][asadIdx] = 0;
            fWaveforms->decayIdx = 0;
        }

        const uint8_t* items = frame.Payload();
        for (size_t iItem = 0; iItem < numSamples; ++iItem) {
            uint32_t word = ReadType1Word(items + iItem * frame.itemSizeBytes);
            UInt_t agetIdx = (word >> 30) & 0x3u;
            if (agetIdx > 3)
                return;
            UInt_t chanIdx = (word >> 23) & 0x7Fu;
            if (chanIdx > 67)
                continue;
            UInt_t buckIdx = (word >> 14) & 0x1FFu;
            UInt_t sampleValue = word & 0xFFFu;

            fWaveforms->coboIdx = coboIdx;
            fWaveforms->asadIdx = asadIdx;
            fWaveforms->hasSignal[asadIdx * fMaxAget + agetIdx][chanIdx] = true;
            fWaveforms->hasHit[asadIdx * fMaxAget + agetIdx] = true;
            fWaveforms->waveform[asadIdx * fMaxAget + agetIdx][chanIdx][buckIdx] = sampleValue;
        }
    }
    else if (frame.frameType == 2u) {
        if (fSet2PMode) {
            if (fAsadIsTriggered[coboIdx][asadIdx] > 0) {
                fAsadIsTriggered[coboIdx][asadIdx] = 2;
                fWaveforms->decayIdx = 1;
            }
            else {
                fAsadIsTriggered[coboIdx][asadIdx] = 1;
                fWaveforms->decayIdx = 0;
            }
        }
        else {
            fAsadIsTriggered[coboIdx][asadIdx] = 0;
            fWaveforms->decayIdx = 0;
        }

        const uint8_t* items = frame.Payload();
        if (numSamples > 0) {
            uint16_t word = ReadType2Word(items);
            uint32_t agetIdx = (word >> 14) & 0x3u;
            uint32_t sampleValue = word & 0xFFFu;

            fWaveforms->coboIdx = coboIdx;
            fWaveforms->asadIdx = asadIdx;
            fWaveforms->hasSignal[asadIdx * fMaxAget + agetIdx][chanIdx_[agetIdx]] = true;
            fWaveforms->hasHit[asadIdx * fMaxAget + agetIdx] = true;
            fWaveforms->waveform[asadIdx * fMaxAget + agetIdx][chanIdx_[agetIdx]][buckIdx_[agetIdx]] = sampleValue;
        }

        for (size_t itemId = 1; itemId < numSamples; ++itemId) {
            uint16_t word = ReadType2Word(items + itemId * frame.itemSizeBytes);
            uint32_t agetIdx = (word >> 14) & 0x3u;
            if (chanIdx_[agetIdx] >= numChannels) {
                chanIdx_[agetIdx] = 0u;
                buckIdx_[agetIdx]++;
            }
            uint32_t sampleValue = word & 0xFFFu;

            fWaveforms->hasSignal[asadIdx * fMaxAget + agetIdx][chanIdx_[agetIdx]] = true;
            fWaveforms->hasHit[asadIdx * fMaxAget + agetIdx] = true;
            fWaveforms->waveform[asadIdx * fMaxAget + agetIdx][chanIdx_[agetIdx]][buckIdx_[agetIdx]] = sampleValue;
            chanIdx_[agetIdx]++;
        }
    }
    else {
        lk_info << "Frame type " << frame.frameType << " not found" << endl;
    }
}

void LKGETRawConverter::DecodeCoBoTopologyFrame(const LKGETRawFrame& /* frame */)
{
}

void LKGETRawConverter::DecodeMuTanTFrame(const LKGETRawFrame& frame)
{
    fMutantCounter++;
    if (frame.frameType != 0x8 || frame.bytes.size() < 64)
        return;

    try {
        if (fSet2PMode) {
            fListOfEventIdx.push_back(ReadBE32(frame.Data() + 14));
            fListOfD2PTime.push_back(ReadBE32(frame.Data() + 60) * 10);
            fListOfTimeStamp.push_back(UInt_t(ReadBE48(frame.Data() + 8)));
        }
        else {
            double scaler1 = ReadBE32(frame.Data() + 48);
            double scaler2 = ReadBE32(frame.Data() + 52);
            double scaler3 = ReadBE32(frame.Data() + 56);
            uint32_t scalerEvent = ReadBE32(frame.Data() + 24);
            uint32_t scalerEventIdx = ReadBE32(frame.Data() + 14);
            uint32_t scalerTimeStamp = UInt_t(ReadBE48(frame.Data() + 8));
            (void) scalerEvent;
            (void) scalerEventIdx;
            (void) scalerTimeStamp;

            if (fMutantCounter == 100) {
                if (fSetScaler) {
                    fFileScaler.open(fScalerFile.Data(), std::ofstream::out | std::ofstream::app);
                    fFileScaler << scalerTimeStamp << " " << scalerEvent << " " << scalerEventIdx << " "
                                << scaler1 << " " << scaler2 << " " << scaler3 << endl;
                    fFileScaler.close();
                }
                fMutantCounter = 0;
            }
        }
    } catch (const std::exception&) {
    }
}

void LKGETRawConverter::WriteChannels()
{
    UInt_t coboIdx = fWaveforms->coboIdx;

    auto eventHeader = (LKEventHeader*) fEventHeaderArray->ConstructedAt(0);
    eventHeader->SetEventNumber(Int_t(fCurrEventIdx));
    eventHeader->SetEventTime(fEventTime);

    for (UInt_t asad = 0; asad < (UInt_t) fMaxAsad; ++asad) {
        for (UInt_t aget = 0; aget < (UInt_t) fMaxAget; ++aget) {
            if (!fWaveforms->hasHit[asad * fMaxAget + aget])
                continue;

            for (UInt_t chan = 0; chan < (UInt_t) fMaxChannels; ++chan) {
                if (!fWaveforms->hasSignal[asad * fMaxAget + aget][chan])
                    continue;

                auto channel = (GETChannel*) fChannelArray->ConstructedAt(fCountChannels);
                channel->SetChannelID(fCountChannels);
                channel->SetCobo(coboIdx);
                channel->SetAsad(asad);
                channel->SetAget(aget);
                channel->SetChan(chan);
                channel->SetWaveformY(fWaveforms->waveform[asad * fMaxAget + aget][chan]);

                double time = 0.;
                double energy = 0.;
                double pedestal = 0.;
                auto buffer = channel->GetWaveformY();
                fChannelAnalyzer->Analyze(buffer);
                if (fChannelAnalyzer->GetNumHits() > 0) {
                    time = fChannelAnalyzer->GetTbHit(0);
                    energy = fChannelAnalyzer->GetAmplitude(0);
                    pedestal = fChannelAnalyzer->GetPedestal();
                }
                channel->SetTime(time);
                channel->SetEnergy(energy);
                channel->SetPedestal(pedestal);

                fCountChannels++;
                fMultGET++;
            }
        }
    }
}
