#ifndef LKCOMPASSRECO_H
#define LKCOMPASSRECO_H

#include "LKComChannel.h"
#include "LKComHit.h"
#include "LKComW1Channel.h"
#include "LKComW1Hit.h"

#include "TClonesArray.h"
#include "TString.h"
#include "TTree.h"

#include <map>
#include <vector>

class TFile;
class TH1D;
class TH2D;
class TDirectory;
class LKBinning;
class LKDrawingGroup;

class LKCompassReco
{
    public:
        LKCompassReco();
        virtual ~LKCompassReco();

        /// Set input ROOT file. Data_R raw trees and LKCompassReco event trees are supported.
        void SetInputFile(TString name) { fInputFileName = name; }
        /// Set output ROOT file. If unset, a default .reco.root or .recoN.root name is generated.
        void SetOutputFile(TString name) { fOutputFileName = name; }
        /// Set raw-hit grouping time window for Data_R input.
        void SetTimeWindow(Long64_t value) { fTimeWindow = value; }
        /// Limit input entries. lastEntry=0 means run to the end.
        void SetEntryRange(Long64_t firstEntry=0, Long64_t lastEntry=0);
        /// Enable or disable output channel branches.
        void SetChannelBranches(bool value=true) { fStoreChannelBranches = value; }
        /// Enable or disable output hit branches.
        void SetHitBranches(bool value=true) { fStoreHitBranches = value; }
        /// Use junction/Y-side W1 channels to define hit energy.
        void SetW1MainJunction() { fW1MainSide = false; }
        /// Use ohmic/X-side W1 channels to define hit energy.
        void SetW1MainOhmic() { fW1MainSide = true; }
        /// Set W1 energy calibration: ECal = c0 + c1*E + c2*E*E for one detector ID.
        void SetECalParametersW1(short detID, double c0, double c1, double c2=0);
        /// Set Com energy calibration: ECal = c0 + c1*E + c2*E*E for one detector ID.
        void SetECalParametersCom(short detID, double c0, double c1, double c2=0);
        /// Set W1 histogram binning for one detector ID and histogram type.
        void SetHistogramBinningW1(short detID, TString type, const LKBinning& binning);
        /// Set W1 histogram binning for one detector ID and histogram type.
        void SetHistogramBinningW1(short detID, TString type, int nBins, double xMin, double xMax);
        /// Set Com histogram binning for one detector ID and histogram type.
        void SetHistogramBinningCom(short detID, TString type, const LKBinning& binning);
        /// Set Com histogram binning for one detector ID and histogram type.
        void SetHistogramBinningCom(short detID, TString type, int nBins, double xMin, double xMax);

        /// Register one W1 detector map for raw Data_R input.
        void AddW1(short detID, short board, short yChannelStart, short xChannelStart);
        /// Register one Com detector map for raw Data_R input.
        void AddCom(short detID, short board, short channel);

        /// Initialize input, output, tree branches, detector maps, and histograms.
        bool Init();
        /// Run reconstruction or ECal update. The output tree and drawing group are saved.
        void Run();
        /// Draw the detector summary drawing group.
        void Draw(Option_t* option="");
        /// Write the output tree and drawing group.
        void End();

    private:
        struct W1Map {
            TString name;
            short detID;
            short board;
            short yStart;
            short xStart;
        };

        struct ComMap {
            TString name;
            short detID;
            short board;
            short channel;
        };

        struct W1Hist {
            short detID;
            TString name;
            TH2D* hitPattern = nullptr;
            TH1D* channelMult = nullptr;
            TH1D* hitEnergy = nullptr;
            TH1D* hitECal = nullptr;
            TH1D* xEnergy = nullptr;
            TH1D* yEnergy = nullptr;
            TH1D* xECal = nullptr;
            TH1D* yECal = nullptr;
            TH1D* previousEntryTimeDiff = nullptr;
        };

        struct ComHist {
            short detID;
            TString name;
            TH1D* channelMult = nullptr;
            TH1D* hitEnergy = nullptr;
            TH1D* hitECal = nullptr;
            TH1D* channelEnergy = nullptr;
            TH1D* channelECal = nullptr;
            TH1D* previousEntryTimeDiff = nullptr;
        };

        struct ECalPar {
            double c0 = 0;
            double c1 = 1;
            double c2 = 0;
        };

        void ClearEvent();
        void SetInputTree();
        void SetRecoInputTree();
        bool CollectRecoDetectorMaps();
        void MakeHistograms();
        void WriteHistograms(TDirectory* top);
        void WriteCanvases(TDirectory* top);
        void AddRawHitToEvent();
        void BuildHits();
        void BuildW1Hits();
        void BuildComHits();
        void RunRawReco();
        void RunECalUpdate();
        void UpdateChannelECal();
        void FillTimeDiffHistograms();
        void FillW1TimeDiff(short detID, ULong64_t time);
        void FillComTimeDiff(short detID, ULong64_t time);
        TString MakeDefaultOutputFileName() const;
        double CalibrateW1(short detID, short energy) const;
        double CalibrateCom(short detID, short energy) const;
        ECalPar GetW1ECalPar(short detID) const;
        ECalPar GetComECalPar(short detID) const;
        LKBinning GetW1HistBin(short detID, TString type) const;
        LKBinning GetComHistBin(short detID, TString type) const;
        bool CheckBinning(const LKBinning& binning) const;
        bool FindW1(UShort_t board, UShort_t channel, const W1Map*& map, bool& side, int& strip) const;
        bool FindCom(UShort_t board, UShort_t channel, const ComMap*& map) const;

        TString fInputFileName = "data/DataR_run0025_dummy.root"; //!
        TString fOutputFileName = ""; //!
        Long64_t fTimeWindow = 4000000; //!
        Long64_t fFirstEntry = 0; //!
        Long64_t fLastEntry = 0; //!
        bool fW1MainSide = false; //! false: junction(Y), true: ohmic(X)
        bool fStoreChannelBranches = true; //!
        bool fStoreHitBranches = true; //!
        bool fInputIsReco = false; //!

        TFile* fInputFile = nullptr; //!
        TFile* fOutputFile = nullptr; //!
        TTree* fInputTree = nullptr; //!
        TTree* fOutputTree = nullptr; //!
        LKDrawingGroup* fTopGroup = nullptr; //!

        UShort_t fChannel = 0; //!
        ULong64_t fTimestamp = 0; //!
        UShort_t fBoard = 0; //!
        UShort_t fEnergy = 0; //!
        UShort_t fEnergyShort = 0; //!
        UInt_t fFlags = 0; //!

        TClonesArray* fW1ChannelArray = nullptr; //!
        TClonesArray* fW1HitArray = nullptr; //!
        TClonesArray* fComChannelArray = nullptr; //!
        TClonesArray* fComHitArray = nullptr; //!

        std::vector<W1Map> fW1MapArray; //!
        std::vector<ComMap> fComMapArray; //!
        std::vector<W1Hist> fW1HistArray; //!
        std::vector<ComHist> fComHistArray; //!

        std::map<short,ECalPar> fW1ECalParMap; //!
        std::map<short,ECalPar> fComECalParMap; //!
        std::map<short,std::map<TString,LKBinning*>> fW1HistBinMap; //!
        std::map<short,std::map<TString,LKBinning*>> fComHistBinMap; //!
        std::map<short,ULong64_t> fPreviousW1EntryTimeMap; //!
        std::map<short,ULong64_t> fPreviousComEntryTimeMap; //!
};

#endif
