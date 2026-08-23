#include "LKCompassReco.h"

#include "LKBinning.h"
#include "LKDrawingGroup.h"

#include "TDirectory.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TKey.h"
#include "TMath.h"
#include "TSystem.h"

#include <algorithm>
#include <cctype>
#include <iostream>

using namespace std;

LKCompassReco::LKCompassReco()
{
    fW1ChannelArray = new TClonesArray("LKComW1Channel",128);
    fW1HitArray = new TClonesArray("LKComW1Hit",32);
    fComChannelArray = new TClonesArray("LKComChannel",64);
    fComHitArray = new TClonesArray("LKComHit",16);
}

LKCompassReco::~LKCompassReco()
{
    for (auto& det : fW1HistBinMap)
        for (auto& item : det.second)
            delete item.second;
    for (auto& det : fComHistBinMap)
        for (auto& item : det.second)
            delete item.second;

    delete fW1ChannelArray;
    delete fW1HitArray;
    delete fComChannelArray;
    delete fComHitArray;
}

void LKCompassReco::SetEntryRange(Long64_t firstEntry, Long64_t lastEntry)
{
    fFirstEntry = firstEntry;
    fLastEntry = lastEntry;
}

void LKCompassReco::AddW1(short detID, short board, short yChannelStart, short xChannelStart)
{
    for (auto& w1 : fW1MapArray) {
        if (w1.detID==detID) {
            cout << "W1 detector ID is already used: " << detID << endl;
            return;
        }
    }

    auto name = TString::Format("w1_%d",detID);
    fW1MapArray.push_back({name,detID,board,yChannelStart,xChannelStart});
}

void LKCompassReco::AddCom(short detID, short board, short channel)
{
    for (auto& com : fComMapArray) {
        if (com.detID==detID) {
            cout << "Com detector ID is already used: " << detID << endl;
            return;
        }
    }

    auto name = TString::Format("com_%d",detID);
    fComMapArray.push_back({name,detID,board,channel});
}

void LKCompassReco::SetECalParametersW1(short detID, double c0, double c1, double c2)
{
    fW1ECalParMap[detID] = {c0,c1,c2};
}

void LKCompassReco::SetECalParametersCom(short detID, double c0, double c1, double c2)
{
    fComECalParMap[detID] = {c0,c1,c2};
}

bool LKCompassReco::CheckBinning(const LKBinning& binning) const
{
    if (binning.nx()<=0 || binning.x2()<=binning.x1()) {
        cout << "Invalid histogram binning: " << binning.nx() << ", " << binning.x1() << ", " << binning.x2() << endl;
        return false;
    }

    return true;
}

void LKCompassReco::SetHistogramBinningW1(short detID, TString type, const LKBinning& binning)
{
    if (!CheckBinning(binning))
        return;

    type.ToLower();
    auto& map = fW1HistBinMap[detID];
    auto set = [&map,&binning](TString name) {
        if (map[name]!=nullptr)
            delete map[name];
        map[name] = new LKBinning(binning);
    };

    if (type=="mult" || type=="channel_mult")
        set("mult");
    else if (type=="energy" || type=="hit_energy")
        set("hit_energy");
    else if (type=="ecal" || type=="hit_ecal")
        set("hit_ecal");
    else if (type=="x_energy")
        set("x_energy");
    else if (type=="y_energy")
        set("y_energy");
    else if (type=="x_ecal")
        set("x_ecal");
    else if (type=="y_ecal")
        set("y_ecal");
    else if (type=="time_diff" || type=="timediff" || type=="previous_entry_time_diff")
        set("time_diff");
    else
        cout << "Unknown W1 histogram type: " << type << endl;
}

void LKCompassReco::SetHistogramBinningW1(short detID, TString type, int nBins, double xMin, double xMax)
{
    SetHistogramBinningW1(detID,type,LKBinning(nBins,xMin,xMax));
}

