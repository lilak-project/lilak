#include "mfm/BitField.h"
#include "mfm/Field.h"
#include "mfm/Frame.h"
#include "mfm/FrameBuilder.h"
#include "mfm/FrameDictionary.h"
#include "mfm/Item.h"
#include <sstream>
#include <cstdio>
#include <boost/utility/binary.hpp>

using namespace std;

#include "LKLogger.h"
#include "LKFrameBuilder.h"
#include "LKEventHeader.h"
#include "GETChannel.h"
#include "LKMFMConversionTask.h"

//#define DEBUG_LKFRAMEBUILDER
//#define DEBUG_UNPACKFRAME_CUT 459961344

LKFrameBuilder::LKFrameBuilder()
{
    fName = "LKFrameBuilder";
}

LKFrameBuilder::~LKFrameBuilder()
{
}

void LKFrameBuilder::processFrame(mfm::Frame &frame)
{
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
    if (frame.header().isBlobFrame())
    {
        if (frame.header().frameType() == 0x7)
        {
            lk_info << "Frame is blob frame 0x7" << endl;
            decodeCoBoTopologyFrame(frame);
        }
        else if (frame.header().frameType() == 0x8) {
            lk_info << "Frame is blob frame 0x8" << endl;
            decodeMuTanTFrame(frame);
        }
        lk_info << "Frame is blob frame" << endl;
    }
    else
    {
#ifdef DEBUG_LKFRAMEBUILDER
        lk_debug << fCountEvents << endl;
#endif
        ValidateEvent(frame);
        Event(frame);
        fMotherTask -> SignalNextEvent();
        ++fCountEvents;
    }
}

void LKFrameBuilder::SetPar(LKParameterContainer* par)
{
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
    fPar = par;

    fPar -> UpdatePar(fSet2PMode   ,"LKFrameBuilder/Set2PMode   false");
    fPar -> UpdatePar(fSetScaler   ,"LKFrameBuilder/SetScaler   false");
    fPar -> UpdatePar(fMaxCobo     ,"LKFrameBuilder/MaxCobo     4 # maximum number of CoBo");
    fPar -> UpdatePar(fMaxAsad     ,"LKFrameBuilder/MaxAsad     4 # maximum number of AsAd in one CoBo");
    fPar -> UpdatePar(fMaxAget     ,"LKFrameBuilder/MaxAget     4 # maximum number of AGET in one AsAd");
    fPar -> UpdatePar(fMaxChannels ,"LKFrameBuilder/MaxChannels 68 # maximum number of channels in AGET");
    fPar -> UpdatePar(fFrameFormat ,"LKFrameBuilder/FrameFormat #{lilak_common}/CoboFormats.xcfg");
    fPar -> UpdatePar(fScalerFile  ,"LKFrameBuilder/ScalerFile  # if exist");

    if (fPar -> CheckPar("LKFrameBuilder/Set2PMode"  )) lk_info << "2PMode      is updated = " << fSet2PMode   << endl; else lk_warning << "2PMode      NOT updated = " << fSet2PMode   << endl;
    if (fPar -> CheckPar("LKFrameBuilder/SetScaler"  )) lk_info << "Scaler      is updated = " << fSetScaler   << endl; else lk_warning << "Scaler      NOT updated = " << fSetScaler   << endl;
    if (fPar -> CheckPar("LKFrameBuilder/MaxCobo"    )) lk_info << "MaxCobo     is updated = " << fMaxCobo     << endl; else lk_warning << "MaxCobo     NOT updated = " << fMaxCobo     << endl;
    if (fPar -> CheckPar("LKFrameBuilder/MaxAsad"    )) lk_info << "MaxAsad     is updated = " << fMaxAsad     << endl; else lk_warning << "MaxAsad     NOT updated = " << fMaxAsad     << endl;
    if (fPar -> CheckPar("LKFrameBuilder/MaxAget"    )) lk_info << "MaxAget     is updated = " << fMaxAget     << endl; else lk_warning << "MaxAget     NOT updated = " << fMaxAget     << endl;
    if (fPar -> CheckPar("LKFrameBuilder/MaxChannels")) lk_info << "MaxChannels is updated = " << fMaxChannels << endl; else lk_warning << "MaxChannels NOT updated = " << fMaxChannels << endl;
    if (fPar -> CheckPar("LKFrameBuilder/FrameFormat")) lk_info << "FrameFormat is updated = " << fFrameFormat << endl; else lk_warning << "FrameFormat NOT updated = " << fFrameFormat << endl;
    if (fPar -> CheckPar("LKFrameBuilder/ScalerFile" )) lk_info << "ScalerFile  is updated = " << fScalerFile  << endl; else lk_warning << "ScalerFile  NOT updated = " << fScalerFile  << endl;
}

