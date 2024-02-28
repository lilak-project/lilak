#ifndef LKFRAMEBUILDER_H
#define LKFRAMEBUILDER_H

#include "mfm/FrameBuilder.h"
#include <map>
#include <vector>
#include <TFile.h>
#include <TTree.h>
#include <TTree.h>
#include <TKey.h>
#include <TH1F.h>
#include <TH2I.h>
#include <TGraph2D.h>
#include <TProfile.h>
#include <TSpectrum.h>
#include <TF1.h>
#include <TF2.h>
#include <TRandom.h>
#include <TMath.h>
#include <TCutG.h>
#include <string.h>
#include <TError.h>
#include <fstream>

#include "TClonesArray.h"

#include "LKTask.h"
#include "LKParameterContainer.h"

using namespace std;

class WaveForms {
    public:
        WaveForms() {};
        ~WaveForms() {};
        Bool_t isRejected;
        vector<vector<Bool_t>> hasSignal; // To let us know if there was a signal that fired
        vector<Bool_t> hasHit; // To let us know if there was any signal other than FPNs
        vector<Bool_t> hasFPN; // To let us know if there was any FPN signal
        vector<Bool_t> doneFPN; // To let us know if there was any FPN signal averaged
        UInt_t frameIdx;
        UInt_t decayIdx;
        UInt_t ICenergy;
        UInt_t Sienergy;
        Double_t SiX;
        Double_t SiY;
        Double_t SiZ;
        UInt_t coboIdx;
        UInt_t asadIdx;
        vector<vector<vector<UInt_t>>> waveform; // To save the waveform for each Aget, Channel and Bucket
};

/// @todo mfm::FrameDictionary::instance().addFormats(fFormatFileName.Data());
/// @todo frame.headerField(56u, 4u).value< uint32_t >
/// @todo frame.header().frameType()  == 0x8
/// @todo fListOfEventIdx.push_back(frame.headerField(14u, 4u).value< uint32_t >());
/// @todo 2pmode
/// @todo scaler
class LKFrameBuilder : public mfm::FrameBuilder
{
    public:
        LKFrameBuilder();
        ~LKFrameBuilder();

        void processFrame(mfm::Frame & frame);
        void decodeMuTanTFrame(mfm::Frame & frame); // processFrame
        void decodeCoBoTopologyFrame(mfm::Frame& frame); // processFrame
        void ValidateEvent(mfm::Frame & frame);
        void ValidateFrame(mfm::Frame& frame);
        void Event(mfm::Frame & frame);
        void ResetWaveforms(); // Event
        void UnpackFrame(mfm::Frame& frame); // Event
        void WriteChannels();

        void SetMotherTask(LKTask* task) { fMotherTask = task; } /// Must be set from mother task
        void SetChannelArray(TClonesArray *array) { fChannelArray = array; }
        void SetEventHeaderArray(TClonesArray *array) { fEventHeaderArray = array; }

        void SetPar(LKParameterContainer* par);
        //void Set2pMode(Bool_t flag) { fSet2PMode = flag; }
        //void SetScaler(Bool_t flag) { fSetScaler = flag; }
        //void SetIgnoreMM(Bool_t value) { fIgnoreMM = value; }
        //void SetMaxCobo(Int_t value) { fMaxCobo = value; }
        //void SetMaxAsad(Int_t value) { fMaxAsad = value; }
        //void SetMaxAget(Int_t value) { fMaxAget = value; }
        //void SetMaxChannels(Int_t value) { fMaxChannels = value; }
        //void SetFormatFile(TString fileName) { fFormatFileName = fileName; }
        //void SetScalerFile(TString fileName) { fScalerFileName = fileName; }

        void InitWaveforms();
        bool Init();

    private:
        TClonesArray *fChannelArray = nullptr;
        TClonesArray *fEventHeaderArray = nullptr;
        LKTask *fMotherTask;
        TString fName;
        Int_t fCountChannels = 0;

        LKParameterContainer *fPar = nullptr;
        const Int_t fMaxTimeBuckets = 512;
        Bool_t fSet2PMode = false;
        Bool_t fSetScaler = false;
        Int_t fMaxCobo = 4;
        Int_t fMaxAsad = 4;
        Int_t fMaxAget = 4;
        Int_t fMaxChannels = 68;

        TString fFormatFileName = "/usr/local/get/share/get-bench/format/CoboFormats.xcfg"; //
        TString fScalerFileName = "scalers.txt";
        ofstream fFileScaler;

        WaveForms* fWaveforms;

        Bool_t fIsFirstEvent = true;
        Int_t fFirstEventIdx = -1;
        Int_t fCurrEventIdx = -1;
        Int_t fPrevEventIdx = 0;
        Int_t fCountPrint = 0;
        Int_t fMutantCounter = 0;
        Int_t** fAsadIsTriggered = nullptr;

        vector<UInt_t> fListOfEventIdx; // eventidx value
        vector<UInt_t> fListOfTimeStamp; // timestamp value
        vector<UInt_t> fListOfD2PTime; // d2ptime value

        Int_t fMultGET = 0;
        Int_t fGETEventIdx;
        Int_t fGETTimeStamp;
        Int_t fGETD2PTime;
};

#endif