void LKCompassReco::SetHistogramBinningCom(short detID, TString type, const LKBinning& binning)
{
    if (!CheckBinning(binning))
        return;

    type.ToLower();
    auto& map = fComHistBinMap[detID];
    auto set = [&map,&binning](TString name) {
        if (map[name]!=nullptr)
            delete map[name];
        map[name] = new LKBinning(binning);
    };

    if (type=="mult" || type=="channel_mult")
        set("mult");
    else if (type=="energy" || type=="hit_energy")
        set("hit_energy");
    else if (type=="ecal" || type=="hit_ecal")
        set("hit_ecal");
    else if (type=="channel_energy")
        set("channel_energy");
    else if (type=="channel_ecal")
        set("channel_ecal");
    else if (type=="time_diff" || type=="timediff" || type=="previous_entry_time_diff")
        set("time_diff");
    else
        cout << "Unknown Com histogram type: " << type << endl;
}

void LKCompassReco::SetHistogramBinningCom(short detID, TString type, int nBins, double xMin, double xMax)
{
    SetHistogramBinningCom(detID,type,LKBinning(nBins,xMin,xMax));
}

bool LKCompassReco::Init()
{
    fInputFile = new TFile(fInputFileName);
    if (fInputFile==nullptr || fInputFile->IsZombie()) {
        cout << "Cannot open input file: " << fInputFileName << endl;
        return false;
    }

    fInputTree = (TTree*) fInputFile -> Get("Data_R");
    if (fInputTree!=nullptr) {
        fInputIsReco = false;
        SetInputTree();
    }
    else {
        fInputTree = (TTree*) fInputFile -> Get("event");
        if (fInputTree==nullptr || TString(fInputTree->GetTitle())!="LKCompassReco") {
            cout << "Cannot find Data_R or LKCompassReco event tree in " << fInputFileName << endl;
            return false;
        }
        fInputIsReco = true;
        SetRecoInputTree();
        if (!CollectRecoDetectorMaps())
            return false;
    }

    if (fOutputFileName.IsNull())
        fOutputFileName = MakeDefaultOutputFileName();

    fOutputFile = new TFile(fOutputFileName,"recreate");
    fOutputTree = new TTree("event","LKCompassReco");
    if (fStoreChannelBranches) {
        fOutputTree -> Branch("ComW1Channel",&fW1ChannelArray);
        fOutputTree -> Branch("ComChannel",&fComChannelArray);
    }
    if (fStoreHitBranches) {
        fOutputTree -> Branch("ComW1Hit",&fW1HitArray);
        fOutputTree -> Branch("ComHit",&fComHitArray);
    }
    MakeHistograms();

    cout << "Input: " << fInputFileName << endl;
    cout << "Output: " << fOutputFileName << endl;
    cout << "Input mode: " << (fInputIsReco ? "LKCompassReco" : "Data_R") << endl;
    cout << "Time window: " << fTimeWindow << endl;
    cout << "Channel branches: " << fStoreChannelBranches << endl;
    cout << "Hit branches: " << fStoreHitBranches << endl;
    cout << "W1 detectors: " << fW1MapArray.size() << endl;
    cout << "Com detectors: " << fComMapArray.size() << endl;

    return true;
}

void LKCompassReco::SetInputTree()
{
    fInputTree -> SetBranchAddress("Channel",&fChannel);
    fInputTree -> SetBranchAddress("Timestamp",&fTimestamp);
    fInputTree -> SetBranchAddress("Board",&fBoard);
    fInputTree -> SetBranchAddress("Energy",&fEnergy);
    fInputTree -> SetBranchAddress("EnergyShort",&fEnergyShort);
    fInputTree -> SetBranchAddress("Flags",&fFlags);
}

void LKCompassReco::SetRecoInputTree()
{
    auto w1BranchName = fInputTree->GetBranch("ComW1Channel")!=nullptr ? "ComW1Channel" : "W1Channel";
    auto comBranchName = "ComChannel";
    if (fInputTree->GetBranch(w1BranchName)==nullptr || fInputTree->GetBranch(comBranchName)==nullptr) {
        cout << "Reco input needs ComW1Channel and ComChannel branches for ECal update." << endl;
        return;
    }

    fInputTree -> SetBranchAddress(w1BranchName,&fW1ChannelArray);
    fInputTree -> SetBranchAddress(comBranchName,&fComChannelArray);
}

