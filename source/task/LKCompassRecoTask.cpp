#include "LKCompassRecoTask.h"

#include "LKComChannel.h"
#include "LKComHit.h"
#include "LKComW1Channel.h"
#include "LKComW1Hit.h"
#include "LKCompassReco.h"
#include "LKBinning.h"
#include "LKDrawingGroup.h"
#include "LKLogger.h"
#include "LKRun.h"
#include "TClonesArray.h"
#include "TGraph.h"
#include "TH1D.h"
#include "TH2D.h"

#include <algorithm>
#include <cmath>
#include <limits>

ClassImp(LKCompassRecoTask)

namespace {
    struct TimestampDifferenceRange {
        const char* name;
        const char* title;
        double maximum;
    };

    const TimestampDifferenceRange kTimestampDifferenceRanges[] = {
        {"0p01us", "0.01 #mus",   0.01},
        {"0p1us",  "0.1 #mus",    0.1},
        {"1us",    "1 #mus",      1.0},
        {"100us",  "100 #mus",  100.0},
    };

    bool ConvertTimeWindowToPs(double value, TString unit, Long64_t& valueInPs)
    {
        unit.ToLower();
        double scale = 0;
        if      (unit == "ps") scale = 1.;
        else if (unit == "ns") scale = 1.e3;
        else if (unit == "us") scale = 1.e6;
        else if (unit == "ms") scale = 1.e9;
        else return false;

        const double converted = value*scale;
        if (!std::isfinite(converted) || converted <= 0
            || converted > double(std::numeric_limits<Long64_t>::max()))
            return false;

        valueInPs = Long64_t(std::llround(converted));
        return valueInPs > 0;
    }
}

LKCompassRecoTask::LKCompassRecoTask()
    : LKTask("LKCompassRecoTask", "CoMPASS ROOT event trigger and reconstruction")
{
}

LKCompassRecoTask::~LKCompassRecoTask()
{
    for (auto& det : fW1HistBinMap)
        for (auto& item : det.second)
            delete item.second;
    for (auto& det : fComHistBinMap)
        for (auto& item : det.second)
            delete item.second;
    delete fReco;
}

void LKCompassRecoTask::SetInputFile(TString name)
{
    fInputFileArray.clear();
    if (!name.IsNull())
        fInputFileArray.push_back(name);
}

void LKCompassRecoTask::SetEntryRange(Long64_t firstEntry, Long64_t lastEntry)
{
    fFirstEntry = firstEntry;
    fLastEntry = lastEntry;
}

bool LKCompassRecoTask::SetTimeWindow(Double_t value, TString unit)
{
    Long64_t valueInPs = 0;
    if (!ConvertTimeWindowToPs(value,unit,valueInPs)) {
        lk_error << "Invalid CoMPASS time window: " << value << " " << unit
                 << ". Supported units are ps, ns, us, and ms." << endl;
        return false;
    }
    fTimeWindow = valueInPs;
    return true;
}

void LKCompassRecoTask::AddTriggerInputFile(TString fileName, TString)
{
    if (!fileName.IsNull())
        fTriggerInputFileNameArray.push_back(fileName);
}

void LKCompassRecoTask::AddW1(short detID, short board, short junctionChannelStart, short ohmicChannelStart)
{
    for (auto& map : fW1MapArray) {
        if (map.detID == detID) {
            lk_error << "W1 detector ID is already used: " << detID << endl;
            return;
        }
    }
    fW1MapArray.push_back({detID, board, junctionChannelStart, ohmicChannelStart});
}

void LKCompassRecoTask::AddCom(short detID, short board, short channel)
{
    for (auto& map : fComMapArray) {
        if (map.detID == detID) {
            lk_error << "Com detector ID is already used: " << detID << endl;
            return;
        }
    }
    fComMapArray.push_back({detID, board, channel});
}

bool LKCompassRecoTask::SetW1XOrigin(short detID, TString origin)
{
    origin.ToLower();
    if (origin == "left")
        fW1XStartsFromLeftMap[detID] = true;
    else if (origin == "right")
        fW1XStartsFromLeftMap[detID] = false;
    else {
        lk_error << "Unknown W1 X origin for detector " << detID << ": " << origin
                 << ". Use left or right." << endl;
        return false;
    }
    return true;
}

bool LKCompassRecoTask::SetW1YOrigin(short detID, TString origin)
{
    origin.ToLower();
    if (origin == "top")
        fW1YStartsFromTopMap[detID] = true;
    else if (origin == "bottom")
        fW1YStartsFromTopMap[detID] = false;
    else {
        lk_error << "Unknown W1 Y origin for detector " << detID << ": " << origin
                 << ". Use top or bottom." << endl;
        return false;
    }
    return true;
}

void LKCompassRecoTask::SetECalParametersW1(short detID, double c0, double c1, double c2)
{
    fW1ECalParMap[detID] = {c0, c1, c2};
}

void LKCompassRecoTask::SetECalParametersCom(short detID, double c0, double c1, double c2)
{
    fComECalParMap[detID] = {c0, c1, c2};
}

bool LKCompassRecoTask::CheckBinning(const LKBinning& binning) const
{
    if (binning.nx() <= 0 || binning.x2() <= binning.x1()) {
        lk_error << "Invalid histogram binning: " << binning.nx()
                 << ", " << binning.x1() << ", " << binning.x2() << endl;
        return false;
    }
    return true;
}

