#ifndef LKGETRAWCONVERTER_HH
#define LKGETRAWCONVERTER_HH

#include "TClonesArray.h"
#include "TObject.h"

#include "LKChannelAnalyzer.h"
#include "LKParameterContainer.h"
#include "LKGETRawFrame.h"

#include <fstream>
#include <vector>

class LKGETRawConverter : public TObject
{
  public:
    LKGETRawConverter();
    virtual ~LKGETRawConverter();

    void SetPar(LKParameterContainer* par);
    void SetChannelArray(TClonesArray* array) { fChannelArray = array; }
    void SetEventHeaderArray(TClonesArray* array) { fEventHeaderArray = array; }

    bool Init();
    void ProcessFrame(const LKGETRawFrame& frame);

  private:
    struct WaveForms {
        Bool_t isRejected = false;
        std::vector<std::vector<Bool_t>> hasSignal;
        std::vector<Bool_t> hasHit;
        std::vector<Bool_t> hasFPN;
        std::vector<Bool_t> doneFPN;
        UInt_t frameIdx = 0;
        UInt_t decayIdx = 0;
        UInt_t ICenergy = 0;
        UInt_t Sienergy = 0;
        Double_t SiX = 0;
        Double_t SiY = 0;
        Double_t SiZ = 0;
        UInt_t coboIdx = 0;
        UInt_t asadIdx = 0;
        std::vector<std::vector<std::vector<UInt_t>>> waveform;
    };

    void InitWaveforms();
    void ResetWaveforms();
    void Event(const LKGETRawFrame& frame);
    void UnpackFrame(const LKGETRawFrame& frame);
    void WriteChannels();
    void DecodeCoBoTopologyFrame(const LKGETRawFrame& frame);
    void DecodeMuTanTFrame(const LKGETRawFrame& frame);

    static uint32_t ReadBE32(const uint8_t* data);
    static uint64_t ReadBE48(const uint8_t* data);
    static uint32_t ReadType1Word(const uint8_t* data);
    static uint16_t ReadType2Word(const uint8_t* data);

  private:
    TString fName = "LKGETRawConverter";
    TClonesArray* fChannelArray = nullptr;
    TClonesArray* fEventHeaderArray = nullptr;

    LKParameterContainer* fPar = nullptr;
    LKChannelAnalyzer* fChannelAnalyzer = nullptr;
    WaveForms* fWaveforms = nullptr;

    Int_t fCountChannels = 0;
    const Int_t fMaxTimeBuckets = 512;
    Bool_t fSet2PMode = false;
    Bool_t fSetScaler = false;
    Int_t fMaxCobo = 4;
    Int_t fMaxAsad = 4;
    Int_t fMaxAget = 4;
    Int_t fMaxChannels = 68;

    TString fScalerFile = "";
    std::ofstream fFileScaler;

    Bool_t fIsFirstEvent = true;
    Int_t fFirstEventIdx = -1;
    UInt_t fEventTime = 0;
    Int_t fCurrEventIdx = -1;
    Int_t fPrevEventIdx = 0;
    Int_t fMutantCounter = 0;
    Int_t** fAsadIsTriggered = nullptr;

    std::vector<UInt_t> fListOfEventIdx;
    std::vector<UInt_t> fListOfTimeStamp;
    std::vector<UInt_t> fListOfD2PTime;

    Int_t fMultGET = 0;
    Int_t fGETEventIdx = 0;
    Int_t fGETTimeStamp = 0;
    Int_t fGETD2PTime = 0;

    Long64_t fCountEvents = 0;

    ClassDef(LKGETRawConverter, 1);
};

#endif
