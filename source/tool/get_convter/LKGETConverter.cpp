#include "LKGETConverter.h"

#include "GETChannel.h"
#include "LKEventHeader.h"
#include "LKLogger.h"

ClassImp(LKGETConverter);

LKGETConverter::LKGETConverter()
{
}

LKGETConverter::~LKGETConverter()
{
    if (fAsadIsTriggered != nullptr) {
        for (Int_t i = 0; i < fMaxCobo; ++i)
            delete [] fAsadIsTriggered[i];
        delete [] fAsadIsTriggered;
    }

    delete fWaveforms;
    delete fChannelAnalyzer;
}

void LKGETConverter::SetPar(LKParameterContainer* par)
{
    fPar = par;

    fPar->UpdatePar(fSet2PMode   ,"LKFrameBuilder/Set2PMode   false");
    fPar->UpdatePar(fSetScaler   ,"LKFrameBuilder/SetScaler   false");
    fPar->UpdatePar(fMaxCobo     ,"LKFrameBuilder/MaxCobo     4 # maximum number of CoBo");
    fPar->UpdatePar(fMaxAsad     ,"LKFrameBuilder/MaxAsad     4 # maximum number of AsAd in one CoBo");
    fPar->UpdatePar(fMaxAget     ,"LKFrameBuilder/MaxAget     4 # maximum number of AGET in one AsAd");
    fPar->UpdatePar(fMaxChannels ,"LKFrameBuilder/MaxChannels 68 # maximum number of channels in AGET");
    fPar->UpdatePar(fFrameFormat ,"LKFrameBuilder/FrameFormat #{lilak_common}/CoboFormats.xcfg");
    fPar->UpdatePar(fScalerFile  ,"LKFrameBuilder/ScalerFile  # if exist");
}

bool LKGETConverter::Init()
{
    bool missingEssentials = false;
    if (fChannelArray == nullptr) {
        missingEssentials = true;
        lk_error << "Channel array should be set before LKGETConverter::Init()" << endl;
    }
    if (fEventHeaderArray == nullptr) {
        missingEssentials = true;
        lk_error << "Event header array should be set before LKGETConverter::Init()" << endl;
    }
    if (fPar == nullptr) {
        missingEssentials = true;
        lk_error << "Parameter container should be set before LKGETConverter::Init()" << endl;
    }
    if (missingEssentials)
        return false;

    fIsFirstEvent = true;
    fFirstEventIdx = -1;
    fCurrEventIdx = -1;
    fPrevEventIdx = 0;
    fCountPrint = 0;
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

    mfm::FrameDictionary::instance().addFormats(fFrameFormat.Data());

    InitWaveforms();

    delete fChannelAnalyzer;
    fChannelAnalyzer = new LKChannelAnalyzer();

    return true;
}

void LKGETConverter::InitWaveforms()
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

void LKGETConverter::ResetWaveforms()
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

void LKGETConverter::ProcessFrame(mfm::Frame& frame)
{
    if (frame.header().isBlobFrame()) {
        if (frame.header().frameType() == 0x7)
            DecodeCoBoTopologyFrame(frame);
        else if (frame.header().frameType() == 0x8)
            DecodeMuTanTFrame(frame);
        return;
    }

    ValidateEvent(frame);
    Event(frame);
    ++fCountEvents;
}

void LKGETConverter::ValidateEvent(mfm::Frame& frame)
{
    if (frame.header().isLayeredFrame()) {
        for (Int_t iFrame = 0; iFrame < (Int_t) frame.itemCount(); ++iFrame) {
            try {
                auto subFrame = frame.frameAt(iFrame);
                auto numItems = (Int_t) (*subFrame.get()).itemCount();
                if (numItems > 0)
                    ValidateFrame(*subFrame.get());
            } catch (const std::exception& e) {
                e_error << "Error in LKGETConverter::ValidateEvent at sub-frame " << iFrame << endl;
                e_cout << e.what() << endl;
                return;
            }
        }
    }
    else if (frame.itemCount() > 0) {
        ValidateFrame(frame);
    }
}

void LKGETConverter::ValidateFrame(mfm::Frame& /* frame */)
{
}