void LKCompassRecoTask::SetHistogramBinningW1(short detID, TString type, const LKBinning& binning)
{
    if (!CheckBinning(binning))
        return;
    type.ToLower();
    auto& map = fW1HistBinMap[detID];
    auto set = [&map,&binning](TString name) {
        delete map[name];
        map[name] = new LKBinning(binning);
    };

    if (type=="mult" || type=="channel_mult") set("mult");
    else if (type=="energy" || type=="hit_energy") set("hit_energy");
    else if (type=="ecal" || type=="hit_ecal") set("hit_ecal");
    else if (type=="channel_energy") {
        set("junction_channel_energy");
        set("ohmic_channel_energy");
    }
    else if (type=="junction_channel_energy" || type=="x_channel_energy") set("junction_channel_energy");
    else if (type=="ohmic_channel_energy" || type=="y_channel_energy") set("ohmic_channel_energy");
    else if (type=="x_energy") set("x_energy");
    else if (type=="y_energy") set("y_energy");
    else if (type=="x_ecal") set("x_ecal");
    else if (type=="y_ecal") set("y_ecal");
    else if (type=="time_diff" || type=="timediff" || type=="previous_entry_time_diff") {
        for (auto& range : kTimestampDifferenceRanges)
            set(Form("time_diff_%s",range.name));
    }
    else if (type.BeginsWith("time_diff_")) set(type);
    else lk_warning << "Unknown W1 histogram type: " << type << endl;
}

void LKCompassRecoTask::SetHistogramBinningW1(short detID, TString type, int nBins, double xMin, double xMax)
{
    SetHistogramBinningW1(detID, type, LKBinning(nBins,xMin,xMax));
}

void LKCompassRecoTask::SetHistogramBinningCom(short detID, TString type, const LKBinning& binning)
{
    if (!CheckBinning(binning))
        return;
    type.ToLower();
    auto& map = fComHistBinMap[detID];
    auto set = [&map,&binning](TString name) {
        delete map[name];
        map[name] = new LKBinning(binning);
    };

    if (type=="mult" || type=="channel_mult") set("mult");
    else if (type=="energy" || type=="hit_energy") set("hit_energy");
    else if (type=="ecal" || type=="hit_ecal") set("hit_ecal");
    else if (type=="channel_energy") set("channel_energy");
    else if (type=="channel_ecal") set("channel_ecal");
    else if (type=="time_diff" || type=="timediff" || type=="previous_entry_time_diff") {
        for (auto& range : kTimestampDifferenceRanges)
            set(Form("time_diff_%s",range.name));
    }
    else if (type.BeginsWith("time_diff_")) set(type);
    else lk_warning << "Unknown Com histogram type: " << type << endl;
}

void LKCompassRecoTask::SetHistogramBinningCom(short detID, TString type, int nBins, double xMin, double xMax)
{
    SetHistogramBinningCom(detID, type, LKBinning(nBins,xMin,xMax));
}

bool LKCompassRecoTask::ConfigureParameters()
{
    const bool hasInputTreeName = fPar->CheckPar("LKCompassRecoTask/InputTreeName");
    const bool hasTimeWindow = fPar->CheckPar("LKCompassRecoTask/TimeWindow");
    const bool hasFirstEntry = fPar->CheckPar("LKCompassRecoTask/FirstEntry");
    const bool hasLastEntry = fPar->CheckPar("LKCompassRecoTask/LastEntry");
    const bool hasEnergyThreshold = fPar->CheckPar("LKCompassRecoTask/EnergyThreshold");
    const bool hasSortInput = fPar->CheckPar("LKCompassRecoTask/SortInput");
    const bool hasW1MainSide = fPar->CheckPar("LKCompassRecoTask/W1MainSide");

    fPar->Require("LKCompassRecoTask/InputFileName", "", "CoMPASS ROOT input file", "t");
    fPar->Require("LKCompassRecoTask/InputTreeName", "Data_R", "CoMPASS raw tree name", "t");
    fPar->Require("LKCompassRecoTask/TimeWindow", "4000000", "timestamp grouping window; optional unit ps/ns/us/ms, default ps", "t");
    fPar->Require("LKCompassRecoTask/FirstEntry", "0", "first raw entry", "t");
    fPar->Require("LKCompassRecoTask/LastEntry", "0", "last raw entry, inclusive; 0 means end", "t");
    fPar->Require("LKCompassRecoTask/EnergyThreshold", "0", "minimum raw Energy to collect", "t");
    fPar->Require("LKCompassRecoTask/SortInput", "true", "sort raw entries by Timestamp before reconstruction", "t");
    fPar->Require("LKCompassRecoTask/W1Map", "", "detID,board,junctionStart,ohmicStart repeated", "t");
    fPar->Require("LKCompassRecoTask/W1XOrigin", "", "detID,left|right repeated; default left", "t");
    fPar->Require("LKCompassRecoTask/W1YOrigin", "", "detID,top|bottom repeated; default top", "t");
    fPar->Require("LKCompassRecoTask/ComMap", "", "detID,board,channel repeated", "t");
    fPar->Require("LKCompassRecoTask/W1ECal", "", "detID,c0,c1,c2 repeated", "t");
    fPar->Require("LKCompassRecoTask/ComECal", "", "detID,c0,c1,c2 repeated", "t");
    fPar->Require("LKCompassRecoTask/W1MainSide", "junction", "junction or ohmic", "t");

    if (hasInputTreeName) fPar->UpdatePar(fInputTreeName, "LKCompassRecoTask/InputTreeName");
    if (hasTimeWindow && !ConfigureTimeWindow())
        return false;
    if (hasFirstEntry) fPar->UpdatePar(fFirstEntry, "LKCompassRecoTask/FirstEntry");
    if (hasLastEntry) fPar->UpdatePar(fLastEntry, "LKCompassRecoTask/LastEntry");
    if (hasEnergyThreshold) fPar->UpdatePar(fEnergyThreshold, "LKCompassRecoTask/EnergyThreshold");
    if (hasSortInput) fPar->UpdatePar(fSortInput, "LKCompassRecoTask/SortInput");

    if (hasW1MainSide) {
        TString mainSide = "junction";
        fPar->UpdatePar(mainSide, "LKCompassRecoTask/W1MainSide");
        mainSide.ToLower();
        if (mainSide == "junction" || mainSide == "x")
            fW1MainSide = false;
        else if (mainSide == "ohmic" || mainSide == "y")
            fW1MainSide = true;
        else {
            lk_error << "Unknown LKCompassRecoTask/W1MainSide: " << mainSide << endl;
            return false;
        }
    }

    if (fTimeWindow <= 0) {
        lk_error << "LKCompassRecoTask/TimeWindow must be positive." << endl;
        return false;
    }
    if (fEnergyThreshold < 0) {
        lk_error << "LKCompassRecoTask/EnergyThreshold must not be negative." << endl;
        return false;
    }
    return true;
}

