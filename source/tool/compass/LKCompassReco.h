#ifndef LKCOMPASSRECO_H
#define LKCOMPASSRECO_H

#include "Rtypes.h"
#include "TString.h"

#include <functional>
#include <vector>

class TFile;
class TTree;

/**
 * Reader for CoMPASS ROOT data with optional Timestamp sorting.
 *
 * FindEvent() groups consecutive raw entries whose timestamps fall inside one
 * fixed time window, so the entries have to reach it in time order.
 *
 * CoMPASS usually writes entries in the order the boards report them, which
 * leaves the raw tree only locally out of order: an entry sits a few hundred
 * slots at most from where a full sort would put it. Sorting therefore does not
 * need the whole tree in memory. kSortByWindow streams the tree through a
 * buffer holding one block plus a window of that size: each buffer is sorted
 * and only the entries that no unread entry can still precede are released.
 * The emitted order is identical to sorting the whole tree, at a few MB instead
 * of 32 bytes per entry, and it costs no extra pass.
 *
 * A file whose clock jumps backwards by more than the window breaks that
 * assumption. GetNumOrderViolation() counts entries that left the sorter before
 * their predecessor, which is exactly what such a file produces, so a run whose
 * violation count is zero is sorted exactly. For files that do not hold,
 * kSortByMeasuredWindow first scans the timestamps to measure the window the
 * file actually needs and then streams with it, and kSortWholeRange falls back
 * to sorting the entire entry range in memory. Both are exact and slower.
 *
 * SetReadEnergyThreshold() and AddReadChannels() drop entries while reading,
 * before they reach the sorter. That shrinks every later stage, but it also
 * moves the start of each time window onto the entries that survive, so it
 * changes which hits end up grouped together.
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

        /// How Sort() puts the raw entries in Timestamp order.
        enum ESortPolicy {
            kSortByWindow,          ///< stream through the configured window
            kSortByMeasuredWindow,  ///< scan first, then stream through the window the file needs
            kSortWholeRange         ///< load and sort the whole entry range
        };

        /// Receives the timestamp of one raw entry.
        typedef std::function<void(ULong64_t)> EntryMonitor;

        LKCompassReco();
        virtual ~LKCompassReco();

        void SetInputFile(TString name) { fInputFileName = name; }
        void SetInputTreeName(TString name) { fInputTreeName = name; }
        void SetTimeWindow(Long64_t value) { fTimeWindow = value; }
        void SetEntryRange(Long64_t firstEntry=0, Long64_t lastEntry=0);
        void SetSortPolicy(ESortPolicy policy) { fSortPolicy = policy; }
        /// Largest displacement to expect between file order and time order.
        void SetSortWindow(Long64_t value) { fSortWindow = value; }
        /// Raw entries read into the sorting buffer at a time.
        void SetSortBlock(Long64_t value) { fSortBlock = value; }
        /// Drop entries below this raw Energy while reading. 0 keeps every entry.
        void SetReadEnergyThreshold(Int_t value) { fReadEnergyThreshold = value; }
        /// Keep only these channels while reading. A negative board matches any
        /// board. With no range added at all, every channel is kept.
        void AddReadChannels(int board, int channelLow, int channelHigh);
        void ClearReadChannels();
        /// Called for every entry in file order, as it is read from the tree,
        /// including entries the read filters go on to drop.
        void SetRawEntryMonitor(EntryMonitor monitor) { fRawEntryMonitor = monitor; }
        /// Called for every entry in time order, as FindEvent() consumes it.
        void SetSortedEntryMonitor(EntryMonitor monitor) { fSortedEntryMonitor = monitor; }

        TString GetInputFileName() const { return fInputFileName; }
        TString GetInputTreeName() const { return fInputTreeName; }
        Long64_t GetTimeWindow() const { return fTimeWindow; }
        ESortPolicy GetSortPolicy() const { return fSortPolicy; }
        Long64_t GetSortWindow() const { return fSortWindow; }
        Long64_t GetSortBlock() const { return fSortBlock; }
        Long64_t GetNumEntries() const;
        Long64_t GetNextEntry() const;
        /// Entries emitted before their predecessor; must be 0 for an exact sort.
        Long64_t GetNumOrderViolation() const { return fNumOrderViolation; }
        /// Largest sorting buffer reached, in entries.
        Long64_t GetMaxSortBufferSize() const { return fMaxSortBufferSize; }
        /// Largest displacement seen between file order and Timestamp order.
        /// After a violation this is only a lower bound, because a released
        /// entry no longer reveals how far it should have travelled.
        Long64_t GetMaxEntryReach() const { return fMaxEntryReach; }
        Long64_t GetNumReadEntry() const { return fNumReadEntry; }
        Long64_t GetNumSkippedEntry() const { return fNumSkippedEntry; }

        bool Init();
        /// Arm Timestamp sorting for the selected entry range.
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
        enum ESortMode { kSortNone, kSortWindow, kSortFull };

        static const int kMaxBoard = 64;
        static const int kMaxChannel = 4096;

        struct SortedEntry {
            ULong64_t timestamp;
            Long64_t entry;
            UInt_t flags;
            UShort_t channel;
            UShort_t board;
            UShort_t energy;
            UShort_t energyShort;
        };

        static bool IsEarlier(const SortedEntry& lhs, const SortedEntry& rhs);

        void CloseInput();
        bool CheckInputBranches() const;
        void ConfigureInputBranches();
        void ResetSortState();
        bool AcceptRawEntry() const;
        /// Scan the entry range and return the smallest window that sorts it
        /// exactly. The result is rounded up, never down.
        bool MeasureSortWindow(Long64_t& window);
        bool SortWholeRange();
        /// Read one block into the sorting buffer, merge it in and mark how much
        /// of the buffer can be released.
        bool ReadSortBlock();
        /// Make sure the sorting buffer can release its next entry.
        bool PrepareSortBuffer();
        bool HasNextEntry();
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
        Long64_t fSortWindow = 65536; //!
        Long64_t fSortBlock = 262144; //!
        ESortPolicy fSortPolicy = kSortByWindow; //!
        ESortMode fSortMode = kSortNone; //!

        TFile* fInputFile = nullptr; //!
        TTree* fInputTree = nullptr; //!

        UShort_t fChannel = 0; //!
        ULong64_t fTimestamp = 0; //!
        UShort_t fBoard = 0; //!
        UShort_t fEnergy = 0; //!
        UShort_t fEnergyShort = 0; //!
        UInt_t fFlags = 0; //!

        std::vector<char> fChannelAccept; //!
        Int_t fReadEnergyThreshold = 0; //!

        std::vector<RawChannel> fRawChannelArray; //!
        std::vector<SortedEntry> fSortedEntryArray; //!
        std::vector<SortedEntry> fSortBuffer; //!
        size_t fSortBufferIndex = 0; //!
        Long64_t fSortBufferSafeEntry = 0; //!
        Long64_t fMaxSortBufferSize = 0; //!
        Long64_t fNumOrderViolation = 0; //!
        Long64_t fMaxEmittedEntry = -1; //!
        Long64_t fMaxEntryReach = 0; //!
        Long64_t fNumReadEntry = 0; //!
        Long64_t fNumSkippedEntry = 0; //!
        ULong64_t fLastEmittedTimestamp = 0; //!
        bool fHasEmittedEntry = false; //!
        bool fUnsortedEntryReady = false; //!

        EntryMonitor fRawEntryMonitor; //!
        EntryMonitor fSortedEntryMonitor; //!

        ULong64_t fEventStartTime = 0; //!
        ULong64_t fEventEndTime = 0; //!
};

#endif