bool LKCompassReco::CollectRecoDetectorMaps()
{
    auto w1BranchName = fInputTree->GetBranch("ComW1Channel")!=nullptr ? "ComW1Channel" : "W1Channel";
    auto comBranchName = "ComChannel";
    if (fInputTree->GetBranch(w1BranchName)==nullptr || fInputTree->GetBranch(comBranchName)==nullptr)
        return false;

    auto hasW1 = [this](short detID) {
        for (auto& w1 : fW1MapArray)
            if (w1.detID==detID)
                return true;
        return false;
    };

    auto hasCom = [this](short detID) {
        for (auto& com : fComMapArray)
            if (com.detID==detID)
                return true;
        return false;
    };

    auto numEntries = fInputTree -> GetEntries();
    for (auto entry=0; entry<numEntries; ++entry)
    {
        fW1ChannelArray -> Clear("C");
        fComChannelArray -> Clear("C");
        fInputTree -> GetEntry(entry);

        for (auto i=0; i<fW1ChannelArray->GetEntriesFast(); ++i) {
            auto channel = (LKComW1Channel*) fW1ChannelArray -> At(i);
            if (!hasW1(channel->fDetectorID)) {
                auto name = TString::Format("w1_%d",channel->fDetectorID);
                fW1MapArray.push_back({name,channel->fDetectorID,-1,-1,-1});
            }
        }

        for (auto i=0; i<fComChannelArray->GetEntriesFast(); ++i) {
            auto channel = (LKComChannel*) fComChannelArray -> At(i);
            if (!hasCom(channel->fDetectorID)) {
                auto name = TString::Format("com_%d",channel->fDetectorID);
                fComMapArray.push_back({name,channel->fDetectorID,-1,-1});
            }
        }
    }

    fW1ChannelArray -> Clear("C");
    fComChannelArray -> Clear("C");
    return true;
}

void LKCompassReco::ClearEvent()
{
    fW1ChannelArray -> Clear("C");
    fW1HitArray -> Clear("C");
    fComChannelArray -> Clear("C");
    fComHitArray -> Clear("C");
}