bool LKCompassRecoTask::ConfigureTimeWindow()
{
    const TString parameterName = "LKCompassRecoTask/TimeWindow";
    const auto numValues = fPar->GetParN(parameterName);
    if (numValues < 1 || numValues > 2) {
        lk_error << parameterName << " must be '<value>' or '<value> <unit>'." << endl;
        return false;
    }

    const double value = fPar->GetParDouble(parameterName,0);
    TString unit = numValues == 2 ? fPar->GetParString(parameterName,1) : "ps";
    if (!SetTimeWindow(value,unit))
        return false;

    lk_info << "CoMPASS time window: " << value << " " << unit
            << " = " << fTimeWindow << " ps" << endl;
    return true;
}

bool LKCompassRecoTask::ConfigureInputFiles()
{
    if (fPar->CheckPar("LKCompassRecoTask/InputFileName")) {
        auto inputFileName = fPar->GetParString("LKCompassRecoTask/InputFileName");
        if (!inputFileName.IsNull()) {
            if (!fInputFileArray.empty() || !fTriggerInputFileNameArray.empty())
                lk_warning << "LKCompassRecoTask/InputFileName overrides other CoMPASS inputs." << endl;
            fInputFileArray.clear();
            fInputFileArray.push_back(inputFileName);
        }
    }

    if (fInputFileArray.empty())
        fInputFileArray = fTriggerInputFileNameArray;

    if (fInputFileArray.empty()) {
        lk_error << "No CoMPASS input file is set." << endl;
        return false;
    }
    return true;
}

bool LKCompassRecoTask::ConfigureMappings()
{
    if (fPar->CheckPar("LKCompassRecoTask/W1Map")) {
        auto values = fPar->GetParVInt("LKCompassRecoTask/W1Map");
        if (!values.empty() && values.size() % 4 != 0) {
            lk_error << "LKCompassRecoTask/W1Map needs groups of 4 values." << endl;
            return false;
        }
        for (size_t i=0; i+3<values.size(); i+=4)
            AddW1(values[i], values[i+1], values[i+2], values[i+3]);
    }

    if (fPar->CheckPar("LKCompassRecoTask/ComMap")) {
        auto values = fPar->GetParVInt("LKCompassRecoTask/ComMap");
        if (!values.empty() && values.size() % 3 != 0) {
            lk_error << "LKCompassRecoTask/ComMap needs groups of 3 values." << endl;
            return false;
        }
        for (size_t i=0; i+2<values.size(); i+=3)
            AddCom(values[i], values[i+1], values[i+2]);
    }

    if (fPar->CheckPar("LKCompassRecoTask/W1XOrigin")) {
        auto values = fPar->GetParVString("LKCompassRecoTask/W1XOrigin");
        if (!values.empty() && values.size() % 2 != 0) {
            lk_error << "LKCompassRecoTask/W1XOrigin needs detID,left|right pairs." << endl;
            return false;
        }
        for (size_t i=0; i+1<values.size(); i+=2) {
            if (!values[i].IsDec()) {
                lk_error << "Invalid detector ID in LKCompassRecoTask/W1XOrigin: "
                         << values[i] << endl;
                return false;
            }
            const short detID = short(values[i].Atoi());
            const auto found = std::find_if(fW1MapArray.begin(),fW1MapArray.end(),
                [detID](const W1Map& map) { return map.detID == detID; });
            if (found == fW1MapArray.end()) {
                lk_error << "W1 detector " << detID
                         << " in LKCompassRecoTask/W1XOrigin is not configured in W1Map." << endl;
                return false;
            }
            if (!SetW1XOrigin(detID,values[i+1]))
                return false;
        }
    }

    if (fPar->CheckPar("LKCompassRecoTask/W1YOrigin")) {
        auto values = fPar->GetParVString("LKCompassRecoTask/W1YOrigin");
        if (!values.empty() && values.size() % 2 != 0) {
            lk_error << "LKCompassRecoTask/W1YOrigin needs detID,top|bottom pairs." << endl;
            return false;
        }
        for (size_t i=0; i+1<values.size(); i+=2) {
            if (!values[i].IsDec()) {
                lk_error << "Invalid detector ID in LKCompassRecoTask/W1YOrigin: "
                         << values[i] << endl;
                return false;
            }
            const short detID = short(values[i].Atoi());
            const auto found = std::find_if(fW1MapArray.begin(),fW1MapArray.end(),
                [detID](const W1Map& map) { return map.detID == detID; });
            if (found == fW1MapArray.end()) {
                lk_error << "W1 detector " << detID
                         << " in LKCompassRecoTask/W1YOrigin is not configured in W1Map." << endl;
                return false;
            }
            if (!SetW1YOrigin(detID,values[i+1]))
                return false;
        }
    }

    auto loadECal = [this](TString name, bool w1) {
        if (!fPar->CheckPar(name))
            return true;
        auto values = fPar->GetParVDouble(name);
        if (!values.empty() && values.size() % 4 != 0) {
            lk_error << name << " needs groups of 4 values." << endl;
            return false;
        }
        for (size_t i=0; i+3<values.size(); i+=4) {
            if (w1) SetECalParametersW1(short(values[i]), values[i+1], values[i+2], values[i+3]);
            else    SetECalParametersCom(short(values[i]), values[i+1], values[i+2], values[i+3]);
        }
        return true;
    };

    if (!loadECal("LKCompassRecoTask/W1ECal", true))
        return false;
    if (!loadECal("LKCompassRecoTask/ComECal", false))
        return false;

    if (fW1MapArray.empty() && fComMapArray.empty()) {
        lk_error << "No CoMPASS detector mapping is configured." << endl;
        return false;
    }
    return true;
}