bool LKFrameBuilder::Init()
{
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
    Bool_t missingEssentials = false;
    if (fMotherTask==nullptr) {
        missingEssentials = true;
        lk_error << "Mother task should be set before Init()!" << endl;
    }
    if (fChannelArray==nullptr) {
        missingEssentials = true;
        lk_error << "Channel array should be set before Init()!" << endl;
    }
    if (fEventHeaderArray==nullptr) {
        missingEssentials = true;
        lk_error << "Event header array should be set before Init()!" << endl;
    }
    if (fPar==nullptr) {
        missingEssentials = true;
        lk_error << "Parameter container should be set before Init()!" << endl;
    }

    if (missingEssentials)
        return false;

    fIsFirstEvent = true;
    fFirstEventIdx = -1;
    fCurrEventIdx = -1;
    fPrevEventIdx = 0;
    fCountPrint = 0;
    fMutantCounter = 0;

    fAsadIsTriggered = new Int_t*[fMaxCobo];
    for (Int_t i=0; i<fMaxCobo; i++) {
        fAsadIsTriggered[i] = new Int_t[fMaxAsad];
        for (Int_t j=0; j<fMaxAsad; j++)
            fAsadIsTriggered[i][j] = 0;
    }

    mfm::FrameDictionary::instance().addFormats(fFrameFormat.Data());

    InitWaveforms();

    fChannelAnalyzer = new LKChannelAnalyzer();

    return true;
}

void LKFrameBuilder::InitWaveforms()
{
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
    fWaveforms = new WaveForms();
    fWaveforms->waveform.resize(fMaxAsad*fMaxAget);
    fWaveforms->hasSignal.resize(fMaxAsad*fMaxAget);
    fWaveforms->hasHit.resize(fMaxAsad*fMaxAget); //default set to false
    fWaveforms->hasFPN.resize(fMaxAsad*fMaxAget); //default set to false
    fWaveforms->doneFPN.resize(fMaxAsad*fMaxAget); //default set to false
    for(Int_t i=0;i<fMaxAsad;i++){
        for(Int_t j=0;j<fMaxAget;j++){
            fWaveforms->waveform[i*fMaxAget+j].resize(fMaxChannels);
            fWaveforms->hasSignal[i*fMaxAget+j].resize(fMaxChannels); //default set to false
            for(Int_t k=0;k<fMaxChannels;k++){
                fWaveforms->waveform[i*fMaxAget+j][k].resize(fMaxTimeBuckets);
            }
        }
    }
}

void LKFrameBuilder::ResetWaveforms()
{
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
    for(Int_t i=0;i<fMaxAsad;i++){
        for(Int_t j=0;j<fMaxAget;j++){
            if(fWaveforms->hasHit[i*fMaxAget+j] || fWaveforms->hasFPN[i*fMaxAget+j]){
                for(Int_t k=0;k<fMaxChannels;k++){
                    if(fWaveforms->hasSignal[i*fMaxAget+j][k]){
                        fWaveforms->hasSignal[i*fMaxAget+j][k] = false;
                        fill(fWaveforms->waveform[i*fMaxAget+j][k].begin(),
                                fWaveforms->waveform[i*fMaxAget+j][k].end(),0);
                    }
                }
            }
            fWaveforms->hasHit[i*fMaxAget+j] = false;
            fWaveforms->hasFPN[i*fMaxAget+j] = false;
            fWaveforms->doneFPN[i*fMaxAget+j] = false;

        }
    }
    fWaveforms->ICenergy = 0;
    fWaveforms->Sienergy = 0;
    fWaveforms->SiX = 0;
    fWaveforms->SiY = 0;
    fWaveforms->SiZ = 0;
    fWaveforms->isRejected = 0;
}

