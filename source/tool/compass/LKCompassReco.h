#ifndef LKCOMPASSRECO_H
#define LKCOMPASSRECO_H

#include "Rtypes.h"
#include "TString.h"

#include <vector>

class TFile;
class TTree;

/**
 * Reader for CoMPASS ROOT data with optional in-memory Timestamp sorting.
 *
 * FindEvent() groups consecutive raw entries whose timestamps fall inside one
 * fixed time window. Sort() makes FindEvent() consume sorted raw headers, and
 * WriteSortedFile() can persist the complete raw tree in the same order.
 */
class LKCompassReco
{
    public:
        struct RawChannel {
            UShort_t channel = 0;
            ULong64_t timestamp = 0;
            UShort_t board = 0;
            UShort_t energy = 0;
            UShort_t energyShort = 0;
            UInt_t flags = 0;
        };

        LKCompassReco();
        virtual ~LKCompassReco();

        void SetInputFile(TString name) { fInputFileName = name; }
        void SetInputTreeName(TString name) { fInputTreeName = name; }
        void SetTimeWindow(Long64_t value) { fTimeWindow = value; }
        void SetEntryRange(Long64_t firstEntry=0, Long64_t lastEntry=0);

        TString GetInputFileName() const { return fInputFileName; }
        TString GetInputTreeName() const { return fInputTreeName; }
        Long64_t GetTimeWindow() const { return fTimeWindow; }
        Long64_t GetNumEntries() const;
        Long64_t GetNextEntry() const;
        bool IsSorted() const { return fIsSorted; }
        bool GetTimestampAt(Long64_t index, ULong64_t& timestamp, bool sorted=false);

        bool Init();
        /// Sort the selected entry range in memory and reset FindEvent().
        bool Sort();
        /// Write every input branch in Timestamp order. Empty name uses
        /// <input-base>.sorted.root.
        bool WriteSortedFile(TString fileName="");
        bool FindEvent();

        int GetNumRawChannels() const { return int(fRawChannelArray.size()); }
        const RawChannel* GetRawChannel(int index) const;
        const std::vector<RawChannel>& GetRawChannelArray() const { return fRawChannelArray; }
        ULong64_t GetEventStartTime() const { return fEventStartTime; }
        ULong64_t GetEventEndTime() const { return fEventEndTime; }

    private:
        struct SortedEntry {
            ULong64_t timestamp;
            Long64_t entry;
            UInt_t flags;
            UShort_t channel;
            UShort_t board;
            UShort_t energy;
            UShort_t energyShort;
        };

        void CloseInput();
        bool CheckInputBranches() const;
        void ConfigureInputBranches();
        bool HasNextEntry() const;
        bool ReadNextEntry();
        void AdvanceEntry();
        void CopyCurrentEntry();

        TString fInputFileName = ""; //!
        TString fInputTreeName = "Data_R"; //!
        Long64_t fTimeWindow = 4000000; //!
        Long64_t fFirstEntry = 0; //!
        Long64_t fLastEntry = 0; //!
        Long64_t fNextEntry = 0; //!
        Long64_t fEndEntry = 0; //!
        Long64_t fNextSortedIndex = 0; //!
        bool fIsSorted = false; //!

        TFile* fInputFile = nullptr; //!
        TTree* fInputTree = nullptr; //!

        UShort_t fChannel = 0; //!
        ULong64_t fTimestamp = 0; //!
        UShort_t fBoard = 0; //!
        UShort_t fEnergy = 0; //!
        UShort_t fEnergyShort = 0; //!
        UInt_t fFlags = 0; //!

        std::vector<RawChannel> fRawChannelArray; //!
        std::vector<SortedEntry> fSortedEntryArray; //!
        ULong64_t fEventStartTime = 0; //!
        ULong64_t fEventEndTime = 0; //!
};

#endif