LKBinning LKCompassRecoTask::GetW1HistBin(short detID, TString type) const
{
    auto detIt = fW1HistBinMap.find(detID);
    if (detIt != fW1HistBinMap.end()) {
        auto binIt = detIt->second.find(type);
        if (binIt != detIt->second.end() && binIt->second != nullptr)
            return *binIt->second;
    }
    if (type=="mult") return LKBinning(20,0,20);
    if (type=="hit_energy" || type=="hit_ecal") return LKBinning(1000,0,20000);
    if (type=="junction_channel_energy" || type=="ohmic_channel_energy") return LKBinning(1000,0,10000);
    if (type=="x_energy" || type=="y_energy" || type=="x_ecal" || type=="y_ecal") return LKBinning(1000,0,10000);
    for (auto& range : kTimestampDifferenceRanges)
        if (type == Form("time_diff_%s",range.name))
            return LKBinning(100,0,range.maximum);
    return LKBinning(100,0,1);
}

LKBinning LKCompassRecoTask::GetComHistBin(short detID, TString type) const
{
    auto detIt = fComHistBinMap.find(detID);
    if (detIt != fComHistBinMap.end()) {
        auto binIt = detIt->second.find(type);
        if (binIt != detIt->second.end() && binIt->second != nullptr)
            return *binIt->second;
    }
    if (type=="mult") return LKBinning(20,0,20);
    if (type=="hit_energy" || type=="hit_ecal" || type=="channel_energy" || type=="channel_ecal")
        return LKBinning(1000,0,20000);
    for (auto& range : kTimestampDifferenceRanges)
        if (type == Form("time_diff_%s",range.name))
            return LKBinning(100,0,range.maximum);
    return LKBinning(100,0,1);
}

void LKCompassRecoTask::MakeHistograms()
{
    for (auto& w1 : fW1MapArray)
    {
        W1Hist hist;
        hist.detID = w1.detID;
        hist.name = Form("w1_%d",w1.detID);
        const bool yStartsFromTop = GetW1DisplayY(w1.detID,1) == 16;
        const auto yOrigin = yStartsFromTop ? "top" : "bottom";
        hist.hitPattern = new TH2D(Form("%s_hitpattern",hist.name.Data()),Form("%s Ohmic Y vs Junction X;Junction X strip;Ohmic Y strip (1=%s)",hist.name.Data(),yOrigin),16,1,17,16,1,17);
        hist.junctionPattern = new TH1D(Form("%s_junction_pattern",hist.name.Data()),Form("%s Junction channels;Junction X strip;Counts",hist.name.Data()),16,1,17);
        hist.ohmicPattern = new TH1D(Form("%s_ohmic_pattern",hist.name.Data()),Form("%s Ohmic channels;Ohmic Y strip;Counts",hist.name.Data()),16,1,17);
        hist.rawChannelEnergy = (LKBinning(64,0,64)*GetW1HistBin(w1.detID,"hit_energy")).NewH2(
            Form("%s_energy_vs_raw_channel",hist.name.Data()),
            Form("%s energy vs raw CoMPASS channel;Raw channel;Energy",hist.name.Data()));
        hist.channelMult = GetW1HistBin(w1.detID,"mult").NewH1(Form("%s_channel_mult",hist.name.Data()),Form("%s channel multiplicity;mult;counts",hist.name.Data()));
        hist.hitEnergy = GetW1HistBin(w1.detID,"hit_energy").NewH1(Form("%s_hit_energy",hist.name.Data()),Form("%s hit energy;energy;counts",hist.name.Data()));
        hist.hitECal = GetW1HistBin(w1.detID,"hit_ecal").NewH1(Form("%s_hit_ecal",hist.name.Data()),Form("%s hit ECal;ECal;counts",hist.name.Data()));
        hist.xEnergy = GetW1HistBin(w1.detID,"x_energy").NewH1(Form("%s_x_energy",hist.name.Data()),Form("%s X energy;energy;counts",hist.name.Data()));
        hist.yEnergy = GetW1HistBin(w1.detID,"y_energy").NewH1(Form("%s_y_energy",hist.name.Data()),Form("%s Y energy;energy;counts",hist.name.Data()));
        hist.xECal = GetW1HistBin(w1.detID,"x_ecal").NewH1(Form("%s_x_ecal",hist.name.Data()),Form("%s X ECal;ECal;counts",hist.name.Data()));
        hist.yECal = GetW1HistBin(w1.detID,"y_ecal").NewH1(Form("%s_y_ecal",hist.name.Data()),Form("%s Y ECal;ECal;counts",hist.name.Data()));
        for (auto& range : kTimestampDifferenceRanges) {
            const auto type = Form("time_diff_%s",range.name);
            hist.previousEntryTimeDiff.push_back(GetW1HistBin(w1.detID,type).NewH1(
                Form("%s_previous_entry_time_difference_%s",hist.name.Data(),range.name),
                Form("%s adjacent entry time difference up to %s;|#Deltat| (#mus);counts",hist.name.Data(),range.title)));
        }
        for (auto strip=1; strip<=16; ++strip)
            hist.channelEnergy.push_back(GetW1HistBin(w1.detID,"junction_channel_energy").NewH1(Form("%s_junction_x%02d_energy",hist.name.Data(),strip),Form("%s junction X strip %d energy;energy;counts",hist.name.Data(),strip)));
        for (auto strip=1; strip<=16; ++strip)
            hist.channelEnergy.push_back(GetW1HistBin(w1.detID,"ohmic_channel_energy").NewH1(Form("%s_ohmic_y%02d_energy",hist.name.Data(),strip),Form("%s ohmic Y strip %d energy;energy;counts",hist.name.Data(),strip)));

        for (auto bin=1; bin<=16; ++bin) {
            const auto strip = yStartsFromTop ? 17-bin : bin;
            hist.hitPattern->GetYaxis()->SetBinLabel(bin,TString::Format("%02d",strip));
        }
        hist.hitPattern->SetStats(0);
        hist.junctionPattern->SetStats(1);
        hist.ohmicPattern->SetStats(1);
        std::vector<TH1*> histograms = {
            hist.hitPattern, hist.junctionPattern, hist.ohmicPattern, hist.rawChannelEnergy,
            hist.channelMult, hist.hitEnergy, hist.hitECal,
            hist.xEnergy, hist.yEnergy, hist.xECal, hist.yECal
        };
        for (auto object : histograms)
            object->SetDirectory(nullptr);
        for (auto object : hist.previousEntryTimeDiff)
            object->SetDirectory(nullptr);
        for (auto object : hist.channelEnergy)
            object->SetDirectory(nullptr);
        fW1HistArray.push_back(hist);
    }

    for (auto& com : fComMapArray)
    {
        ComHist hist;
        hist.detID = com.detID;
        hist.name = Form("com_%d",com.detID);
        hist.channelMult = GetComHistBin(com.detID,"mult").NewH1(Form("%s_channel_mult",hist.name.Data()),Form("%s channel multiplicity;mult;counts",hist.name.Data()));
        hist.hitEnergy = GetComHistBin(com.detID,"hit_energy").NewH1(Form("%s_hit_energy",hist.name.Data()),Form("%s hit energy;energy;counts",hist.name.Data()));
        hist.hitECal = GetComHistBin(com.detID,"hit_ecal").NewH1(Form("%s_hit_ecal",hist.name.Data()),Form("%s hit ECal;ECal;counts",hist.name.Data()));
        hist.channelEnergy = GetComHistBin(com.detID,"channel_energy").NewH1(Form("%s_channel_energy",hist.name.Data()),Form("%s channel energy;energy;counts",hist.name.Data()));
        hist.channelECal = GetComHistBin(com.detID,"channel_ecal").NewH1(Form("%s_channel_ecal",hist.name.Data()),Form("%s channel ECal;ECal;counts",hist.name.Data()));
        for (auto& range : kTimestampDifferenceRanges) {
            const auto type = Form("time_diff_%s",range.name);
            hist.previousEntryTimeDiff.push_back(GetComHistBin(com.detID,type).NewH1(
                Form("%s_previous_entry_time_difference_%s",hist.name.Data(),range.name),
                Form("%s adjacent entry time difference up to %s;|#Deltat| (#mus);counts",hist.name.Data(),range.title)));
        }
        for (auto object : {hist.channelMult, hist.hitEnergy, hist.hitECal, hist.channelEnergy, hist.channelECal})
            object->SetDirectory(nullptr);
        for (auto object : hist.previousEntryTimeDiff)
            object->SetDirectory(nullptr);
        fComHistArray.push_back(hist);
    }
}