void LKGETConverter::Event(mfm::Frame& frame)
{
    fWaveforms->frameIdx = 0;
    fWaveforms->decayIdx = 0;
    fChannelArray->Clear("C");
    fCountChannels = 0;

    if (frame.header().isLayeredFrame()) {
        for (Int_t iFrame = 0; iFrame < (Int_t) frame.itemCount(); ++iFrame) {
            try {
                std::auto_ptr<mfm::Frame> subFrame = frame.frameAt(iFrame);
                if ((*subFrame.get()).itemCount() > 0) {
                    fWaveforms->frameIdx = iFrame;
                    fWaveforms->decayIdx = 0;
                    UnpackFrame(*subFrame.get());
                    WriteChannels();
                    ResetWaveforms();
                }
            } catch (const std::exception& e) {
                lk_info << e.what() << endl;
                return;
            }
        }
    }
    else {
        try {
            if (frame.itemCount() > 0) {
                UnpackFrame(frame);
                WriteChannels();
                ResetWaveforms();
            }
        } catch (const std::exception& e) {
            lk_info << e.what() << endl;
            return;
        }
    }
}

void LKGETConverter::UnpackFrame(mfm::Frame& frame)
{
    Int_t prevWEventIdx = fCurrEventIdx;
    UInt_t coboIdx = frame.headerField("coboIdx").value<UInt_t>();
    UInt_t asadIdx = frame.headerField("asadIdx").value<UInt_t>();
    fEventTime = frame.headerField("eventTime").value<UInt_t>();
    fCurrEventIdx = (Int_t) frame.headerField("eventIdx").value<UInt_t>();

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

    mfm::Item item = frame.itemAt(0u);
    mfm::Field field = item.field("");

    const size_t numSamples = frame.itemCount();
    const size_t numChannels = 68u;
    const size_t numChips = 4u;
    std::vector<uint32_t> chanIdx_(numChips, 0u);
    std::vector<uint32_t> buckIdx_(numChips, 0u);

    if (frame.header().frameType() == 1u) {
        mfm::BitField agetIdxField = field.bitField("agetIdx");
        mfm::BitField chanIdxField = field.bitField("chanIdx");
        mfm::BitField buckIdxField = field.bitField("buckIdx");
        mfm::BitField sampleValueField = field.bitField("sample");

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

        for (UInt_t iItem = 0; iItem < frame.itemCount(); ++iItem) {
            item = frame.itemAt(iItem);
            field = item.field(field);
            agetIdxField = field.bitField(agetIdxField);
            chanIdxField = field.bitField(chanIdxField);
            buckIdxField = field.bitField(buckIdxField);
            sampleValueField = field.bitField(sampleValueField);

            UInt_t agetIdx = agetIdxField.value<UInt_t>();
            if (agetIdx > 3)
                return;

            UInt_t chanIdx = chanIdxField.value<UInt_t>();
            if (chanIdx > 67)
                continue;

            UInt_t buckIdx = buckIdxField.value<UInt_t>();
            UInt_t sampleValue = sampleValueField.value<UInt_t>();

            fWaveforms->coboIdx = coboIdx;
            fWaveforms->asadIdx = asadIdx;
            fWaveforms->hasSignal[asadIdx * fMaxAget + agetIdx][chanIdx] = true;
            fWaveforms->hasHit[asadIdx * fMaxAget + agetIdx] = true;
            fWaveforms->waveform[asadIdx * fMaxAget + agetIdx][chanIdx][buckIdx] = sampleValue;
        }
    }
    else if (frame.header().frameType() == 2u) {
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

        if (numSamples > 0) {
            item = frame.itemAt(0u);
            field = item.field("");
            mfm::BitField agetIdxField = field.bitField("agetIdx");
            mfm::BitField sampleValueField = field.bitField("sample");

            const uint32_t agetIdx = agetIdxField.value<uint32_t>();
            const uint32_t sampleValue = sampleValueField.value<uint32_t>();

            fWaveforms->coboIdx = coboIdx;
            fWaveforms->asadIdx = asadIdx;
            fWaveforms->hasSignal[asadIdx * fMaxAget + agetIdx][chanIdx_[agetIdx]] = true;
            fWaveforms->hasHit[asadIdx * fMaxAget + agetIdx] = true;
            fWaveforms->waveform[asadIdx * fMaxAget + agetIdx][chanIdx_[agetIdx]][buckIdx_[agetIdx]] = sampleValue;
        }

        for (size_t itemId = 1; itemId < numSamples; ++itemId) {
            item = frame.itemAt(itemId);
            field = item.field(field);
            mfm::BitField agetIdxField = field.bitField("agetIdx");
            mfm::BitField sampleValueField = field.bitField("sample");

            const uint32_t agetIdx = agetIdxField.value<uint32_t>();
            if (chanIdx_[agetIdx] >= numChannels) {
                chanIdx_[agetIdx] = 0u;
                buckIdx_[agetIdx]++;
            }
            const uint32_t sampleValue = sampleValueField.value<uint32_t>();

            fWaveforms->hasSignal[asadIdx * fMaxAget + agetIdx][chanIdx_[agetIdx]] = true;
            fWaveforms->hasHit[asadIdx * fMaxAget + agetIdx] = true;
            fWaveforms->waveform[asadIdx * fMaxAget + agetIdx][chanIdx_[agetIdx]][buckIdx_[agetIdx]] = sampleValue;
            chanIdx_[agetIdx]++;
        }
    }
    else {
        lk_info << "Frame type " << frame.header().frameType() << " not found" << endl;
    }
}