void LKFrameBuilder::ValidateEvent(mfm::Frame& frame)
{
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
    if(frame.header().isLayeredFrame())
    {
        unique_ptr<mfm::Frame> subFrame;
        for(Int_t i = 0;i<frame.itemCount();i++)
        {
            // frameAt ///////////////////////////////////////////////////////////////////////////////
            try { subFrame = frame.frameAt(i); }
            catch (const std::exception& e) {
                e_error << "Error at frameAt("<<i<<")" << endl;
                e_cout << e.what() << endl;
                return;
            }

            // subFrame.get, itemCount ///////////////////////////////////////////////////////////////
            Int_t numItems;
            try { numItems = (*subFrame.get()).itemCount(); /*Make sure we have data*/ }
            catch (const std::exception& e){
                e_error << "Error at (*subFrame.get()).itemCount()" << endl;
                e_cout << e.what() << endl;
                return;
            }

            // subFrame.get(), ValidateFrame /////////////////////////////////////////////////////////
            try {
                if(numItems>0) { ValidateFrame(*subFrame.get()); }
                else{
                }
            }
            catch (const std::exception& e) {
                e_error << "Error at ValidateFrame" << endl;
                e_cout << e.what() << endl;
                return;
            }
            //////////////////////////////////////////////////////////////////////////////////////////
        }
    }
    else {
        if(frame.itemCount()>0){ //Make sure we have data
            lk_info << "Frame is single frame" << endl;
            ValidateFrame(frame);
        }
        else {
        }
    }
}

void LKFrameBuilder::ValidateFrame(mfm::Frame& frame)
{
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
    /*
    UInt_t coboIdx = frame.headerField("coboIdx").value<UInt_t>();
    UInt_t asadIdx = frame.headerField("asadIdx").value<UInt_t>();
    UInt_t ceventIdx = (Int_t)frame.headerField("eventIdx").value<UInt_t>();

    //unsigned short SiMask = BOOST_BINARY( 111011111111111011111111110111111111111111111111101111111111011111111111 );
    ULong_t SiMask = 0xFEFFDFFFFFBFF7FF; //LSB 8byte
    //unsigned short MMMask = BOOST_BINARY( 111011111111111011111111110111111111111111111111101111111111011111111111 );
    ULong_t MMMask = 0xFEFFDFFFFFBFF7FF; //LSB 8byte
    //unsigned short MMMask1 = BOOST_BINARY( 000000000000000000000000000000000000000111111111101111111111011111111111 );
    ULong_t MMMask1 = 0x3FFBFF7FF; //LSB 8byte
    //unsigned short IcMask = BOOST_BINARY( 000000100000000000000000000000000000000000000000000000000000000000000000 );
    ULong_t IcMask = 0x13; // Channel 19

    //lk_info << coboIdx << " " << asadIdx << "(LSB):" << endl;
    ULong_t hitPat_0 = frame.headerField(32u, 8u).value<ULong_t>();
    ULong_t hitPat_1 = frame.headerField(41u, 8u).value<ULong_t>();
    ULong_t hitPat_2 = frame.headerField(50u, 8u).value<ULong_t>();
    ULong_t hitPat_3 = frame.headerField(59u, 8u).value<ULong_t>();
    //lk_info << "HitPattern 0: " << hex << hitPat_0 << endl;
    //lk_info << "HitPattern 1: " << hex << hitPat_1 << endl;
    //lk_info << "HitPattern 2: " << hex << hitPat_2 << endl;
    //lk_info << "HitPattern 3: " << hex << hitPat_3 << endl;
    //lk_info << "===========================" << endl;
    //000100000000000100000000001000000000000000000000010000000000100000000000
    //Si and IC data validation
    if(coboIdx==1 && asadIdx==0 && ((hitPat_0&SiMask)||(hitPat_1&SiMask)||(hitPat_2&SiMask)||(hitPat_3&SiMask))){
        fGoodSiEvent = true;
    }

    //MM and IC data validation
    //if(coboIdx==0 && ((hitPat_0&MMMask)||(hitPat_1&MMMask)||(hitPat_2&MMMask)||(hitPat_3&MMMask)))
    if(coboIdx==0 && ((asadIdx==2||asadIdx==3) && hitPat_2&MMMask1) && (asadIdx==0 && (hitPat_0&MMMask)) && (asadIdx<2 && ((hitPat_1&MMMask)||(hitPat_2&MMMask)||(hitPat_3&MMMask))))
    {
        fGoodMMEvent = true;
    }

    //lk_info << coboIdx << " " << asadIdx << "(USB):" << endl;
    hitPat_0 = frame.headerField(31u, 1u).value<ULong_t>();
    hitPat_1 = frame.headerField(40u, 1u).value<ULong_t>();
    hitPat_2 = frame.headerField(49u, 1u).value<ULong_t>();
    hitPat_3 = frame.headerField(58u, 1u).value<ULong_t>();
    //lk_info << "HitPattern 0: " << hex << hitPat_0 << endl;
    //lk_info << "HitPattern 1: " << hex << hitPat_1 << endl;
    //lk_info << "HitPattern 2: " << hex << hitPat_2 << endl;
    //lk_info << "HitPattern 3: " << hex << hitPat_3 << endl;
    //lk_info << "===========================" << endl;
    //000100000000000100000000001000000000000000000000010000000000100000000000

    //Si and IC data validation
    //unsigned short SiMask = BOOST_BINARY( 111011111111111011111111110111111111111111111111101111111111011111111111 );
    SiMask = 0xEF; //USB 1byte
    if(coboIdx==1 && asadIdx==0 && ((hitPat_0&SiMask)||(hitPat_1&SiMask)||(hitPat_2&SiMask)||(hitPat_3&SiMask))){
        fGoodSiEvent = true;
    }

    //MM and IC data validation
    MMMask = 0xEF; //USB 1byte
    //if(coboIdx==0 && ((hitPat_0&MMMask)||(hitPat_1&MMMask)||(hitPat_2&MMMask)||(hitPat_3&MMMask)))
    if(coboIdx==0 && (asadIdx==0 && (hitPat_0&MMMask)) && (asadIdx<2 && ((hitPat_1&MMMask)||(hitPat_2&MMMask)||(hitPat_3&MMMask))))
    {
        fGoodMMEvent = true;
    }
    if(coboIdx==1 && asadIdx==1 && (hitPat_0&IcMask))
    {
        fGoodICEvent = true;
    }
    */
}