void LKCompassRecoTask::MakeTimestampGraphs()
{
    fTimestampBefore40 = new TGraph();
    fTimestampBefore40->SetName("timestamp_before_sort_40");
    fTimestampBefore40->SetTitle("Timestamp before sorting, first 40 entries;Entry;Timestamp");

    fTimestampBefore1000 = new TGraph();
    fTimestampBefore1000->SetName("timestamp_before_sort_1000");
    fTimestampBefore1000->SetTitle("Timestamp before sorting, first 1000 entries;Entry;Timestamp");

    fTimestampAfter40 = new TGraph();
    fTimestampAfter40->SetName("timestamp_after_sort_40");
    fTimestampAfter40->SetTitle("Timestamp after sorting, first 40 entries;Sorted entry;Timestamp");

    fTimestampAfter1000 = new TGraph();
    fTimestampAfter1000->SetName("timestamp_after_sort_1000");
    fTimestampAfter1000->SetTitle("Timestamp after sorting, first 1000 entries;Sorted entry;Timestamp");
}

void LKCompassRecoTask::MakeTimestampDifferenceHistograms()
{
    for (auto& range : kTimestampDifferenceRanges) {
        auto before = new TH1D(
            Form("timestamp_difference_before_sort_%s",range.name),
            Form("Adjacent entry time difference before sorting, up to %s;|#Deltat| (#mus);counts",range.title),
            100,0,range.maximum);
        before->SetDirectory(nullptr);
        fTimestampDifferenceBefore.push_back(before);

        auto after = new TH1D(
            Form("timestamp_difference_after_sort_%s",range.name),
            Form("Adjacent entry time difference after sorting, up to %s;|#Deltat| (#mus);counts",range.title),
            100,0,range.maximum);
        after->SetDirectory(nullptr);
        fTimestampDifferenceAfter.push_back(after);
    }
}

void LKCompassRecoTask::PrefixHistogramTitles()
{
    const TString prefix = Form("[%d] ",fRun->GetRunID());
    auto prefixTitle = [&prefix](TH1* histogram) {
        if (histogram == nullptr)
            return;
        TString title = histogram->GetTitle();
        if (!title.BeginsWith(prefix))
            histogram->SetTitle(prefix+title);
    };

    for (auto& hist : fW1HistArray) {
        for (auto object : {hist.hitPattern, hist.rawChannelEnergy})
            prefixTitle(object);
        for (auto object : {hist.junctionPattern, hist.ohmicPattern,
                            hist.channelMult, hist.hitEnergy, hist.hitECal,
                            hist.xEnergy, hist.yEnergy, hist.xECal, hist.yECal})
            prefixTitle(object);
        for (auto object : hist.previousEntryTimeDiff)
            prefixTitle(object);
        for (auto object : hist.channelEnergy)
            prefixTitle(object);
    }

    for (auto& hist : fComHistArray) {
        for (auto object : {hist.channelMult, hist.hitEnergy, hist.hitECal,
                            hist.channelEnergy, hist.channelECal})
            prefixTitle(object);
        for (auto object : hist.previousEntryTimeDiff)
            prefixTitle(object);
    }

    for (auto object : fTimestampDifferenceBefore)
        prefixTitle(object);
    for (auto object : fTimestampDifferenceAfter)
        prefixTitle(object);
}

