#ifndef LKCOMPASSRECOTASK_HH
#define LKCOMPASSRECOTASK_HH

#include "LKTask.h"

#include <map>
#include <vector>

class LKCompassReco;
class TClonesArray;
class TH1D;
class TH2D;
class TGraph;
class LKBinning;
class LKDrawingGroup;

/**
 * LILAK event-trigger task for CoMPASS ROOT input.
 *
 * LKCompassReco finds timestamp windows. This task maps the raw channels,
 * builds detector channels and hits, publishes them as LILAK branches, and
 * triggers the remaining task chain once per window.
 *
 * Parameters:
 * - LKCompassRecoTask/InputFileName: CoMPASS ROOT input (overrides LKRun input)
 * - LKCompassRecoTask/InputTreeName: raw tree name (default: Data_R)
 * - LKCompassRecoTask/TimeWindow: timestamp window with optional ps/ns/us/ms unit
 *   (unitless values use ps; default: 4000000 ps)
 * - LKCompassRecoTask/EnergyThreshold: minimum raw Energy to collect (default: 0)
 * - LKCompassRecoTask/FirstEntry, LastEntry: optional inclusive raw-entry range
 * - LKCompassRecoTask/SortInput: sort raw entries by Timestamp before events
 * - LKCompassRecoTask/W1Map: repeated detID,board,junctionStart,ohmicStart values
 * - LKCompassRecoTask/W1YOrigin: repeated detID,top|bottom values (default: top)
 * - LKCompassRecoTask/ComMap: repeated detID,board,channel values
 * - LKCompassRecoTask/W1ECal: repeated detID,c0,c1,c2 values
 * - LKCompassRecoTask/ComECal: repeated detID,c0,c1,c2 values
 * - LKCompassRecoTask/W1MainSide: junction or ohmic
 */
class LKCompassRecoTask : public LKTask
{
    public:
        LKCompassRecoTask();
        virtual ~LKCompassRecoTask();

        bool IsEventTrigger() override { return true; }
        bool Init() override;
        void Run(Long64_t numEvents=-1) override;
        void SignalNextEvent() override;
        bool EndOfRun() override { return true; }
        void AddTriggerInputFile(TString fileName, TString option) override;

        void SetInputFile(TString name);
        void SetInputTreeName(TString name) { fInputTreeName = name; }
        void SetTimeWindow(Long64_t valueInPs) { fTimeWindow = valueInPs; }
        bool SetTimeWindow(Double_t value, TString unit);
        void SetEnergyThreshold(Int_t value) { fEnergyThreshold = value; }
        void SetSortInput(bool value=true) { fSortInput = value; }
        void SetEntryRange(Long64_t firstEntry=0, Long64_t lastEntry=0);
        void SetChannelBranches(bool value=true) { fStoreChannelBranches = value; }
        void SetHitBranches(bool value=true) { fStoreHitBranches = value; }
        void SetW1MainJunction() { fW1MainSide = false; }
        void SetW1MainOhmic() { fW1MainSide = true; }

        void AddW1(short detID, short board, short junctionChannelStart, short ohmicChannelStart);
        void AddCom(short detID, short board, short channel);
        bool SetW1YOrigin(short detID, TString origin);
        void SetECalParametersW1(short detID, double c0, double c1, double c2=0);
        void SetECalParametersCom(short detID, double c0, double c1, double c2=0);
        void SetHistogramBinningW1(short detID, TString type, const LKBinning& binning);
        void SetHistogramBinningW1(short detID, TString type, int nBins, double xMin, double xMax);
        void SetHistogramBinningCom(short detID, TString type, const LKBinning& binning);
        void SetHistogramBinningCom(short detID, TString type, int nBins, double xMin, double xMax);
        void Draw(Option_t* option="") override;

    private:
        struct W1Map {
            short detID;
            short board;
            short junctionStart;
            short ohmicStart;
        };

        struct ComMap {
            short detID;
            short board;
            short channel;
        };

        struct ECalPar {
            double c0 = 0;
            double c1 = 1;
            double c2 = 0;
        };

