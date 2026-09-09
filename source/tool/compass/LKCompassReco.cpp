#include "LKCompassReco.h"

#include "TFile.h"
#include "TMath.h"
#include "TTree.h"

#include <algorithm>
#include <iostream>
#include <utility>

using namespace std;

LKCompassReco::LKCompassReco()
{
}

LKCompassReco::~LKCompassReco()
{
    CloseInput();
}

bool LKCompassReco::IsEarlier(const SortedEntry& lhs, const SortedEntry& rhs)
{
    if (lhs.timestamp != rhs.timestamp)
        return lhs.timestamp < rhs.timestamp;
    return lhs.entry < rhs.entry;
}

void LKCompassReco::ClearReadChannels()
{
    fChannelAccept.clear();
}

void LKCompassReco::AddReadChannels(int board, int channelLow, int channelHigh)
{
    if (channelLow > channelHigh)
        std::swap(channelLow, channelHigh);
    channelLow = TMath::Max(0, channelLow);
    channelHigh = TMath::Min(kMaxChannel-1, channelHigh);
    if (channelLow > channelHigh)
        return;

    if (fChannelAccept.empty())
        fChannelAccept.assign(size_t(kMaxBoard)*kMaxChannel, 0);

    const int boardLow = board < 0 ? 0 : board;
    const int boardHigh = board < 0 ? kMaxBoard-1 : board;
    if (boardLow >= kMaxBoard)
        return;
    for (auto b=boardLow; b<=TMath::Min(boardHigh,kMaxBoard-1); ++b)
        for (auto c=channelLow; c<=channelHigh; ++c)
            fChannelAccept[size_t(b)*kMaxChannel + c] = 1;
}

bool LKCompassReco::AcceptRawEntry() const
{
    if (fReadEnergyThreshold > 0 && Int_t(fEnergy) < fReadEnergyThreshold)
        return false;
    if (fChannelAccept.empty())
        return true;
    if (fBoard >= kMaxBoard || fChannel >= kMaxChannel)
        return false;
    return fChannelAccept[size_t(fBoard)*kMaxChannel + fChannel] != 0;
}

void LKCompassReco::SetEntryRange(Long64_t firstEntry, Long64_t lastEntry)
{
    fFirstEntry = firstEntry;
    fLastEntry = lastEntry;
}

Long64_t LKCompassReco::GetNumEntries() const
{
    return fInputTree == nullptr ? 0 : fInputTree->GetEntries();
}

Long64_t LKCompassReco::GetNextEntry() const
{
    if (fSortMode == kSortFull) {
        if (fNextSortedIndex >= Long64_t(fSortedEntryArray.size()))
            return fEndEntry;
        return fSortedEntryArray[fNextSortedIndex].entry;
    }
    if (fSortMode == kSortWindow && fSortBufferIndex < fSortBuffer.size())
        return fSortBuffer[fSortBufferIndex].entry;
    return fNextEntry;
}

const LKCompassReco::RawChannel* LKCompassReco::GetRawChannel(int index) const
{
    if (index < 0 || index >= int(fRawChannelArray.size()))
        return nullptr;
    return &fRawChannelArray[index];
}

void LKCompassReco::CloseInput()
{
    ResetSortState();
    fInputTree = nullptr;
    if (fInputFile != nullptr) {
        fInputFile->Close();
        delete fInputFile;
        fInputFile = nullptr;
    }
}

void LKCompassReco::ConfigureInputBranches()
{
    fInputTree->ResetBranchAddresses();
    fInputTree->SetBranchStatus("*", false);
    fInputTree->SetBranchStatus("Channel", true);
    fInputTree->SetBranchStatus("Timestamp", true);
    fInputTree->SetBranchStatus("Board", true);
    fInputTree->SetBranchStatus("Energy", true);
    fInputTree->SetBranchAddress("Channel", &fChannel);
    fInputTree->SetBranchAddress("Timestamp", &fTimestamp);
    fInputTree->SetBranchAddress("Board", &fBoard);
    fInputTree->SetBranchAddress("Energy", &fEnergy);

    fEnergyShort = 0;
    fFlags = 0;
    if (fInputTree->GetBranch("EnergyShort") != nullptr) {
        fInputTree->SetBranchStatus("EnergyShort", true);
        fInputTree->SetBranchAddress("EnergyShort", &fEnergyShort);
    }
    if (fInputTree->GetBranch("Flags") != nullptr) {
        fInputTree->SetBranchStatus("Flags", true);
        fInputTree->SetBranchAddress("Flags", &fFlags);
    }
}