void LKCompassRecoTask::FillTimestampGraphs(bool sorted)
{
    auto fill = [this,sorted](TGraph* graph, Long64_t maxEntries) {
        auto graphEntry = Long64_t(graph->GetN());
        Long64_t inputEntry = 0;
        ULong64_t timestamp = 0;
        while (graphEntry < maxEntries
            && fReco->GetTimestampAt(inputEntry, timestamp, sorted)) {
            graph->SetPoint(graphEntry, graphEntry, double(timestamp));
            ++graphEntry;
            ++inputEntry;
        }
    };

    if (sorted) {
        fill(fTimestampAfter40, 40);
        fill(fTimestampAfter1000, 1000);
    }
    else {
        fill(fTimestampBefore40, 40);
        fill(fTimestampBefore1000, 1000);
    }
}

void LKCompassRecoTask::FillTimestampDifferenceHistograms(bool sorted)
{
    auto& histograms = sorted ? fTimestampDifferenceAfter : fTimestampDifferenceBefore;
    Long64_t inputEntry = 0;
    ULong64_t previousTimestamp = 0;
    ULong64_t timestamp = 0;
    bool hasPreviousTimestamp = false;
    while (fReco->GetTimestampAt(inputEntry, timestamp, sorted)) {
        if (hasPreviousTimestamp) {
            const double difference = timestamp >= previousTimestamp
                ? double(timestamp-previousTimestamp)
                : double(previousTimestamp-timestamp);
            for (auto histogram : histograms)
                histogram->Fill(difference*1.e-6);
        }
        previousTimestamp = timestamp;
        hasPreviousTimestamp = true;
        ++inputEntry;
    }
}

void LKCompassRecoTask::AddDrawingGroups()
{
    fTopDrawingGroup = fRun->GetTopDrawingGroup();

    for (auto& hist : fW1HistArray) {
        auto detectorGroup = fTopDrawingGroup->CreateGroup(hist.name);

        auto hitPatternGroup = detectorGroup->CreateGroup("hit_pattern");
        hitPatternGroup->AddHist(hist.hitPattern,"colzstat0text","");
        hitPatternGroup->AddHist(hist.ohmicPattern,"hist","");
        hitPatternGroup->AddHist(hist.junctionPattern,"hist","");
        hitPatternGroup->AddHist(hist.rawChannelEnergy,"colz","");

        auto junctionGroup = detectorGroup->CreateGroup("junction_channels");
        auto ohmicGroup = detectorGroup->CreateGroup("ohmic_channels");
        for (auto strip=0; strip<16; ++strip) {
            junctionGroup->AddHist(hist.channelEnergy.at(strip),"","");
            ohmicGroup->AddHist(hist.channelEnergy.at(16+strip),"","");
        }

        auto energyGroup = detectorGroup->CreateGroup("energy");
        for (auto object : {hist.hitEnergy, hist.hitECal, hist.xEnergy, hist.yEnergy, hist.xECal, hist.yECal})
            energyGroup->AddHist(object,"","");

        auto eventGroup = detectorGroup->CreateGroup("event");
        eventGroup->AddHist(hist.channelMult,"","");

        auto timeDifferenceGroup = detectorGroup->CreateGroup("timestamp_difference");
        for (auto object : hist.previousEntryTimeDiff)
            timeDifferenceGroup->AddHist(object,"","");
    }

    for (auto& hist : fComHistArray) {
        auto detectorGroup = fTopDrawingGroup->CreateGroup(hist.name);

        auto energyGroup = detectorGroup->CreateGroup("energy");
        for (auto object : {hist.channelEnergy, hist.channelECal, hist.hitEnergy, hist.hitECal})
            energyGroup->AddHist(object,"","");

        auto eventGroup = detectorGroup->CreateGroup("event");
        eventGroup->AddHist(hist.channelMult,"","");

        auto timeDifferenceGroup = detectorGroup->CreateGroup("timestamp_difference");
        for (auto object : hist.previousEntryTimeDiff)
            timeDifferenceGroup->AddHist(object,"","");
    }

    auto timestampGroup = fTopDrawingGroup->CreateGroup("timestamp");
    timestampGroup->AddGraph(fTimestampBefore40,"apl","");
    timestampGroup->AddGraph(fTimestampBefore1000,"apl","");
    timestampGroup->AddGraph(fTimestampAfter40,"apl","");
    timestampGroup->AddGraph(fTimestampAfter1000,"apl","");

    auto timestampDifferenceGroup = fTopDrawingGroup->CreateGroup("timestamp_difference");
    auto beforeGroup = timestampDifferenceGroup->CreateGroup("before");
    for (auto object : fTimestampDifferenceBefore)
        beforeGroup->AddHist(object,"","");
    auto afterGroup = timestampDifferenceGroup->CreateGroup("after");
    for (auto object : fTimestampDifferenceAfter)
        afterGroup->AddHist(object,"","");
}

void LKCompassRecoTask::Draw(Option_t* option)
{
    if (fTopDrawingGroup != nullptr)
        fTopDrawingGroup->Draw(option);
}

bool LKCompassRecoTask::Init()
{
    if (!ConfigureParameters() || !ConfigureInputFiles() || !ConfigureMappings())
        return false;

    fW1ChannelArray = fRun->RegisterBranchA("ComW1Channel", "LKComW1Channel", 128, fStoreChannelBranches);
    fComChannelArray = fRun->RegisterBranchA("ComChannel", "LKComChannel", 64, fStoreChannelBranches);
    fW1HitArray = fRun->RegisterBranchA("ComW1Hit", "LKComW1Hit", 32, fStoreHitBranches);
    fComHitArray = fRun->RegisterBranchA("ComHit", "LKComHit", 16, fStoreHitBranches);
    if (fW1ChannelArray == nullptr || fComChannelArray == nullptr
        || fW1HitArray == nullptr || fComHitArray == nullptr)
        return false;

    MakeHistograms();
    MakeTimestampGraphs();
    MakeTimestampDifferenceHistograms();
    PrefixHistogramTitles();
    AddDrawingGroups();
    fReco = new LKCompassReco();
    return true;
}