void LKCompassReco::MakeHistograms()
{
    for (auto& w1 : fW1MapArray)
    {
        auto bnnMult = GetW1HistBin(w1.detID,"mult");
        auto bnnHitEnergy = GetW1HistBin(w1.detID,"hit_energy");
        auto bnnHitECal = GetW1HistBin(w1.detID,"hit_ecal");
        auto bnnXEnergy = GetW1HistBin(w1.detID,"x_energy");
        auto bnnYEnergy = GetW1HistBin(w1.detID,"y_energy");
        auto bnnXECal = GetW1HistBin(w1.detID,"x_ecal");
        auto bnnYECal = GetW1HistBin(w1.detID,"y_ecal");
        auto bnnTimeDiff = GetW1HistBin(w1.detID,"time_diff");
        W1Hist hist;
        hist.detID = w1.detID;
        hist.name = w1.name;
        hist.hitPattern = new TH2D(Form("%s_hitpattern",w1.name.Data()),Form("%s Y vs X;X strip;Y strip",w1.name.Data()),16,1,17,16,1,17);
        hist.channelMult = bnnMult.NewH1(Form("%s_channel_mult",w1.name.Data()),Form("%s channel multiplicity;mult;counts",w1.name.Data()));
        hist.hitEnergy = bnnHitEnergy.NewH1(Form("%s_hit_energy",w1.name.Data()),Form("%s hit energy;energy;counts",w1.name.Data()));
        hist.hitECal = bnnHitECal.NewH1(Form("%s_hit_ecal",w1.name.Data()),Form("%s hit ECal;ECal;counts",w1.name.Data()));
        hist.xEnergy = bnnXEnergy.NewH1(Form("%s_x_energy",w1.name.Data()),Form("%s X energy;energy;counts",w1.name.Data()));
        hist.yEnergy = bnnYEnergy.NewH1(Form("%s_y_energy",w1.name.Data()),Form("%s Y energy;energy;counts",w1.name.Data()));
        hist.xECal = bnnXECal.NewH1(Form("%s_x_ecal",w1.name.Data()),Form("%s X ECal;ECal;counts",w1.name.Data()));
        hist.yECal = bnnYECal.NewH1(Form("%s_y_ecal",w1.name.Data()),Form("%s Y ECal;ECal;counts",w1.name.Data()));
        hist.previousEntryTimeDiff = bnnTimeDiff.NewH1(Form("%s_previous_entry_time_diff",w1.name.Data()),Form("%s previous entry time difference;#Deltat (#mus);counts",w1.name.Data()));
        hist.hitPattern -> SetDirectory(0);
        hist.channelMult -> SetDirectory(0);
        hist.hitEnergy -> SetDirectory(0);
        hist.hitECal -> SetDirectory(0);
        hist.xEnergy -> SetDirectory(0);
        hist.yEnergy -> SetDirectory(0);
        hist.xECal -> SetDirectory(0);
        hist.yECal -> SetDirectory(0);
        hist.previousEntryTimeDiff -> SetDirectory(0);
        fW1HistArray.push_back(hist);
    }

    for (auto& com : fComMapArray)
    {
        auto bnnMult = GetComHistBin(com.detID,"mult");
        auto bnnHitEnergy = GetComHistBin(com.detID,"hit_energy");
        auto bnnHitECal = GetComHistBin(com.detID,"hit_ecal");
        auto bnnChannelEnergy = GetComHistBin(com.detID,"channel_energy");
        auto bnnChannelECal = GetComHistBin(com.detID,"channel_ecal");
        auto bnnTimeDiff = GetComHistBin(com.detID,"time_diff");
        ComHist hist;
        hist.detID = com.detID;
        hist.name = com.name;
        hist.channelMult = bnnMult.NewH1(Form("%s_channel_mult",com.name.Data()),Form("%s channel multiplicity;mult;counts",com.name.Data()));
        hist.hitEnergy = bnnHitEnergy.NewH1(Form("%s_hit_energy",com.name.Data()),Form("%s hit energy;energy;counts",com.name.Data()));
        hist.hitECal = bnnHitECal.NewH1(Form("%s_hit_ecal",com.name.Data()),Form("%s hit ECal;ECal;counts",com.name.Data()));
        hist.channelEnergy = bnnChannelEnergy.NewH1(Form("%s_channel_energy",com.name.Data()),Form("%s channel energy;energy;counts",com.name.Data()));
        hist.channelECal = bnnChannelECal.NewH1(Form("%s_channel_ecal",com.name.Data()),Form("%s channel ECal;ECal;counts",com.name.Data()));
        hist.previousEntryTimeDiff = bnnTimeDiff.NewH1(Form("%s_previous_entry_time_diff",com.name.Data()),Form("%s previous entry time difference;#Deltat (#mus);counts",com.name.Data()));
        hist.channelMult -> SetDirectory(0);
        hist.hitEnergy -> SetDirectory(0);
        hist.hitECal -> SetDirectory(0);
        hist.channelEnergy -> SetDirectory(0);
        hist.channelECal -> SetDirectory(0);
        hist.previousEntryTimeDiff -> SetDirectory(0);
        fComHistArray.push_back(hist);
    }
}

void LKCompassReco::WriteHistograms(TDirectory* top)
{
    if (top==nullptr)
        return;
}