bool LKCompassReco::CheckInputBranches() const
{
    const char* branchNames[] = {"Channel", "Timestamp", "Board", "Energy"};

    for (auto branchName : branchNames) {
        if (fInputTree->GetBranch(branchName) == nullptr) {
            cout << "Cannot find CoMPASS branch " << branchName
                 << " in tree " << fInputTreeName << endl;
            return false;
        }
    }
    return true;
}

bool LKCompassReco::Init()
{
    CloseInput();
    fRawChannelArray.clear();

    if (fInputFileName.IsNull()) {
        cout << "CoMPASS input file is not set." << endl;
        return false;
    }
    if (fInputTreeName.IsNull()) {
        cout << "CoMPASS input tree name is not set." << endl;
        return false;
    }
    if (fTimeWindow <= 0) {
        cout << "CoMPASS time window must be positive: " << fTimeWindow << endl;
        return false;
    }

    fInputFile = TFile::Open(fInputFileName, "read");
    if (fInputFile == nullptr || fInputFile->IsZombie()) {
        cout << "Cannot open CoMPASS input file: " << fInputFileName << endl;
        CloseInput();
        return false;
    }

    fInputTree = dynamic_cast<TTree*>(fInputFile->Get(fInputTreeName));
    if (fInputTree == nullptr) {
        cout << "Cannot find CoMPASS input tree " << fInputTreeName
             << " in " << fInputFileName << endl;
        CloseInput();
        return false;
    }
    if (!CheckInputBranches()) {
        CloseInput();
        return false;
    }

    // CoMPASS files may contain large waveform branches such as Samples.
    // Reconstruction only needs the raw header values below, so keep every
    // unrelated branch disabled to avoid reading and decompressing it.
    ConfigureInputBranches();

    const auto numEntries = fInputTree->GetEntries();
    fEndEntry = fLastEntry > 0 ? TMath::Min(fLastEntry + 1, numEntries) : numEntries;
    ResetSortState();

    cout << "CoMPASS input: " << fInputFileName << endl;
    cout << "CoMPASS tree: " << fInputTreeName << endl;
    cout << "CoMPASS entries: " << numEntries << endl;
    cout << "CoMPASS entry range: " << fNextEntry << " - " << fEndEntry << endl;
    cout << "CoMPASS time window: " << fTimeWindow << " ps" << endl;
    return true;
}

void LKCompassReco::ResetSortState()
{
    fSortMode = kSortNone;
    fSortedEntryArray.clear();
    fSortedEntryArray.shrink_to_fit();
    fNextSortedIndex = 0;
    fSortBuffer.clear();
    fSortBufferIndex = 0;
    fSortBufferSafeEntry = 0;
    fMaxSortBufferSize = 0;
    fNumOrderViolation = 0;
    fMaxEmittedEntry = -1;
    fMaxEntryReach = 0;
    fNumReadEntry = 0;
    fNumSkippedEntry = 0;
    fLastEmittedTimestamp = 0;
    fHasEmittedEntry = false;
    fUnsortedEntryReady = false;
    fRawChannelArray.clear();
    fNextEntry = TMath::Min(TMath::Max(Long64_t(0), fFirstEntry), fEndEntry);
}

bool LKCompassReco::Sort()
{
    if (fInputTree == nullptr) {
        cout << "Initialize CoMPASS input before sorting." << endl;
        return false;
    }

    ResetSortState();

    if (fSortPolicy == kSortWholeRange)
        return SortWholeRange();

    if (fSortPolicy == kSortByMeasuredWindow) {
        Long64_t measured = 0;
        if (!MeasureSortWindow(measured))
            return false;
        cout << "CoMPASS raw entries need a sort window of " << measured
             << " entries." << endl;
        fSortWindow = TMath::Max(fSortWindow, measured);
        ResetSortState();
    }

    if (fSortWindow <= 0)
        return SortWholeRange();

    // Each block is merged into the carry left by the previous one, so a block
    // smaller than the window would merge far more entries than it releases.
    fSortBlock = TMath::Max(fSortBlock, fSortWindow);
    fSortMode = kSortWindow;
    fSortBuffer.reserve(fSortBlock + fSortWindow);

    cout << "Sorting CoMPASS entries by Timestamp while reading: window "
         << fSortWindow << " entries, block " << fSortBlock << " entries, "
         << 1e-6*double(fSortBlock+fSortWindow)*sizeof(SortedEntry)
         << " MB buffer." << endl;
    return true;
}