        struct W1Hist {
            short detID;
            TString name;
            TH2D* hitPattern = nullptr;
            TH1D* junctionPattern = nullptr;
            TH1D* ohmicPattern = nullptr;
            TH2D* rawChannelEnergy = nullptr;
            TH1D* channelMult = nullptr;
            TH1D* hitEnergy = nullptr;
            TH1D* hitECal = nullptr;
            TH1D* xEnergy = nullptr;
            TH1D* yEnergy = nullptr;
            TH1D* xECal = nullptr;
            TH1D* yECal = nullptr;
            std::vector<TH1D*> previousEntryTimeDiff;
            std::vector<TH1D*> channelEnergy;
        };

        struct ComHist {
            short detID;
            TString name;
            TH1D* channelMult = nullptr;
            TH1D* hitEnergy = nullptr;
            TH1D* hitECal = nullptr;
            TH1D* channelEnergy = nullptr;
            TH1D* channelECal = nullptr;
            std::vector<TH1D*> previousEntryTimeDiff;
        };

        bool ConfigureParameters();
        bool ConfigureTimeWindow();
        bool ConfigureInputFiles();
        bool ConfigureMappings();
        bool CheckBinning(const LKBinning& binning) const;
        void MakeHistograms();
        void MakeTimestampGraphs();
        void MakeTimestampDifferenceHistograms();
        void PrefixHistogramTitles();
        void FillTimestampGraphs(bool sorted);
        void FillTimestampDifferenceHistograms(bool sorted);
        void AddDrawingGroups();
        void ClearEvent();
        void ProcessEvent();
        void AddRawChannels();
        void BuildHits();
        void BuildW1Hits();
        void BuildComHits();
        bool FindW1(UShort_t board, UShort_t channel, const W1Map*& map, bool& side, int& strip) const;
        bool FindCom(UShort_t board, UShort_t channel, const ComMap*& map) const;
        int GetW1DisplayY(short detID, int strip) const;
        double CalibrateW1(short detID, UShort_t energy) const;
        double CalibrateCom(short detID, UShort_t energy) const;
        ECalPar GetW1ECalPar(short detID) const;
        ECalPar GetComECalPar(short detID) const;
        LKBinning GetW1HistBin(short detID, TString type) const;
        LKBinning GetComHistBin(short detID, TString type) const;
        void FillW1TimeDiff(short detID, ULong64_t time);
        void FillComTimeDiff(short detID, ULong64_t time);

        LKCompassReco* fReco = nullptr; //!
        TClonesArray* fW1ChannelArray = nullptr; //!
        TClonesArray* fW1HitArray = nullptr; //!
        TClonesArray* fComChannelArray = nullptr; //!
        TClonesArray* fComHitArray = nullptr; //!
        LKDrawingGroup* fTopDrawingGroup = nullptr; //!
        TGraph* fTimestampBefore40 = nullptr; //!
        TGraph* fTimestampBefore1000 = nullptr; //!
        TGraph* fTimestampAfter40 = nullptr; //!
        TGraph* fTimestampAfter1000 = nullptr; //!
        std::vector<TH1D*> fTimestampDifferenceBefore; //!
        std::vector<TH1D*> fTimestampDifferenceAfter; //!

        std::vector<TString> fInputFileArray; //!
        TString fInputTreeName = "Data_R"; //!
        Long64_t fTimeWindow = 4000000; //!
        Long64_t fFirstEntry = 0; //!
        Long64_t fLastEntry = 0; //!
        Int_t fEnergyThreshold = 0; //!
        Long64_t fNumEvents = -1; //!
        Long64_t fCountEvents = 0; //!
        bool fContinueEvent = false; //!
        bool fSortInput = true; //!
        bool fW1MainSide = false; //!
        bool fStoreChannelBranches = true; //!
        bool fStoreHitBranches = true; //!

        std::vector<W1Map> fW1MapArray; //!
        std::vector<ComMap> fComMapArray; //!
        std::map<short,ECalPar> fW1ECalParMap; //!
        std::map<short,ECalPar> fComECalParMap; //!
        std::map<short,bool> fW1YStartsFromTopMap; //!
        std::vector<W1Hist> fW1HistArray; //!
        std::vector<ComHist> fComHistArray; //!
        std::map<short,std::map<TString,LKBinning*>> fW1HistBinMap; //!
        std::map<short,std::map<TString,LKBinning*>> fComHistBinMap; //!
        std::map<short,ULong64_t> fPreviousW1EntryTimeMap; //!
        std::map<short,ULong64_t> fPreviousComEntryTimeMap; //!

    ClassDefOverride(LKCompassRecoTask, 1)
};

#endif