void LKCompassRecoTask::Run(Long64_t numEvents)
{
    fNumEvents = numEvents;
    fCountEvents = 0;
    fContinueEvent = true;

    for (auto inputFileName : fInputFileArray)
    {
        fPreviousW1EntryTimeMap.clear();
        fPreviousComEntryTimeMap.clear();
        fReco->SetInputFile(inputFileName);
        fReco->SetInputTreeName(fInputTreeName);
        fReco->SetTimeWindow(fTimeWindow);
        fReco->SetEntryRange(fFirstEntry, fLastEntry);
        if (!fReco->Init()) {
            fRun->SignalEndOfRun();
            return;
        }
        FillTimestampGraphs(false);
        FillTimestampDifferenceHistograms(false);
        if (fSortInput) {
            if (!fReco->Sort()) {
                fRun->SignalEndOfRun();
                return;
            }
            FillTimestampGraphs(true);
            FillTimestampDifferenceHistograms(true);
        }

        while (fContinueEvent && fReco->FindEvent()) {
            ProcessEvent();
            SignalNextEvent();
            if (fNumEvents > 0 && fCountEvents >= fNumEvents)
                fContinueEvent = false;
        }

        if (!fContinueEvent)
            break;
    }
}

void LKCompassRecoTask::SignalNextEvent()
{
    ++fCountEvents;
    if (fRun->CheckMute(fCountEvents) == false)
        lk_info << "CoMPASS event " << fCountEvents
                << ", next raw entry " << fReco->GetNextEntry() << endl;
    fContinueEvent = fRun->ExecuteNextEvent();
}

void LKCompassRecoTask::ClearEvent()
{
    fW1ChannelArray->Clear("C");
    fW1HitArray->Clear("C");
    fComChannelArray->Clear("C");
    fComHitArray->Clear("C");
}

void LKCompassRecoTask::ProcessEvent()
{
    ClearEvent();
    AddRawChannels();
    BuildHits();
}

void LKCompassRecoTask::AddRawChannels()
{
    for (auto& raw : fReco->GetRawChannelArray())
    {
        if (raw.energy < fEnergyThreshold)
            continue;

        const W1Map* w1 = nullptr;
        bool side = false;
        int strip = -1;
        if (FindW1(raw.board, raw.channel, w1, side, strip)) {
            for (auto& hist : fW1HistArray) {
                if (hist.detID == w1->detID) {
                    hist.rawChannelEnergy->Fill(raw.channel,raw.energy);
                    break;
                }
            }
            auto index = fW1ChannelArray->GetEntriesFast();
            auto channel = new ((*fW1ChannelArray)[index]) LKComW1Channel();
            channel->Set(w1->detID, side, strip, raw.timestamp,
                         short(raw.energy), short(raw.energyShort),
                         CalibrateW1(w1->detID, raw.energy));
            FillW1TimeDiff(w1->detID, raw.timestamp);
            continue;
        }

        const ComMap* com = nullptr;
        if (FindCom(raw.board, raw.channel, com)) {
            auto index = fComChannelArray->GetEntriesFast();
            auto channel = new ((*fComChannelArray)[index]) LKComChannel();
            channel->Set(com->detID, raw.timestamp,
                         short(raw.energy), short(raw.energyShort),
                         CalibrateCom(com->detID, raw.energy));
            FillComTimeDiff(com->detID, raw.timestamp);
        }
    }
}

void LKCompassRecoTask::FillW1TimeDiff(short detID, ULong64_t time)
{
    auto timeIt = fPreviousW1EntryTimeMap.find(detID);
    if (timeIt != fPreviousW1EntryTimeMap.end()) {
        for (auto& hist : fW1HistArray) {
            if (hist.detID != detID)
                continue;
            const double difference = time >= timeIt->second
                ? double(time-timeIt->second)
                : double(timeIt->second-time);
            for (auto histogram : hist.previousEntryTimeDiff)
                histogram->Fill(difference*1.e-6);
            break;
        }
    }
    fPreviousW1EntryTimeMap[detID] = time;
}

void LKCompassRecoTask::FillComTimeDiff(short detID, ULong64_t time)
{
    auto timeIt = fPreviousComEntryTimeMap.find(detID);
    if (timeIt != fPreviousComEntryTimeMap.end()) {
        for (auto& hist : fComHistArray) {
            if (hist.detID != detID)
                continue;
            const double difference = time >= timeIt->second
                ? double(time-timeIt->second)
                : double(timeIt->second-time);
            for (auto histogram : hist.previousEntryTimeDiff)
                histogram->Fill(difference*1.e-6);
            break;
        }
    }
    fPreviousComEntryTimeMap[detID] = time;
}

bool LKCompassRecoTask::FindW1(UShort_t board, UShort_t channel, const W1Map*& map, bool& side, int& strip) const
{
    for (auto& w1 : fW1MapArray) {
        if (board != w1.board)
            continue;
        const int junctionStrip = int(channel) - w1.junctionStart;
        if (junctionStrip >= 0 && junctionStrip < 16) {
            map = &w1;
            side = false;
            strip = junctionStrip + 1;
            return true;
        }
        const int ohmicStrip = int(channel) - w1.ohmicStart;
        if (ohmicStrip >= 0 && ohmicStrip < 16) {
            map = &w1;
            side = true;
            strip = ohmicStrip + 1;
            return true;
        }
    }
    return false;
}

