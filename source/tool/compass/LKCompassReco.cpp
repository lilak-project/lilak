#include "LKCompassReco.h"

#include "TFile.h"
#include "TMath.h"
#include "TTree.h"

#include <algorithm>
#include <iostream>

using namespace std;

LKCompassReco::LKCompassReco()
{
}

LKCompassReco::~LKCompassReco()
{
    CloseInput();
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
    if (!fIsSorted)
        return fNextEntry;
    if (fNextSortedIndex >= Long64_t(fSortedEntryArray.size()))
        return fEndEntry;
    return fSortedEntryArray[fNextSortedIndex].entry;
}

bool LKCompassReco::GetTimestampAt(Long64_t index, ULong64_t& timestamp, bool sorted)
{
    if (fInputTree == nullptr || index < 0)
        return false;

    if (sorted) {
        if (!fIsSorted || index >= Long64_t(fSortedEntryArray.size()))
            return false;
        timestamp = fSortedEntryArray[index].timestamp;
        return true;
    }

    const auto firstEntry = TMath::Min(
        TMath::Max(Long64_t(0), fFirstEntry), fEndEntry);
    const auto entry = firstEntry + index;
    if (entry >= fEndEntry || fInputTree->GetEntry(entry) < 0)
        return false;
    timestamp = fTimestamp;
    return true;
}

const LKCompassReco::RawChannel* LKCompassReco::GetRawChannel(int index) const
{
    if (index < 0 || index >= int(fRawChannelArray.size()))
        return nullptr;
    return &fRawChannelArray[index];
}

void LKCompassReco::CloseInput()
{
    fSortedEntryArray.clear();
    fNextSortedIndex = 0;
    fIsSorted = false;
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
    fNextEntry = TMath::Max(Long64_t(0), fFirstEntry);
    fEndEntry = fLastEntry > 0 ? TMath::Min(fLastEntry + 1, numEntries) : numEntries;
    if (fNextEntry > fEndEntry)
        fNextEntry = fEndEntry;
    fNextSortedIndex = 0;
    fIsSorted = false;

    cout << "CoMPASS input: " << fInputFileName << endl;
    cout << "CoMPASS tree: " << fInputTreeName << endl;
    cout << "CoMPASS entries: " << numEntries << endl;
    cout << "CoMPASS entry range: " << fNextEntry << " - " << fEndEntry << endl;
    cout << "CoMPASS time window: " << fTimeWindow << " ps" << endl;
    return true;
}

bool LKCompassReco::HasNextEntry() const
{
    if (fIsSorted)
        return fNextSortedIndex < Long64_t(fSortedEntryArray.size());
    return fNextEntry < fEndEntry;
}

bool LKCompassReco::ReadNextEntry()
{
    if (!HasNextEntry())
        return false;
    if (fIsSorted) {
        const auto& sorted = fSortedEntryArray[fNextSortedIndex];
        fTimestamp = sorted.timestamp;
        fChannel = sorted.channel;
        fBoard = sorted.board;
        fEnergy = sorted.energy;
        fEnergyShort = sorted.energyShort;
        fFlags = sorted.flags;
        return true;
    }
    return fInputTree->GetEntry(fNextEntry) >= 0;
}

void LKCompassReco::AdvanceEntry()
{
    if (fIsSorted)
        ++fNextSortedIndex;
    else
        ++fNextEntry;
}

bool LKCompassReco::Sort()
{
    if (fInputTree == nullptr) {
        cout << "Initialize CoMPASS input before sorting." << endl;
        return false;
    }

    if (!fIsSorted) {
        const auto firstEntry = TMath::Min(
            TMath::Max(Long64_t(0), fFirstEntry), fEndEntry);
        const auto numEntries = fEndEntry - firstEntry;
        fSortedEntryArray.clear();
        fSortedEntryArray.reserve(numEntries);
        for (auto entry=firstEntry; entry<fEndEntry; ++entry) {
            if (fInputTree->GetEntry(entry) < 0) {
                cout << "Failed to read CoMPASS entry " << entry << " while sorting." << endl;
                fSortedEntryArray.clear();
                return false;
            }
            fSortedEntryArray.push_back({
                fTimestamp, entry, fFlags, fChannel, fBoard, fEnergy, fEnergyShort
            });
        }

        std::sort(fSortedEntryArray.begin(), fSortedEntryArray.end(),
            [](const SortedEntry& lhs, const SortedEntry& rhs) {
                if (lhs.timestamp != rhs.timestamp)
                    return lhs.timestamp < rhs.timestamp;
                return lhs.entry < rhs.entry;
            });
        fIsSorted = true;
        cout << "Sorted " << fSortedEntryArray.size() << " CoMPASS entries by Timestamp." << endl;
    }

    fNextSortedIndex = 0;
    fRawChannelArray.clear();
    return true;
}

bool LKCompassReco::WriteSortedFile(TString fileName)
{
    if (fInputTree == nullptr) {
        cout << "Initialize CoMPASS input before writing a sorted file." << endl;
        return false;
    }
    if (!Sort())
        return false;

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

    auto outputFile = TFile::Open(fileName, "recreate");
    if (outputFile == nullptr || outputFile->IsZombie()) {
        cout << "Cannot create sorted CoMPASS file: " << fileName << endl;
        if (outputFile != nullptr)
            delete outputFile;
        return false;
    }
    outputFile->SetCompressionSettings(fInputFile->GetCompressionSettings());

    // Clone every raw branch, including optional waveform data. FindEvent()
    // returns to the lightweight header-only configuration after this write.
    fInputTree->SetBranchStatus("*", true);
    outputFile->cd();
    auto sortedTree = fInputTree->CloneTree(0);
    if (sortedTree == nullptr) {
        cout << "Cannot clone CoMPASS tree " << fInputTreeName << endl;
        outputFile->Close();
        delete outputFile;
        ConfigureInputBranches();
        fNextSortedIndex = 0;
        return false;
    }
    sortedTree->SetName(fInputTreeName);
    sortedTree->SetTitle(fInputTree->GetTitle());
    sortedTree->SetAutoSave(0);

    bool success = true;
    const auto numEntries = Long64_t(fSortedEntryArray.size());
    for (Long64_t index=0; index<numEntries; ++index) {
        if (fInputTree->GetEntry(fSortedEntryArray[index].entry) < 0) {
            cout << "Failed to read CoMPASS entry " << fSortedEntryArray[index].entry
                 << " while writing sorted file." << endl;
            success = false;
            break;
        }
        if (sortedTree->Fill() < 0) {
            cout << "Failed to write sorted CoMPASS entry " << index << endl;
            success = false;
            break;
        }
        if ((index+1)%100000 == 0 || index+1 == numEntries)
            cout << "Writing sorted CoMPASS entries: " << index+1
                 << " / " << numEntries << endl;
    }

    if (success) {
        outputFile->cd();
        sortedTree->Write(fInputTreeName, TObject::kOverwrite);
        cout << "Sorted CoMPASS file: " << fileName << endl;
    }
    outputFile->Close();
    delete outputFile;

    ConfigureInputBranches();
    fNextSortedIndex = 0;
    fRawChannelArray.clear();
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