void LKFrameBuilder::Event(mfm::Frame& frame)
{
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
    fWaveforms->frameIdx = 0;
    fWaveforms->decayIdx = 0;
    fChannelArray -> Clear("C");
    fCountChannels = 0;
    if(frame.header().isLayeredFrame()) {
        for(Int_t i = 0;i<frame.itemCount();i++) {
            //lk_info << "1isLayered=" << frame.header().isLayeredFrame() << ", itemCount=" << frame.itemCount() << ", frameIndex=" << i << endl;
            fWaveforms->frameIdx = 0;
            fWaveforms->decayIdx = 0;
            try{
                auto_ptr<mfm::Frame> subFrame = frame.frameAt(i);
                if((*subFrame.get()).itemCount()>0){ //Make sure we have data
                    //lk_info << "2isLayered=" << frame.header().isLayeredFrame() << ", itemCount=" << frame.itemCount() << ", frameIndex=" << i << endl;
                    fWaveforms->frameIdx = i;
                    UnpackFrame(*subFrame.get());
                    WriteChannels();
                    ResetWaveforms();
                }else{
                    //lk_info << "2no subframe" << endl;
                }
            }catch (const std::exception& e){
                lk_info << e.what() << endl;
                return;
            }
        }
    } else {
        try{
            if(frame.itemCount()>0){ //Make sure we have data
                //lk_info << "Not Layered Frame" << endl;
                UnpackFrame(frame);
                WriteChannels();
                ResetWaveforms();
            }else{
                lk_info << "3no subframe" << endl;
            }
        }catch (const std::exception& e){
            lk_info << e.what() << endl;
            return;
        }
    }
}