void LKCompassReco::WriteCanvases(TDirectory* top)
{
    if (top==nullptr)
        return;

    if (fTopGroup==nullptr)
    {
        fTopGroup = new LKDrawingGroup("top");
        for (auto& hist : fW1HistArray)
        {
            auto group = new LKDrawingGroup(hist.name);
            group -> AddHist(hist.hitPattern,"colz","");
            group -> AddHist(hist.channelMult,"","");
            group -> AddHist(hist.hitEnergy,"","");
            group -> AddHist(hist.hitECal,"","");
            group -> AddHist(hist.xEnergy,"","");
            group -> AddHist(hist.yEnergy,"","");
            group -> AddHist(hist.xECal,"","");
            group -> AddHist(hist.yECal,"","");
            group -> AddHist(hist.previousEntryTimeDiff,"","");
            fTopGroup -> AddGroup(group);
        }

        for (auto& hist : fComHistArray)
        {
            auto group = new LKDrawingGroup(hist.name);
            group -> AddHist(hist.channelMult,"","");
            group -> AddHist(hist.channelEnergy,"","");
            group -> AddHist(hist.channelECal,"","");
            group -> AddHist(hist.hitEnergy,"","");
            group -> AddHist(hist.hitECal,"","");
            group -> AddHist(hist.previousEntryTimeDiff,"","");
            fTopGroup -> AddGroup(group);
        }
    }

    top -> cd();
    fTopGroup -> Write("top",TObject::kSingleKey|TObject::kOverwrite);
}

void LKCompassReco::Run()
{
    if (fInputTree==nullptr && !Init())
        return;

    if (fInputIsReco)
        RunECalUpdate();
    else
        RunRawReco();
}

void LKCompassReco::RunRawReco()
{
    auto numEntries = fInputTree -> GetEntries();
    auto entry = TMath::Max((Long64_t)0,fFirstEntry);
    auto endEntry = fLastEntry>0 ? TMath::Min(fLastEntry+1,numEntries) : numEntries;
    fPreviousW1EntryTimeMap.clear();
    fPreviousComEntryTimeMap.clear();

    cout << "Entries: " << numEntries << endl;
    cout << "Range: " << entry << " - " << endEntry << endl;

    while (entry<endEntry)
    {
        ClearEvent();

        fInputTree -> GetEntry(entry);
        auto eventStartTime = fTimestamp;

        while (entry<endEntry && fTimestamp<eventStartTime+fTimeWindow)
        {
            AddRawHitToEvent();
            entry++;
            if (entry<endEntry)
                fInputTree -> GetEntry(entry);
        }

        BuildHits();
        fOutputTree -> Fill();

        if (fOutputTree->GetEntries()%1000==0) {
            cout << "\rEvent " << fOutputTree->GetEntries() << " entry " << entry << " / " << endEntry;
            cout.flush();
        }
    }

    cout << endl;
    End();
}

void LKCompassReco::RunECalUpdate()
{
    auto numEntries = fInputTree -> GetEntries();
    auto entry = TMath::Max((Long64_t)0,fFirstEntry);
    auto endEntry = fLastEntry>0 ? TMath::Min(fLastEntry+1,numEntries) : numEntries;
    fPreviousW1EntryTimeMap.clear();
    fPreviousComEntryTimeMap.clear();

    cout << "Entries: " << numEntries << endl;
    cout << "Range: " << entry << " - " << endEntry << endl;

    while (entry<endEntry)
    {
        ClearEvent();
        fInputTree -> GetEntry(entry);
        UpdateChannelECal();
        FillTimeDiffHistograms();
        fW1HitArray -> Clear("C");
        fComHitArray -> Clear("C");
        BuildHits();
        fOutputTree -> Fill();

        entry++;
        if (fOutputTree->GetEntries()%1000==0) {
            cout << "\rEvent " << fOutputTree->GetEntries() << " entry " << entry << " / " << endEntry;
            cout.flush();
        }
    }

    cout << endl;
    End();
}

void LKCompassReco::AddRawHitToEvent()
{
    const W1Map* w1 = nullptr;
    bool side = false;
    int strip = -1;
    if (FindW1(fBoard,fChannel,w1,side,strip)) {
        auto idx = fW1ChannelArray -> GetEntriesFast();
        auto channel = new ((*fW1ChannelArray)[idx]) LKComW1Channel();
        channel -> Set(w1->detID,side,strip,fTimestamp,fEnergy,fEnergyShort,CalibrateW1(w1->detID,fEnergy));
        FillW1TimeDiff(w1->detID,fTimestamp);
        return;
    }

    const ComMap* com = nullptr;
    if (FindCom(fBoard,fChannel,com)) {
        auto idx = fComChannelArray -> GetEntriesFast();
        auto channel = new ((*fComChannelArray)[idx]) LKComChannel();
        channel -> Set(com->detID,fTimestamp,fEnergy,fEnergyShort,CalibrateCom(com->detID,fEnergy));
        FillComTimeDiff(com->detID,fTimestamp);
    }
}