bool LKCompassReco::MeasureSortWindow(Long64_t& window)
{
    // The window has to cover the widest inversion in the file: for every entry,
    // how far back the first entry sits that already carries a later timestamp.
    // Running maxima answer that with a binary search, and there are few enough
    // of them to keep only a decimated history: merging neighbouring maxima
    // keeps the older index, which can only push the answer up, never down.
    const auto firstEntry = TMath::Min(
        TMath::Max(Long64_t(0), fFirstEntry), fEndEntry);
    const size_t maxRecords = 2000000;
    std::vector<ULong64_t> maxValue;
    std::vector<Long64_t> maxIndex;
    Long64_t bucket = 64;
    Long64_t worst = 0;

    cout << "Scanning CoMPASS timestamps to measure the sort window..." << endl;
    for (auto entry=firstEntry; entry<fEndEntry; ++entry) {
        if (fInputTree->GetEntry(entry) < 0) {
            cout << "Failed to read CoMPASS entry " << entry
                 << " while measuring the sort window." << endl;
            return false;
        }
        if (!AcceptRawEntry())
            continue;

        if (!maxValue.empty() && fTimestamp < maxValue.back()) {
            const auto found = std::upper_bound(maxValue.begin(), maxValue.end(), fTimestamp);
            const auto reach = entry - maxIndex[found-maxValue.begin()];
            if (reach > worst)
                worst = reach;
        }

        if (maxValue.empty() || fTimestamp > maxValue.back()) {
            if (!maxValue.empty() && entry - maxIndex.back() < bucket)
                maxValue.back() = fTimestamp;
            else {
                maxValue.push_back(fTimestamp);
                maxIndex.push_back(entry);
            }
            if (maxValue.size() > maxRecords) {
                size_t write = 0;
                for (size_t read=0; read<maxValue.size(); read+=2, ++write) {
                    maxIndex[write] = maxIndex[read];
                    maxValue[write] = read+1 < maxValue.size() ? maxValue[read+1] : maxValue[read];
                }
                maxValue.resize(write);
                maxIndex.resize(write);
                bucket *= 2;
            }
        }
    }

    window = worst + bucket;
    return true;
}

bool LKCompassReco::SortWholeRange()
{
    const auto firstEntry = fNextEntry;
    const auto numEntries = fEndEntry - firstEntry;
    cout << "Sorting all " << numEntries << " CoMPASS entries in memory ("
         << 1e-9*double(numEntries)*sizeof(SortedEntry) << " GB). Set a nonzero "
         << "sort window to stream the sort instead." << endl;

    fSortedEntryArray.reserve(numEntries);
    for (auto entry=firstEntry; entry<fEndEntry; ++entry) {
        if (fInputTree->GetEntry(entry) < 0) {
            cout << "Failed to read CoMPASS entry " << entry << " while sorting." << endl;
            ResetSortState();
            return false;
        }
        ++fNumReadEntry;
        if (fRawEntryMonitor)
            fRawEntryMonitor(fTimestamp);
        if (!AcceptRawEntry()) {
            ++fNumSkippedEntry;
            continue;
        }
        fSortedEntryArray.push_back({
            fTimestamp, entry, fFlags, fChannel, fBoard, fEnergy, fEnergyShort
        });
    }

    std::sort(fSortedEntryArray.begin(), fSortedEntryArray.end(), IsEarlier);
    fSortMode = kSortFull;
    fNextSortedIndex = 0;
    fMaxSortBufferSize = Long64_t(fSortedEntryArray.size());
    cout << "Sorted " << fSortedEntryArray.size() << " CoMPASS entries by Timestamp." << endl;
    return true;
}