void LKFrameBuilder::UnpackFrame(mfm::Frame& frame)
{
#ifdef DEBUG_UNPACKFRAME_CUT
//#ifdef DEBUG_LKFRAMEBUILDER
    if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
        lk_debug << endl;
#endif
    Int_t prevWEventIdx = fCurrEventIdx;
    UInt_t coboIdx = frame.headerField("coboIdx").value<UInt_t>();
    UInt_t asadIdx = frame.headerField("asadIdx").value<UInt_t>();
    UInt_t itemSize = frame.headerField("itemSize").value<UInt_t>();
    UInt_t frameSize = frame.headerField("frameSize").value<UInt_t>();
    fEventTime = frame.headerField("eventTime").value<UInt_t>();
    fCurrEventIdx = (Int_t)frame.headerField("eventIdx").value<UInt_t>();
#ifdef DEBUG_UNPACKFRAME_CUT
//#ifdef DEBUG_LKFRAMEBUILDER
    if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
    lk_debug << "UnpackFrame Cobo=" << coboIdx << " Asad=" << asadIdx << " itemSize=" << itemSize << " frameSize=" << frameSize << endl;
#endif
    if(fIsFirstEvent) {
        fFirstEventIdx = fCurrEventIdx;
        fIsFirstEvent=false;
    }

    if (prevWEventIdx != fCurrEventIdx)
    {
        if (fMultGET>0)
        {
            fGETEventIdx = prevWEventIdx;

            if(fSet2PMode)
            {
                for(Int_t i=fPrevEventIdx;i<fListOfEventIdx.size();i++)
                {
                    if(fListOfEventIdx.at(i)==fGETEventIdx)
                    {
                        fGETD2PTime = fListOfD2PTime.at(i);
                        fGETTimeStamp = fListOfTimeStamp.at(i);
                        fPrevEventIdx = i;
                    }
                }
            }

            fGETEventIdx = fCurrEventIdx;

            fMultGET=0;
            for (Int_t i=0; i<fMaxCobo; i++)
                for (Int_t j=0; j<fMaxAsad; j++)
                    fAsadIsTriggered[i][j] = 0;
        }
    }

    mfm::Item item = frame.itemAt(0u);
    mfm::Field field = item.field("");

    const size_t numSamples = frame.itemCount();
    const size_t numChannels = 68u;
    const size_t numChips = 4u;
    vector<uint32_t> chanIdx_(numChips, 0u);
    vector<uint32_t> buckIdx_(numChips, 0u);

    if(frame.header().frameType() == 1u)
    {
#ifdef DEBUG_UNPACKFRAME_CUT
    if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
        lk_debug << "frame type 1u" << endl;
#endif
        mfm::Item item = frame.itemAt(0u);
        mfm::Field field = item.field("");
        mfm::BitField agetIdxField = field.bitField("agetIdx");
        mfm::BitField chanIdxField = field.bitField("chanIdx");
        mfm::BitField buckIdxField = field.bitField("buckIdx");
        mfm::BitField sampleValueField = field.bitField("sample");

        if(fSet2PMode){
            if(fAsadIsTriggered[coboIdx][asadIdx]>0){
                fAsadIsTriggered[coboIdx][asadIdx]=2;
                fWaveforms->decayIdx = 1;
            }else{
                fAsadIsTriggered[coboIdx][asadIdx]=1;
                fWaveforms->decayIdx = 0;
            }
        }else{
#ifdef DEBUG_UNPACKFRAME_CUT
            if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
                lk_debug << fAsadIsTriggered << " " << coboIdx << " " << asadIdx << endl;
#endif
            fAsadIsTriggered[coboIdx][asadIdx]=0;
            fWaveforms->decayIdx = 0;
        }
#ifdef DEBUG_UNPACKFRAME_CUT
        if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
            lk_debug << "Type:" << frame.header().frameType() << " " <<  fCurrEventIdx << "  " << frameSize << " " << coboIdx << " " << asadIdx << " " << fWaveforms->frameIdx << " " << fWaveforms->decayIdx << " " << frame.itemCount() << endl;
#endif
        Int_t lastaget=-1;
        for(UInt_t i=0; i<frame.itemCount(); i++) {
#ifdef DEBUG_UNPACKFRAME_CUT
            if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
                lk_debug << ">>>> " << i << endl;
#endif
            item = frame.itemAt(i);
            field = item.field(field);
            agetIdxField = field.bitField(agetIdxField);
            chanIdxField = field.bitField(chanIdxField);
            buckIdxField = field.bitField(buckIdxField);
            sampleValueField =field.bitField(sampleValueField);

            UInt_t agetIdx = agetIdxField.value<UInt_t>();
            //if(agetIdx==lastaget) break;
            lastaget = agetIdx;

#ifdef DEBUG_UNPACKFRAME_CUT
            if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
                lk_debug << ">>>> " << i << " agetIdx " << agetIdx << endl;
#endif
            if(agetIdx<0 || agetIdx>3) return;
            UInt_t chanIdx = chanIdxField.value<UInt_t>();
            if (chanIdx>67) continue;
            UInt_t buckIdx = buckIdxField.value<UInt_t>();
            UInt_t sampleValue = sampleValueField.value<UInt_t>();

            fWaveforms->coboIdx = coboIdx;
            fWaveforms->asadIdx = asadIdx;
#ifdef DEBUG_UNPACKFRAME_CUT
            if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
                lk_debug << ">>>> " << i << " chanIdx " << chanIdx << endl;
#endif

            {
                fWaveforms->hasSignal[asadIdx*fMaxAget+agetIdx][chanIdx] = true;
                fWaveforms->hasHit[asadIdx*fMaxAget+agetIdx] = true;
                fWaveforms->waveform[asadIdx*fMaxAget+agetIdx][chanIdx][buckIdx] = sampleValue;
            }
#ifdef DEBUG_UNPACKFRAME_CUT
            if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
                lk_debug << ">>>> " << i << " chanIdx " << chanIdx << endl;
#endif
        }

    }
    else if (frame.header().frameType() == 2u)
    {
#ifdef DEBUG_UNPACKFRAME_CUT
        if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
            lk_debug << "frame type 2u" << endl;
#endif
        if(fSet2PMode){
            if(fAsadIsTriggered[coboIdx][asadIdx]>0){
                fAsadIsTriggered[coboIdx][asadIdx]=2;
                fWaveforms->decayIdx = 1;
            }else{
                fAsadIsTriggered[coboIdx][asadIdx]=1;
                fWaveforms->decayIdx = 0;
            }
        }else{
            fAsadIsTriggered[coboIdx][asadIdx]=0;
            fWaveforms->decayIdx = 0;
        }
        // Decode first item
        if(numSamples>0) {
            mfm::Item item = frame.itemAt(0u);
            mfm::Field field = item.field("");
            mfm::BitField agetIdxField = field.bitField("agetIdx");
            mfm::BitField sampleValueField = field.bitField("sample");

            const uint32_t agetIdx = agetIdxField.value<uint32_t>();
            const uint32_t sampleValue = sampleValueField.value<uint32_t>();

            fWaveforms->coboIdx = coboIdx;
            fWaveforms->asadIdx = asadIdx;

            {
                fWaveforms->hasSignal[asadIdx*fMaxAget+agetIdx][chanIdx_[agetIdx]] = true;
                fWaveforms->hasHit[asadIdx*fMaxAget+agetIdx] = true;
                fWaveforms->waveform[asadIdx*fMaxAget+agetIdx][chanIdx_[agetIdx]][buckIdx_[agetIdx]] = sampleValue;
            }
        }
        // Loop through other items
        for(size_t itemId=1; itemId<numSamples; ++itemId) {
            item = frame.itemAt(itemId);
            field = item.field(field);
            mfm::BitField agetIdxField = field.bitField(agetIdxField);
            mfm::BitField sampleValueField = field.bitField(sampleValueField);

            {
                const uint32_t agetIdx = agetIdxField.value<uint32_t>();
                if(chanIdx_[agetIdx]>=numChannels) {
                    chanIdx_[agetIdx] = 0u;
                    buckIdx_[agetIdx]++;
                }
                const uint32_t sampleValue = sampleValueField.value<uint32_t>();

                fWaveforms->hasSignal[asadIdx*fMaxAget+agetIdx][chanIdx_[agetIdx]] = true;
                fWaveforms->hasHit[asadIdx*fMaxAget+agetIdx] = true;
                fWaveforms->waveform[asadIdx*fMaxAget+agetIdx][chanIdx_[agetIdx]][buckIdx_[agetIdx]] = sampleValue;
                chanIdx_[agetIdx]++;
            }
        }
    }
    else {
        lk_info << "Frame type " << frame.header().frameType() << " not found" << endl;
        return;
    }
#ifdef DEBUG_UNPACKFRAME_CUT
    if (((LKMFMConversionTask*)fMotherTask)->GetFileBuffer()>DEBUG_UNPACKFRAME_CUT)
        lk_debug << "end" << endl;
#endif
}