bool LKCompassReco::FindW1(UShort_t board, UShort_t channel, const W1Map*& map, bool& side, int& strip) const
{
    for (auto& w1 : fW1MapArray)
    {
        if (board!=w1.board)
            continue;

        auto yStrip = channel - w1.yStart;
        if (yStrip>=0 && yStrip<16) {
            map = &w1;
            side = false;
            strip = yStrip + 1;
            return true;
        }

        auto xStrip = channel - w1.xStart;
        if (xStrip>=0 && xStrip<16) {
            map = &w1;
            side = true;
            strip = xStrip + 1;
            return true;
        }
    }

    return false;
}

bool LKCompassReco::FindCom(UShort_t board, UShort_t channel, const ComMap*& map) const
{
    for (auto& com : fComMapArray)
    {
        if (board==com.board && channel==com.channel) {
            map = &com;
            return true;
        }
    }

    return false;
}

void LKCompassReco::UpdateChannelECal()
{
    for (auto i=0; i<fW1ChannelArray->GetEntriesFast(); ++i) {
        auto channel = (LKComW1Channel*) fW1ChannelArray -> At(i);
        channel -> fECal = CalibrateW1(channel->fDetectorID,channel->fEnergy);
    }

    for (auto i=0; i<fComChannelArray->GetEntriesFast(); ++i) {
        auto channel = (LKComChannel*) fComChannelArray -> At(i);
        channel -> fECal = CalibrateCom(channel->fDetectorID,channel->fEnergy);
    }
}

void LKCompassReco::FillTimeDiffHistograms()
{
    for (auto i=0; i<fW1ChannelArray->GetEntriesFast(); ++i) {
        auto channel = (LKComW1Channel*) fW1ChannelArray -> At(i);
        FillW1TimeDiff(channel->fDetectorID,channel->fTime);
    }

    for (auto i=0; i<fComChannelArray->GetEntriesFast(); ++i) {
        auto channel = (LKComChannel*) fComChannelArray -> At(i);
        FillComTimeDiff(channel->fDetectorID,channel->fTime);
    }
}

void LKCompassReco::FillW1TimeDiff(short detID, ULong64_t time)
{
    auto timeIt = fPreviousW1EntryTimeMap.find(detID);
    if (timeIt!=fPreviousW1EntryTimeMap.end())
    {
        for (auto& hist : fW1HistArray) {
            if (hist.detID!=detID)
                continue;
            if (time>=timeIt->second)
                hist.previousEntryTimeDiff -> Fill((time-timeIt->second)*1.e-6);
            else
                hist.previousEntryTimeDiff -> Fill(-(double)(timeIt->second-time)*1.e-6);
            break;
        }
    }

    fPreviousW1EntryTimeMap[detID] = time;
}

void LKCompassReco::FillComTimeDiff(short detID, ULong64_t time)
{
    auto timeIt = fPreviousComEntryTimeMap.find(detID);
    if (timeIt!=fPreviousComEntryTimeMap.end())
    {
        for (auto& hist : fComHistArray) {
            if (hist.detID!=detID)
                continue;
            if (time>=timeIt->second)
                hist.previousEntryTimeDiff -> Fill((time-timeIt->second)*1.e-6);
            else
                hist.previousEntryTimeDiff -> Fill(-(double)(timeIt->second-time)*1.e-6);
            break;
        }
    }

    fPreviousComEntryTimeMap[detID] = time;
}

double LKCompassReco::CalibrateW1(short detID, short energy) const
{
    auto par = GetW1ECalPar(detID);
    return par.c0 + par.c1*energy + par.c2*energy*energy;
}