bool LKCompassReco::ReadSortBlock()
{
    if (fSortBufferIndex > 0) {
        fSortBuffer.erase(fSortBuffer.begin(), fSortBuffer.begin()+fSortBufferIndex);
        fSortBufferIndex = 0;
    }

    const auto carry = fSortBuffer.size();
    const auto readUntil = TMath::Min(fNextEntry+fSortBlock, fEndEntry);
    for (; fNextEntry<readUntil; ++fNextEntry) {
        if (fInputTree->GetEntry(fNextEntry) < 0) {
            cout << "Failed to read CoMPASS entry " << fNextEntry << " while sorting." << endl;
            return false;
        }
        ++fNumReadEntry;
        if (fRawEntryMonitor)
            fRawEntryMonitor(fTimestamp);
        if (!AcceptRawEntry()) {
            ++fNumSkippedEntry;
            continue;
        }
        fSortBuffer.push_back({
            fTimestamp, fNextEntry, fFlags, fChannel, fBoard, fEnergy, fEnergyShort
        });
    }
    // The carry is already sorted, so the new block only has to be sorted on its
    // own and merged in.
    std::sort(fSortBuffer.begin()+carry, fSortBuffer.end(), IsEarlier);
    if (carry > 0)
        std::inplace_merge(fSortBuffer.begin(), fSortBuffer.begin()+carry,
                           fSortBuffer.end(), IsEarlier);

    // An unread entry sits at file index fNextEntry or later, and can only come
    // before a buffered entry that is within fSortWindow slots of it. Buffered
    // entries older than that boundary are therefore final and can be released;
    // the rest stay as the carry for the next block.
    fSortBufferSafeEntry = (fNextEntry >= fEndEntry) ? fEndEntry : fNextEntry - fSortWindow;
    if (Long64_t(fSortBuffer.size()) > fMaxSortBufferSize)
        fMaxSortBufferSize = Long64_t(fSortBuffer.size());
    return true;
}

bool LKCompassReco::PrepareSortBuffer()
{
    while (true) {
        if (fSortBufferIndex < fSortBuffer.size()
            && fSortBuffer[fSortBufferIndex].entry < fSortBufferSafeEntry)
            return true;
        if (fNextEntry >= fEndEntry) {
            // Nothing is left to read, so whatever is buffered is already final.
            fSortBufferSafeEntry = fEndEntry;
            return fSortBufferIndex < fSortBuffer.size();
        }
        if (!ReadSortBlock())
            return false;
    }
}

bool LKCompassReco::HasNextEntry()
{
    if (fSortMode == kSortWindow)
        return PrepareSortBuffer();
    if (fSortMode == kSortFull)
        return fNextSortedIndex < Long64_t(fSortedEntryArray.size());

    // Unsorted: load entries until one passes the read filters. The flag keeps
    // repeated peeks from reading and reporting the same entry twice.
    while (!fUnsortedEntryReady && fNextEntry < fEndEntry) {
        if (fInputTree->GetEntry(fNextEntry) < 0)
            return false;
        ++fNumReadEntry;
        if (fRawEntryMonitor)
            fRawEntryMonitor(fTimestamp);
        if (AcceptRawEntry()) {
            fUnsortedEntryReady = true;
            break;
        }
        ++fNumSkippedEntry;
        ++fNextEntry;
    }
    return fUnsortedEntryReady;
}

bool LKCompassReco::ReadNextEntry()
{
    if (!HasNextEntry())
        return false;
    if (fSortMode != kSortNone) {
        const auto& sorted = (fSortMode == kSortWindow)
            ? fSortBuffer[fSortBufferIndex]
            : fSortedEntryArray[fNextSortedIndex];
        fTimestamp = sorted.timestamp;
        fChannel = sorted.channel;
        fBoard = sorted.board;
        fEnergy = sorted.energy;
        fEnergyShort = sorted.energyShort;
        fFlags = sorted.flags;
        return true;
    }
    return fUnsortedEntryReady;
}

void LKCompassReco::AdvanceEntry()
{
    // ReadNextEntry() left the entry being consumed in the fTimestamp field.
    if (fSortMode == kSortNone) {
        ++fNextEntry;
        fUnsortedEntryReady = false;
        return;
    }

    if (fHasEmittedEntry && fTimestamp < fLastEmittedTimestamp)
        ++fNumOrderViolation;
    fLastEmittedTimestamp = fTimestamp;
    fHasEmittedEntry = true;

    // An entry released after one that sits further down the file was stored
    // that many slots too early, which is the displacement the window covers.
    const auto entry = GetNextEntry();
    if (entry > fMaxEmittedEntry)
        fMaxEmittedEntry = entry;
    else if (fMaxEmittedEntry - entry > fMaxEntryReach)
        fMaxEntryReach = fMaxEmittedEntry - entry;
    if (fSortedEntryMonitor)
        fSortedEntryMonitor(fTimestamp);

    if (fSortMode == kSortWindow)
        ++fSortBufferIndex;
    else
        ++fNextSortedIndex;
}