void LKFrameBuilder::decodeCoBoTopologyFrame(mfm::Frame& frame)
{ 
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
    if (frame.header().frameType() == 0x7){
        // Print meta-data
        //lk_info << "Topology:" << endl;
        // Skip 8 Byte header
        istream & stream = frame.serializer().inputStream(8u);
        uint8_t value;
        stream.read(reinterpret_cast<char *>(&value), 1lu);
        //lk_info << " coboIdx=" << (uint16_t) value << ", ";
        stream.read(reinterpret_cast<char *>(&value), 1lu);
        //lk_info << " asadMask=" << hex << showbase << (uint16_t) value << dec << ", ";
        stream.read(reinterpret_cast<char *>(&value), 1lu);
        //lk_info << " 2pMode=" << boolalpha << (bool) value << endl;
    }
}

void LKFrameBuilder::decodeMuTanTFrame(mfm::Frame & frame)
{
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
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
    //double scaler4;
    //double scaler4start;
    //double scaler4end;
    //double scaler5;
    //double scaler5start;
    //double scaler5end;

    if (frame.header().frameType() == 0x8)
    {
        try
        {
            if(fSet2PMode){
                fListOfEventIdx.push_back(frame.headerField(14u, 4u).value< uint32_t >());
                fListOfD2PTime.push_back(frame.headerField(60u, 4u).value< uint32_t >()*10);
                fListOfTimeStamp.push_back(frame.headerField(8u, 6u).value< uint64_t >());
                lk_info << "decoded: " << fListOfEventIdx.at(fListOfEventIdx.size()-1) << " " << fListOfD2PTime.at(fListOfD2PTime.size()-1) << " " << fListOfTimeStamp.at(fListOfTimeStamp.size()-1) << endl;
            }else{
                scaler1 = frame.headerField(48u, 4u).value< uint32_t >();
                scaler2 = frame.headerField(52u, 4u).value< uint32_t >();
                scaler3 = frame.headerField(56u, 4u).value< uint32_t >();
                //scaler4 = frame.headerField(40u, 4u).value< uint32_t >();
                //scaler5 = frame.headerField(44u, 4u).value< uint32_t >();
                uint32_t scalerEvent = frame.headerField(24u, 4u).value< uint32_t >();
                uint32_t scalerEventIdx = frame.headerField(14u, 4u).value< uint32_t >();
                uint32_t scalerTimeStamp = frame.headerField(8u, 6u).value< uint64_t >();
                //if(scaler3>0 && (Int_t)scaler3%1000000==0)
                if(fMutantCounter==100)
                {
                    scaler1end = scaler1;
                    scaler2end = scaler2;
                    scaler3end = scaler3;
                    lk_info << " Scaler 1 Rate = " << 100*(scaler1end-scaler1start)/(scaler3end-scaler3start) << ", " << (scaler1end-scaler1start) << " " << (scaler3end-scaler3start) << " " << scaler1end << " " << scaler1start << " " << scaler3end << " " << scaler3start << " or " << (100*scaler1end/scaler3end) << endl;
                    lk_info << " Scaler 2 Rate(Live) = " << 100*(scaler2end-scaler2start)/(scaler3end-scaler3start) << ", " << scaler2end << " " << scaler2start << " " << scaler3end << " " << scaler3start << " or " << (100*scaler2end/scaler3end) << endl;
                    if(fSetScaler)
                    {
                        fFileScaler.open(fScalerFile.Data(), std::ofstream::out|std::ofstream::app);
                        fFileScaler << scalerTimeStamp << " " << scalerEvent << " " << scalerEventIdx << " " << scaler1 << " " << scaler2 << " " << scaler3 << endl;
                        fFileScaler.close();
                    }
                    fMutantCounter=0;
                }
                //else if(scaler3>0 && (Int_t)scaler3%1000000==1)
                else if(fMutantCounter==1)
                {
                    scaler1start = scaler1;
                    scaler2start = scaler2;
                    scaler3start = scaler3;
                }
                if(fSetScaler==false)
                {
                    fCountPrint++;
                    if(fCountPrint>15)
                    {
                        fCountPrint=0;
                        lk_info << "Mutant: ";
                        e_cout  << " TSTMP=" << frame.headerField(8u, 6u).value< uint64_t >() << ", ";
                        e_cout  << " EVT_NO=" << frame.headerField(14u, 4u).value< uint32_t >() << ", ";
                        e_cout  << " TRIG=" << hex << showbase << frame.headerField(18u, 2u).value< uint16_t >() << dec << ", ";
                        e_cout  << " MULT_A="  << frame.headerField(20u, 2u).value< uint16_t >() << ", ";
                        e_cout  << " MULT_B="  << frame.headerField(22u, 2u).value< uint16_t >() << ", ";
                        e_cout  << " L0_EVT="  << frame.headerField(24u, 4u).value< uint32_t >() << ", ";
                        e_cout  << " L1A_EVT="  << frame.headerField(28u, 4u).value< uint32_t >() << ", ";
                        e_cout  << " L1B_EVT="  << frame.headerField(32u, 4u).value< uint32_t >() << ", ";
                        e_cout  << " L2_EVT="  << frame.headerField(36u, 4u).value< uint32_t >() << ", ";
                        e_cout  << " SCA1="  << frame.headerField(48u, 4u).value< uint32_t >() << ", ";
                        e_cout  << " SCA2="  << frame.headerField(52u, 4u).value< uint32_t >() << ", ";
                        e_cout  << " SCA3="  << frame.headerField(56u, 4u).value< uint32_t >() << ", ";
                        e_cout  << " SCA4="  << frame.headerField(40u, 4u).value< uint32_t >() << ", ";
                        e_cout  << " SCA5="  << frame.headerField(44u, 4u).value< uint32_t >() << ", ";
                        e_cout  << " D2P="  << frame.headerField(60u, 4u).value< uint32_t >() << endl;

                        lk_info << scaler1 << " " << scaler2 << " " << scaler3
                            << ", IC_Rate=" << Form("%5.2f",(100*scaler1/scaler3)) << "pps"
                            << ", Live=" << Form("%5.2f",((100*scaler2)/scaler3)) << "\%" << endl;
                    }
                }
            }
        } catch (const mfm::Exception &)
        {}
    }
}