double LKCompassReco::CalibrateCom(short detID, short energy) const
{
    auto par = GetComECalPar(detID);
    return par.c0 + par.c1*energy + par.c2*energy*energy;
}

LKCompassReco::ECalPar LKCompassReco::GetW1ECalPar(short detID) const
{
    auto it = fW1ECalParMap.find(detID);
    if (it!=fW1ECalParMap.end())
        return it->second;
    return ECalPar();
}

LKCompassReco::ECalPar LKCompassReco::GetComECalPar(short detID) const
{
    auto it = fComECalParMap.find(detID);
    if (it!=fComECalParMap.end())
        return it->second;
    return ECalPar();
}

LKBinning LKCompassReco::GetW1HistBin(short detID, TString type) const
{
    auto it = fW1HistBinMap.find(detID);
    if (it!=fW1HistBinMap.end()) {
        auto it2 = it->second.find(type);
        if (it2!=it->second.end() && it2->second!=nullptr)
            return *it2->second;
    }

    if (type=="mult")
        return LKBinning(20,0,20);
    if (type=="hit_energy")
        return LKBinning(1000,0,20000);
    if (type=="hit_ecal")
        return LKBinning(1000,0,20000);
    if (type=="x_energy")
        return LKBinning(1000,0,10000);
    if (type=="y_energy")
        return LKBinning(1000,0,10000);
    if (type=="x_ecal")
        return LKBinning(1000,0,10000);
    if (type=="y_ecal")
        return LKBinning(1000,0,10000);
    if (type=="time_diff")
        return LKBinning(500,0,fTimeWindow*50.e-6);

    return LKBinning(100,0,1);
}

LKBinning LKCompassReco::GetComHistBin(short detID, TString type) const
{
    auto it = fComHistBinMap.find(detID);
    if (it!=fComHistBinMap.end()) {
        auto it2 = it->second.find(type);
        if (it2!=it->second.end() && it2->second!=nullptr)
            return *it2->second;
    }

    if (type=="mult")
        return LKBinning(20,0,20);
    if (type=="hit_energy")
        return LKBinning(1000,0,20000);
    if (type=="hit_ecal")
        return LKBinning(1000,0,20000);
    if (type=="channel_energy")
        return LKBinning(1000,0,20000);
    if (type=="channel_ecal")
        return LKBinning(1000,0,20000);
    if (type=="time_diff")
        return LKBinning(500,0,fTimeWindow*50.e-6);

    return LKBinning(100,0,1);
}

void LKCompassReco::BuildHits()
{
    BuildW1Hits();
    BuildComHits();
}

void LKCompassReco::BuildW1Hits()
{
    for (auto& w1 : fW1MapArray)
    {
        short mult = 0;
        short x = -1;
        short y = -1;
        int energy = 0;
        double ecal = 0;
        ULong64_t time = 0;
        auto hist = (W1Hist*) nullptr;
        for (auto& hist0 : fW1HistArray)
            if (hist0.detID==w1.detID)
                hist = &hist0;

        for (auto i=0; i<fW1ChannelArray->GetEntriesFast(); ++i)
        {
            auto channel = (LKComW1Channel*) fW1ChannelArray -> At(i);
            if (channel->fDetectorID!=w1.detID)
                continue;

            mult++;
            if (time==0 || channel->fTime<time)
                time = channel->fTime;

            if (channel->fSide)
            {
                x = channel->fStrip;
                if (hist!=nullptr) {
                    hist->xEnergy -> Fill(channel->fEnergy);
                    hist->xECal -> Fill(channel->fECal);
                }
            }
            else
            {
                y = channel->fStrip;
                if (hist!=nullptr) {
                    hist->yEnergy -> Fill(channel->fEnergy);
                    hist->yECal -> Fill(channel->fECal);
                }
            }

            if (channel->fSide==fW1MainSide) {
                energy += channel->fEnergy;
                ecal += channel->fECal;
            }
        }

        if (mult==0)
            continue;

        if (hist!=nullptr) {
            hist -> channelMult -> Fill(mult);
            if (x>0 && y>0)
                hist -> hitPattern -> Fill(x,y);
        }

        auto idx = fW1HitArray -> GetEntriesFast();
        auto hit = new ((*fW1HitArray)[idx]) LKComW1Hit();
        hit -> fDetectorID = w1.detID;
        hit -> fChannelMult = mult;
        hit -> fTime = time;
        hit -> fEnergy = energy>0 ? (short) TMath::Min(energy,32767) : -1;
        hit -> fECal = energy>0 ? ecal : -1;
        hit -> fX = x;
        hit -> fY = y;
        if (hist!=nullptr && energy>0) {
            hist -> hitEnergy -> Fill(energy);
            hist -> hitECal -> Fill(ecal);
        }
    }
}