bool LKCompassRecoTask::FindCom(UShort_t board, UShort_t channel, const ComMap*& map) const
{
    for (auto& com : fComMapArray) {
        if (board == com.board && channel == com.channel) {
            map = &com;
            return true;
        }
    }
    return false;
}

int LKCompassRecoTask::GetW1DisplayY(short detID, int strip) const
{
    auto it = fW1YStartsFromTopMap.find(detID);
    const bool startsFromTop = it == fW1YStartsFromTopMap.end() || it->second;
    return startsFromTop ? 17-strip : strip;
}

int LKCompassRecoTask::GetW1DisplayX(short detID, int strip) const
{
    auto it = fW1XStartsFromLeftMap.find(detID);
    const bool startsFromLeft = it == fW1XStartsFromLeftMap.end() || it->second;
    return startsFromLeft ? strip : 17-strip;
}

LKCompassRecoTask::ECalPar LKCompassRecoTask::GetW1ECalPar(short detID) const
{
    auto it = fW1ECalParMap.find(detID);
    return it == fW1ECalParMap.end() ? ECalPar() : it->second;
}

LKCompassRecoTask::ECalPar LKCompassRecoTask::GetComECalPar(short detID) const
{
    auto it = fComECalParMap.find(detID);
    return it == fComECalParMap.end() ? ECalPar() : it->second;
}

double LKCompassRecoTask::CalibrateW1(short detID, UShort_t energy) const
{
    const auto par = GetW1ECalPar(detID);
    const double value = energy;
    return par.c0 + par.c1*value + par.c2*value*value;
}

double LKCompassRecoTask::CalibrateCom(short detID, UShort_t energy) const
{
    const auto par = GetComECalPar(detID);
    const double value = energy;
    return par.c0 + par.c1*value + par.c2*value*value;
}

void LKCompassRecoTask::BuildHits()
{
    BuildW1Hits();
    BuildComHits();
}

void LKCompassRecoTask::BuildW1Hits()
{
    for (auto& w1 : fW1MapArray)
    {
        short multiplicity = 0;
        short x = -1;
        short y = -1;
        int energy = 0;
        double ecal = 0;
        ULong64_t time = 0;
        W1Hist* hist = nullptr;
        for (auto& candidate : fW1HistArray)
            if (candidate.detID == w1.detID)
                hist = &candidate;

        for (auto i=0; i<fW1ChannelArray->GetEntriesFast(); ++i) {
            auto channel = (LKComW1Channel*) fW1ChannelArray->At(i);
            if (channel->fDetectorID != w1.detID)
                continue;
            ++multiplicity;
            if (time == 0 || channel->fTime < time)
                time = channel->fTime;
            if (channel->fSide) {
                y = channel->fStrip;
                if (hist != nullptr) {
                    hist->ohmicPattern->Fill(y);
                    hist->channelEnergy.at(16+y-1)->Fill(channel->fEnergy);
                    hist->yEnergy->Fill(channel->fEnergy);
                    hist->yECal->Fill(channel->fECal);
                }
            }
            else {
                x = channel->fStrip;
                if (hist != nullptr) {
                    hist->junctionPattern->Fill(x);
                    hist->channelEnergy.at(x-1)->Fill(channel->fEnergy);
                    hist->xEnergy->Fill(channel->fEnergy);
                    hist->xECal->Fill(channel->fECal);
                }
            }
            if (channel->fSide == fW1MainSide) {
                energy += channel->fEnergy;
                ecal += channel->fECal;
            }
        }

        if (multiplicity == 0)
            continue;
        if (hist != nullptr) {
            hist->channelMult->Fill(multiplicity);
            if (x > 0 && y > 0)
                hist->hitPattern->Fill(GetW1DisplayX(w1.detID,x),GetW1DisplayY(w1.detID,y));
            if (energy > 0) {
                hist->hitEnergy->Fill(energy);
                hist->hitECal->Fill(ecal);
            }
        }
        auto index = fW1HitArray->GetEntriesFast();
        auto hit = new ((*fW1HitArray)[index]) LKComW1Hit();
        hit->fDetectorID = w1.detID;
        hit->fChannelMult = multiplicity;
        hit->fTime = time;
        hit->fEnergy = energy > 0 ? short(std::min(energy, 32767)) : -1;
        hit->fECal = energy > 0 ? ecal : -1;
        hit->fX = x;
        hit->fY = y;
    }
}

void LKCompassRecoTask::BuildComHits()
{
    for (auto& com : fComMapArray)
    {
        short multiplicity = 0;
        int energy = 0;
        double ecal = 0;
        ULong64_t time = 0;
        ComHist* hist = nullptr;
        for (auto& candidate : fComHistArray)
            if (candidate.detID == com.detID)
                hist = &candidate;

        for (auto i=0; i<fComChannelArray->GetEntriesFast(); ++i) {
            auto channel = (LKComChannel*) fComChannelArray->At(i);
            if (channel->fDetectorID != com.detID)
                continue;
            ++multiplicity;
            energy += channel->fEnergy;
            ecal += channel->fECal;
            if (hist != nullptr) {
                hist->channelEnergy->Fill(channel->fEnergy);
                hist->channelECal->Fill(channel->fECal);
            }
            if (time == 0 || channel->fTime < time)
                time = channel->fTime;
        }

        if (multiplicity == 0)
            continue;
        if (hist != nullptr) {
            hist->channelMult->Fill(multiplicity);
            if (energy > 0) {
                hist->hitEnergy->Fill(energy);
                hist->hitECal->Fill(ecal);
            }
        }
        auto index = fComHitArray->GetEntriesFast();
        auto hit = new ((*fComHitArray)[index]) LKComHit();
        hit->fDetectorID = com.detID;
        hit->fChannelMult = multiplicity;
        hit->fTime = time;
        hit->fEnergy = energy > 0 ? short(std::min(energy, 32767)) : -1;
        hit->fECal = energy > 0 ? ecal : -1;
    }
}