void LKGETConverter::DecodeCoBoTopologyFrame(mfm::Frame& frame)
{
    if (frame.header().frameType() != 0x7)
        return;

    std::istream& stream = frame.serializer().inputStream(8u);
    uint8_t value = 0;
    stream.read(reinterpret_cast<char*>(&value), 1lu);
    stream.read(reinterpret_cast<char*>(&value), 1lu);
    stream.read(reinterpret_cast<char*>(&value), 1lu);
}

void LKGETConverter::DecodeMuTanTFrame(mfm::Frame& frame)
{
    fMutantCounter++;

    double scaler1;
    double scaler1start;
    double scaler1end;
    double scaler2;
    double scaler2start;
    double scaler2end;
    double scaler3;
    double scaler3start;
    double scaler3end;

    if (frame.header().frameType() != 0x8)
        return;

    try {
        if (fSet2PMode) {
            fListOfEventIdx.push_back(frame.headerField(14u, 4u).value<uint32_t>());
            fListOfD2PTime.push_back(frame.headerField(60u, 4u).value<uint32_t>() * 10);
            fListOfTimeStamp.push_back(frame.headerField(8u, 6u).value<uint64_t>());
        }
        else {
            scaler1 = frame.headerField(48u, 4u).value<uint32_t>();
            scaler2 = frame.headerField(52u, 4u).value<uint32_t>();
            scaler3 = frame.headerField(56u, 4u).value<uint32_t>();
            uint32_t scalerEvent = frame.headerField(24u, 4u).value<uint32_t>();
            uint32_t scalerEventIdx = frame.headerField(14u, 4u).value<uint32_t>();
            uint32_t scalerTimeStamp = frame.headerField(8u, 6u).value<uint64_t>();

            if (fMutantCounter == 100) {
                scaler1end = scaler1;
                scaler2end = scaler2;
                scaler3end = scaler3;
                if (fSetScaler) {
                    fFileScaler.open(fScalerFile.Data(), std::ofstream::out | std::ofstream::app);
                    fFileScaler << scalerTimeStamp << " " << scalerEvent << " " << scalerEventIdx << " "
                                << scaler1 << " " << scaler2 << " " << scaler3 << endl;
                    fFileScaler.close();
                }
                fMutantCounter = 0;
            }
            else if (fMutantCounter == 1) {
                scaler1start = scaler1;
                scaler2start = scaler2;
                scaler3start = scaler3;
                (void) scaler1start;
                (void) scaler2start;
                (void) scaler3start;
            }
        }
    } catch (const std::exception&) {
    }
}

void LKGETConverter::WriteChannels()
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