void LKFrameBuilder::WriteChannels()
{
#ifdef DEBUG_LKFRAMEBUILDER
    lk_debug << endl;
    lk_info << endl;
#endif
    UInt_t frameIdx = fWaveforms->frameIdx;
    UInt_t decayIdx = fWaveforms->decayIdx;
    UInt_t coboIdx = fWaveforms->coboIdx;

    auto eventHeader = (LKEventHeader *) fEventHeaderArray -> ConstructedAt(0);
    eventHeader -> SetEventNumber(Int_t(fCurrEventIdx));
    eventHeader -> SetEventTime(fEventTime);

    for (UInt_t asad=0; asad<fMaxAsad; asad++)
    {
        for (UInt_t aget=0; aget<fMaxAget; aget++)
        {
            if (!fWaveforms->hasHit[asad*fMaxAget+aget])
                continue; // Skip no fired agets.

            for (UInt_t chan=0; chan<fMaxChannels; chan++)
            {
                if (!fWaveforms->hasSignal[asad*fMaxAget+aget][chan])
                    continue; // Skip no fired channels.

                if (coboIdx>=0)
                {
                    auto channel = (GETChannel *) fChannelArray -> ConstructedAt(fCountChannels);
                    channel -> SetChannelID(fCountChannels);
                    //channel -> SetFrameNo(frameIdx);
                    //channel -> SetDecayNo(decayIdx);
                    channel -> SetCobo(coboIdx);
                    channel -> SetAsad(asad);
                    channel -> SetAget(aget);
                    channel -> SetChan(chan);
                    channel -> SetWaveformY(fWaveforms->waveform[asad*fMaxAget+aget][chan]);

                    double time = 0.;
                    double energy = 0.;
                    double pedestal = 0.;
                    auto buffer = channel -> GetWaveformY();
                    fChannelAnalyzer -> Analyze(buffer);
                    if (fChannelAnalyzer->GetNumHits()>0) {
                        time = fChannelAnalyzer -> GetTbHit(0);
                        energy = fChannelAnalyzer -> GetAmplitude(0);
                        pedestal = fChannelAnalyzer -> GetPedestal();
                    }
                    channel -> SetTime(time);
                    channel -> SetEnergy(energy);
                    channel -> SetPedestal(pedestal);

                    fCountChannels++;
                    fMultGET++;
                }
            }
        }
    }
}