bool LKCompassReco::WriteSortedFile(TString fileName)
{
    if (fInputTree == nullptr) {
        cout << "Initialize CoMPASS input before writing a sorted file." << endl;
        return false;
    }

    if (fileName.IsNull()) {
        fileName = fInputFileName;
        if (fileName.EndsWith(".root"))
            fileName.Remove(fileName.Length()-5);
        fileName += ".sorted.root";
    }
    if (fileName == fInputFileName) {
        cout << "Sorted output file must differ from input file: " << fileName << endl;
        return false;
    }

    if (!Sort())
        return false;

    // A second handle on the input carries the full payload, including optional
    // waveform branches, so the sorting pass above can keep reading headers
    // only. Its entries are visited in the order the sorter releases them.
    auto sourceFile = TFile::Open(fInputFileName, "read");
    auto sourceTree = sourceFile == nullptr || sourceFile->IsZombie()
        ? nullptr : dynamic_cast<TTree*>(sourceFile->Get(fInputTreeName));
    if (sourceTree == nullptr) {
        cout << "Cannot reopen CoMPASS input file: " << fInputFileName << endl;
        if (sourceFile != nullptr) { sourceFile->Close(); delete sourceFile; }
        ResetSortState();
        return false;
    }

    auto outputFile = TFile::Open(fileName, "recreate");
    if (outputFile == nullptr || outputFile->IsZombie()) {
        cout << "Cannot create sorted CoMPASS file: " << fileName << endl;
        if (outputFile != nullptr)
            delete outputFile;
        sourceFile->Close();
        delete sourceFile;
        ResetSortState();
        return false;
    }
    outputFile->SetCompressionSettings(fInputFile->GetCompressionSettings());
    outputFile->cd();

    auto sortedTree = sourceTree->CloneTree(0);
    if (sortedTree == nullptr) {
        cout << "Cannot clone CoMPASS tree " << fInputTreeName << endl;
        outputFile->Close();
        delete outputFile;
        sourceFile->Close();
        delete sourceFile;
        ResetSortState();
        return false;
    }
    sortedTree->SetName(fInputTreeName);
    sortedTree->SetTitle(sourceTree->GetTitle());
    sortedTree->SetAutoSave(0);

    bool success = true;
    Long64_t count = 0;
    const auto numEntries = fEndEntry - fNextEntry;
    while (HasNextEntry()) {
        if (!ReadNextEntry()) {
            success = false;
            break;
        }
        const auto entry = GetNextEntry();
        if (sourceTree->GetEntry(entry) < 0) {
            cout << "Failed to read CoMPASS entry " << entry
                 << " while writing sorted file." << endl;
            success = false;
            break;
        }
        if (sortedTree->Fill() < 0) {
            cout << "Failed to write sorted CoMPASS entry " << count << endl;
            success = false;
            break;
        }
        AdvanceEntry();
        if ((++count)%1000000 == 0 || count == numEntries)
            cout << "Writing sorted CoMPASS entries: " << count
                 << " / " << numEntries << endl;
    }

    if (success && fNumOrderViolation > 0) {
        cout << fNumOrderViolation << " entries were written out of Timestamp order. "
             << "The sort window " << fSortWindow << " is too small." << endl;
        success = false;
    }
    if (success) {
        outputFile->cd();
        sortedTree->Write(fInputTreeName, TObject::kOverwrite);
        cout << "Sorted CoMPASS file: " << fileName << endl;
    }
    outputFile->Close();
    delete outputFile;
    sourceFile->Close();
    delete sourceFile;

    ResetSortState();
    return success;
}

void LKCompassReco::CopyCurrentEntry()
{
    RawChannel channel;
    channel.channel = fChannel;
    channel.timestamp = fTimestamp;
    channel.board = fBoard;
    channel.energy = fEnergy;
    channel.energyShort = fEnergyShort;
    channel.flags = fFlags;
    fRawChannelArray.push_back(channel);
}

bool LKCompassReco::FindEvent()
{
    fRawChannelArray.clear();
    fEventStartTime = 0;
    fEventEndTime = 0;

    if (fInputTree == nullptr || !HasNextEntry())
        return false;

    if (!ReadNextEntry())
        return false;
    fEventStartTime = fTimestamp;

    while (HasNextEntry())
    {
        const bool insideWindow = fTimestamp < fEventStartTime
            || fTimestamp - fEventStartTime < ULong64_t(fTimeWindow);
        if (!insideWindow)
            break;

        CopyCurrentEntry();
        fEventEndTime = fTimestamp;
        AdvanceEntry();
        if (HasNextEntry() && !ReadNextEntry())
            return false;
    }

    return !fRawChannelArray.empty();
}