void LKCompassReco::BuildComHits()
{
    for (auto& com : fComMapArray)
    {
        short mult = 0;
        int energy = 0;
        double ecal = 0;
        ULong64_t time = 0;
        auto hist = (ComHist*) nullptr;
        for (auto& hist0 : fComHistArray)
            if (hist0.detID==com.detID)
                hist = &hist0;

        for (auto i=0; i<fComChannelArray->GetEntriesFast(); ++i)
        {
            auto channel = (LKComChannel*) fComChannelArray -> At(i);
            if (channel->fDetectorID!=com.detID)
                continue;

            mult++;
            energy += channel->fEnergy;
            ecal += channel->fECal;
            if (hist!=nullptr) {
                hist -> channelEnergy -> Fill(channel->fEnergy);
                hist -> channelECal -> Fill(channel->fECal);
            }
            if (time==0 || channel->fTime<time)
                time = channel->fTime;
        }

        if (mult==0)
            continue;

        if (hist!=nullptr)
            hist -> channelMult -> Fill(mult);

        auto idx = fComHitArray -> GetEntriesFast();
        auto hit = new ((*fComHitArray)[idx]) LKComHit();
        hit -> fDetectorID = com.detID;
        hit -> fChannelMult = mult;
        hit -> fTime = time;
        hit -> fEnergy = energy>0 ? (short) TMath::Min(energy,32767) : -1;
        hit -> fECal = energy>0 ? ecal : -1;
        if (hist!=nullptr && energy>0) {
            hist -> hitEnergy -> Fill(energy);
            hist -> hitECal -> Fill(ecal);
        }
    }
}

TString LKCompassReco::MakeDefaultOutputFileName() const
{
    auto name = fInputFileName;
    if (!fInputIsReco) {
        name.ReplaceAll(".root",".reco.root");
        return name;
    }

    if (name.EndsWith(".root"))
        name.Remove(name.Length()-5);

    auto dot = name.Last('.');
    if (dot>=0) {
        auto suffix = TString(name(dot+1,name.Length()-dot-1));
        if (suffix.BeginsWith("reco")) {
            auto numberText = TString(suffix(4,suffix.Length()-4));
            auto nextNumber = 2;
            auto isNumber = !numberText.IsNull();
            for (auto i=0; i<numberText.Length(); ++i)
                if (!isdigit(numberText[i]))
                    isNumber = false;
            if (isNumber)
                nextNumber = numberText.Atoi() + 1;
            name.Remove(dot);
            name += TString::Format(".reco%d.root",nextNumber);
            return name;
        }
    }

    name += ".reco2.root";
    return name;
}

void LKCompassReco::Draw(Option_t* option)
{
    if (fTopGroup==nullptr) {
        cout << "Draw needs a drawing group. Run first." << endl;
        return;
    }

    fTopGroup -> Draw(option);
}

void LKCompassReco::End()
{
    if (fOutputFile==nullptr)
        return;

    auto numEvents = fOutputTree -> GetEntries();
    fOutputFile -> cd();
    fOutputTree -> Write();
    WriteCanvases(fOutputFile);
    fOutputFile -> Close();

    cout << "Output tree entries: " << numEvents << endl;
}
