#include "LKSiliconMapping.h"

#include "LKLogger.h"
#include "TSystem.h"

#include <fstream>
#include <iostream>
#include <sstream>

ClassImp(LKSiliconMapping)

namespace
{
std::vector<TString> TokenizeColumns(const TString &line)
{
    std::vector<TString> columns;
    std::istringstream stream(line.Data());
    std::string token;
    while (stream >> token)
        columns.push_back(TString(token));
    return columns;
}
}

TString LKSiliconMapping::DetectorInfo::Summary() const
{
    int coboOfDetector = cobo;
    int asadOfDetector = asad;
    if (fMapping != nullptr)
        fMapping -> GetDetectorElectronics(detIndex, coboOfDetector, asadOfDetector);

    TString summary = Form("det_idx=%d det_number=%d type=%s thickness=%g width=%g height=%g"
                           " ring_number=%d ring_type=%s ring_radius=%.4f ring_z=%.4f dEE=%s"
                           " phi_number=%d phi=%.4f phi1=%.4f phi2=%.4f cobo=%d asad=%d"
                           " num_channels=%d",
                           detIndex, detNumber, detType.Data(), detThickness, detWidth, detHeight,
                           ringNumber, ringType.Data(), ringRadius, ringZ, dEE.Data(),
                           phiNumber, phi, phi1, phi2, coboOfDetector, asadOfDetector,
                           fMapping == nullptr ? 0 : fMapping -> GetNumChannelsOfDetector(detIndex));
    if (isLegacy)
        summary += Form(" zapJ=%s zapO=%s mark=%s mark_id=%d fb=%d polar_id=%d",
                        zapJNo.Data(), zapONo.Data(), markNo.Data(), markID, fb, polarID);
    summary += Form(" style=%s", isLegacy ? "legacy" : "new");
    return summary;
}

void LKSiliconMapping::DetectorInfo::Print(Option_t *option) const
{
    TString options(option);
    options.ToLower();

    if (options.Contains("line") || options.Contains("short")) {
        std::cout << Summary() << std::endl;
        return;
    }

    int coboOfDetector = cobo;
    int asadOfDetector = asad;
    if (fMapping != nullptr)
        fMapping -> GetDetectorElectronics(detIndex, coboOfDetector, asadOfDetector);

    printf("== detector det_idx=%d det_number=%d (%s) ====================================\n",
           detIndex, detNumber, detType.Data());
    printf("   det_index      : %d\n",      detIndex);
    printf("   det_number     : %d\n",      detNumber);
    printf("   det_type       : %s\n",      detType.Data());
    printf("   det_thickness  : %g\n",      detThickness);
    printf("   det_width      : %g\n",      detWidth);
    printf("   det_height     : %g\n",      detHeight);
    printf("   ring_number    : %d\n",      ringNumber);
    printf("   ring_type      : %s\n",      ringType.Data());
    printf("   ring_radius    : %.4f\n",    ringRadius);
    printf("   ring_z         : %.4f\n",    ringZ);
    printf("   dEE            : %s\n",      dEE.Data());
    printf("   phi_number     : %d\n",      phiNumber);
    printf("   phi/phi1/phi2  : %.4f / %.4f / %.4f\n", phi, phi1, phi2);
    printf("   cobo, asad     : %d, %d\n",  coboOfDetector, asadOfDetector);
    printf("   mapping style  : %s\n",      isLegacy ? "legacy" : "new");
    if (isLegacy) {
        printf("   zapJ/zapO/mark : %s / %s / %s\n", zapJNo.Data(), zapONo.Data(), markNo.Data());
        printf("   mark_id, fb    : %d, %d\n", markID, fb);
        printf("   polar_id       : %d\n", polarID);
    }

    if (options.Contains("nochannel") || options.Contains("nochan"))
        return;

    if (fMapping == nullptr) {
        printf("   channels       : (detector is not attached to a mapping)\n");
        return;
    }

    const auto &channelIndices = fMapping -> GetChannelVectorIndicesOfDetector(detIndex);
    printf("   channels       : %d\n", (int) channelIndices.size());
    if (channelIndices.empty()) {
        printf("       (no channel mapped to this detector)\n");
        return;
    }

    printf("       %7s %5s %5s %5s %5s %5s %6s\n", "ch_idx", "cobo", "asad", "aget", "chan", "side", "strip");
    for (auto iChannel : channelIndices) {
        auto channel = fMapping -> GetChannelByVectorIndex(iChannel);
        if (channel == nullptr)
            continue;
        printf("       %7d %5d %5d %5d %5d %5d %6d\n",
               channel->channelIndex, channel->cobo, channel->asad, channel->aget, channel->chan,
               channel->side, channel->strip);
    }
}

TString LKSiliconMapping::ChannelInfo::Summary() const
{
    TString summary = Form("ch_idx=%d cobo=%d asad=%d aget=%d chan=%d"
                           " det_idx=%d det_number=%d det_type=%s phi_number=%d side=%d strip=%d",
                           channelIndex, cobo, asad, aget, chan,
                           detIndex, detNumber, detType.Data(), phiNumber, side, strip);
    if (isLegacy)
        summary += Form(" chan2=%d lr=%d det_radius=%.4f det_distance=%.4f",
                        chan2, lr, detRadius, detDistance);
    summary += Form(" style=%s", isLegacy ? "legacy" : "new");
    return summary;
}

const LKSiliconMapping::DetectorInfo *LKSiliconMapping::ChannelInfo::GetDetector() const
{
    if (fMapping == nullptr)
        return nullptr;
    return fMapping -> FindDetectorByIndex(detIndex);
}

void LKSiliconMapping::ChannelInfo::Print(Option_t *option) const
{
    TString options(option);
    options.ToLower();

    std::cout << Summary() << std::endl;

    if (options.Contains("det") || options.Contains("all")) {
        auto detector = GetDetector();
        if (detector != nullptr) {
            std::cout << "    of detector: ";
            detector -> Print("line");
        }
    }
}

LKSiliconMapping::LKSiliconMapping()
    : TNamed("LKSiliconMapping", "")
{
    Clear();
}

LKSiliconMapping::LKSiliconMapping(const LKSiliconMapping &other)
    : TNamed(other)
{
    CopyFrom(other);
}

LKSiliconMapping &LKSiliconMapping::operator=(const LKSiliconMapping &other)
{
    if (this != &other) {
        TNamed::operator=(other);
        CopyFrom(other);
    }
    return *this;
}

void LKSiliconMapping::CopyFrom(const LKSiliconMapping &other)
{
    fDetectorMappingFileName = other.fDetectorMappingFileName;
    fChannelMappingFileName = other.fChannelMappingFileName;
    fDetectorStyleIsNew = other.fDetectorStyleIsNew;
    fChannelStyleIsNew = other.fChannelStyleIsNew;
    fMaxDetectorNumber = other.fMaxDetectorNumber;
    fDetectors = other.fDetectors;
    fChannels = other.fChannels;
    fDetectorVectorIndexByDetIndex = other.fDetectorVectorIndexByDetIndex;
    fDetectorVectorIndexByDetNumber = other.fDetectorVectorIndexByDetNumber;
    fChannelVectorIndexByCAAC = other.fChannelVectorIndexByCAAC;
    fChannelVectorIndicesByDetIndex = other.fChannelVectorIndicesByDetIndex;
    // the copied infos still point at the mapping they were copied from
    RebuildOwnerPointers();
}

void LKSiliconMapping::RebuildOwnerPointers()
{
    for (auto &detector : fDetectors)
        detector.fMapping = this;
    for (auto &channel : fChannels)
        channel.fMapping = this;
}

TString LKSiliconMapping::FindFirstMappingFile(TString mappingPath, TString suffix) const
{
    auto resolvedPath = mappingPath;
    gSystem->ExpandPathName(resolvedPath);
    void *dirHandle = gSystem->OpenDirectory(resolvedPath);
    if (dirHandle == nullptr)
        return "";

    TString found = "";
    while (auto entry = gSystem->GetDirEntry(dirHandle)) {
        TString name(entry);
        if (name == "." || name == "..")
            continue;
        if (name.EndsWith(suffix)) {
            found = TString::Format("%s/%s", resolvedPath.Data(), name.Data());
            break;
        }
    }
    gSystem->FreeDirectory(dirHandle);
    return found;
}

bool LKSiliconMapping::Load(TString mappingPath)
{
    auto resolvedPath = mappingPath;
    gSystem->ExpandPathName(resolvedPath);

    auto detectorFile = TString::Format("%s/detector_mapping.txt", resolvedPath.Data());
    auto channelFile = TString::Format("%s/channel_mapping.txt", resolvedPath.Data());

    if (gSystem->AccessPathName(detectorFile))
        detectorFile = FindFirstMappingFile(mappingPath, "_detector_mapping.txt");
    if (gSystem->AccessPathName(channelFile))
        channelFile = FindFirstMappingFile(mappingPath, "_channel_mapping.txt");

    if (detectorFile.IsNull() || channelFile.IsNull()) {
        lk_error << "Cannot find mapping files in " << mappingPath << std::endl;
        lk_error << "  detector mapping: " << detectorFile << std::endl;
        lk_error << "  channel mapping : " << channelFile << std::endl;
        return false;
    }
    return Load(detectorFile, channelFile);
}

void LKSiliconMapping::Clear(Option_t *)
{
    fDetectorMappingFileName = "";
    fChannelMappingFileName = "";
    fDetectorStyleIsNew = false;
    fChannelStyleIsNew = false;
    fMaxDetectorNumber = -1;
    fDetectors.clear();
    fChannels.clear();
    fDetectorVectorIndexByDetIndex.clear();
    fDetectorVectorIndexByDetNumber.clear();
    fChannelVectorIndexByCAAC.clear();
    fChannelVectorIndicesByDetIndex.clear();
}

bool LKSiliconMapping::Load(TString detectorFileName, TString channelFileName)
{
    Clear();
    if (!LoadDetectorMapping(detectorFileName))
        return false;
    if (!LoadChannelMapping(channelFileName))
        return false;
    return true;
}

bool LKSiliconMapping::LoadDetectorMapping(TString fileName)
{
    fDetectorMappingFileName = fileName;

    std::ifstream input(fileName.Data());
    if (!input.is_open()) {
        lk_error << "Cannot open detector mapping file " << fileName << std::endl;
        return false;
    }

    TString firstLine = "";
    std::string rawLine;
    while (std::getline(input, rawLine)) {
        firstLine = TString(rawLine).Strip(TString::kBoth);
        if (!firstLine.IsNull())
            break;
    }

    input.clear();
    input.seekg(0);

    fDetectorStyleIsNew = firstLine.BeginsWith("detector", TString::kIgnoreCase);
    int lineIndex = 0;
    while (std::getline(input, rawLine)) {
        ++lineIndex;
        TString line(rawLine);
        line = line.Strip(TString::kBoth);
        if (line.IsNull() || line.BeginsWith("#"))
            continue;
        if (fDetectorStyleIsNew && lineIndex <= 2)
            continue;

        auto columns = TokenizeColumns(line);
        DetectorInfo info;
        bool ok = fDetectorStyleIsNew ? ParseNewDetectorRow(columns, info) : ParseLegacyDetectorRow(columns, info);
        if (!ok)
            continue;
        fDetectors.push_back(info);
    }

    RebuildLookupTables();
    RebuildOwnerPointers();
    return true;
}

bool LKSiliconMapping::LoadChannelMapping(TString fileName)
{
    fChannelMappingFileName = fileName;

    std::ifstream input(fileName.Data());
    if (!input.is_open()) {
        lk_error << "Cannot open channel mapping file " << fileName << std::endl;
        return false;
    }

    TString firstLine = "";
    std::string rawLine;
    while (std::getline(input, rawLine)) {
        firstLine = TString(rawLine).Strip(TString::kBoth);
        if (!firstLine.IsNull())
            break;
    }

    input.clear();
    input.seekg(0);

    fChannelStyleIsNew = firstLine.BeginsWith("channel", TString::kIgnoreCase);
    int lineIndex = 0;
    int dataIndex = 0;
    while (std::getline(input, rawLine)) {
        ++lineIndex;
        TString line(rawLine);
        line = line.Strip(TString::kBoth);
        if (line.IsNull() || line.BeginsWith("#"))
            continue;
        if (fChannelStyleIsNew && lineIndex <= 2)
            continue;

        auto columns = TokenizeColumns(line);
        ChannelInfo info;
        bool ok = fChannelStyleIsNew ? ParseNewChannelRow(columns, info) : ParseLegacyChannelRow(columns, info, dataIndex);
        if (!ok)
            continue;

        auto detector = FindDetectorByIndex(info.detIndex);
        if (detector != nullptr) {
            info.detNumber = detector->detNumber;
            if (info.detType.IsNull())
                info.detType = detector->detType;
        }

        fChannelVectorIndexByCAAC[MakeChannelKey(info.cobo, info.asad, info.aget, info.chan)] = fChannels.size();
        fChannels.push_back(info);
        ++dataIndex;
    }

    RebuildDetectorChannelTable();
    RebuildOwnerPointers();
    return true;
}

bool LKSiliconMapping::ParseNewDetectorRow(const std::vector<TString> &columns, DetectorInfo &info) const
{
    if (columns.size() < 15)
        return false;

    info.detType = columns[0];
    info.detIndex = columns[1].Atoi();
    info.detNumber = columns[2].Atoi();
    info.detThickness = columns[3].Atof();
    info.detWidth = columns[4].Atof();
    info.detHeight = columns[5].Atof();
    info.ringNumber = columns[6].Atoi();
    info.ringType = columns[7];
    info.ringRadius = columns[8].Atof();
    info.ringZ = columns[9].Atof();
    info.dEE = columns[10];
    info.phiNumber = columns[11].Atoi();
    info.phi = columns[12].Atof();
    info.phi1 = columns[13].Atof();
    info.phi2 = columns[14].Atof();
    info.isLegacy = false;
    return true;
}

bool LKSiliconMapping::ParseLegacyDetectorRow(const std::vector<TString> &columns, DetectorInfo &info) const
{
    if (columns.size() < 16)
        return false;

    info.detType = columns[0];
    info.detIndex = columns[1].Atoi();
    info.detNumber = info.detIndex;
    info.cobo = columns[2].Atoi();
    info.asad = columns[3].Atoi();
    info.zapJNo = columns[4];
    info.zapONo = columns[5];
    info.markNo = columns[6];
    info.markID = columns[7].Atoi();
    info.fb = columns[8].Atoi();
    info.ringNumber = columns[9].Atoi();
    info.dEE = columns[10] == "1" ? "E" : "dE";
    info.polarID = columns[11].Atoi();
    info.ringRadius = 5. * columns[12].Atof();
    info.ringZ = 10. * columns[13].Atof();
    info.phi = columns[14].Atof();
    info.phiNumber = columns[15].Atoi();
    info.ringType = Form("%d%s", info.ringNumber, info.dEE.Data());
    info.isLegacy = true;
    return true;
}

bool LKSiliconMapping::ParseNewChannelRow(const std::vector<TString> &columns, ChannelInfo &info) const
{
    if (columns.size() < 9)
        return false;

    info.channelIndex = columns[0].Atoi();
    info.cobo = columns[1].Atoi();
    info.asad = columns[2].Atoi();
    info.aget = columns[3].Atoi();
    info.chan = columns[4].Atoi();
    info.detIndex = columns[5].Atoi();
    info.phiNumber = columns[6].Atoi();
    info.side = columns[7].Atoi();
    info.strip = columns[8].Atoi();
    info.isLegacy = false;
    return true;
}

bool LKSiliconMapping::ParseLegacyChannelRow(const std::vector<TString> &columns, ChannelInfo &info, int rowIndex) const
{
    if (columns.size() < 12)
        return false;

    info.channelIndex = rowIndex;
    info.cobo = columns[0].Atoi();
    info.asad = columns[1].Atoi();
    info.aget = columns[2].Atoi();
    info.chan = columns[3].Atoi();
    info.chan2 = columns[4].Atoi();
    info.detType = columns[5];
    info.detIndex = columns[6].Atoi();
    info.detNumber = info.detIndex;
    info.side = columns[7].Atoi();
    info.strip = columns[8].Atoi();
    info.lr = columns[9].Atoi();
    info.detRadius = columns[10].Atof();
    info.detDistance = columns[11].Atof();
    info.phiNumber = -1;
    info.isLegacy = true;
    return true;
}

void LKSiliconMapping::RebuildLookupTables()
{
    int maxDetIndex = -1;
    fMaxDetectorNumber = -1;
    for (const auto &detector : fDetectors) {
        if (maxDetIndex < detector.detIndex)
            maxDetIndex = detector.detIndex;
        if (fMaxDetectorNumber < detector.detNumber)
            fMaxDetectorNumber = detector.detNumber;
    }

    fDetectorVectorIndexByDetIndex.assign(maxDetIndex + 1, -1);
    fDetectorVectorIndexByDetNumber.assign(fMaxDetectorNumber + 1, -1);
    for (size_t iDetector = 0; iDetector < fDetectors.size(); ++iDetector) {
        auto detIndex = fDetectors[iDetector].detIndex;
        auto detNumber = fDetectors[iDetector].detNumber;
        if (detIndex >= 0)
            fDetectorVectorIndexByDetIndex[detIndex] = iDetector;
        if (detNumber >= 0)
            fDetectorVectorIndexByDetNumber[detNumber] = iDetector;
    }
}

void LKSiliconMapping::RebuildDetectorChannelTable()
{
    fChannelVectorIndicesByDetIndex.assign(fDetectorVectorIndexByDetIndex.size(), std::vector<int>());
    for (size_t iChannel = 0; iChannel < fChannels.size(); ++iChannel) {
        auto detIndex = fChannels[iChannel].detIndex;
        if (detIndex < 0 || detIndex >= (int) fChannelVectorIndicesByDetIndex.size())
            continue;
        fChannelVectorIndicesByDetIndex[detIndex].push_back(iChannel);
    }
}

bool LKSiliconMapping::HasLegacyDetector() const
{
    for (const auto &detector : fDetectors)
        if (detector.isLegacy)
            return true;
    return false;
}

bool LKSiliconMapping::HasLegacyChannel() const
{
    for (const auto &channel : fChannels)
        if (channel.isLegacy)
            return true;
    return false;
}

const std::vector<int> &LKSiliconMapping::GetChannelVectorIndicesOfDetector(int detIndex) const
{
    if (detIndex < 0 || detIndex >= (int) fChannelVectorIndicesByDetIndex.size())
        return fEmptyChannelIndices;
    return fChannelVectorIndicesByDetIndex[detIndex];
}

int LKSiliconMapping::GetNumChannelsOfDetector(int detIndex) const
{
    return GetChannelVectorIndicesOfDetector(detIndex).size();
}

bool LKSiliconMapping::GetDetectorElectronics(int detIndex, int &cobo, int &asad) const
{
    cobo = -1;
    asad = -1;

    auto detector = FindDetectorByIndex(detIndex);
    if (detector != nullptr && detector->cobo >= 0) {
        cobo = detector->cobo;
        asad = detector->asad;
        return true;
    }

    // new-style detector mapping does not carry the electronics id, take it from the channels
    for (auto iChannel : GetChannelVectorIndicesOfDetector(detIndex)) {
        auto channel = GetChannelByVectorIndex(iChannel);
        if (channel == nullptr || channel->cobo < 0)
            continue;
        cobo = channel->cobo;
        asad = channel->asad;
        return true;
    }
    return false;
}

Long64_t LKSiliconMapping::MakeChannelKey(int cobo, int asad, int aget, int chan) const
{
    return (((Long64_t)cobo) << 24) | (((Long64_t)asad) << 16) | (((Long64_t)aget) << 8) | (Long64_t)chan;
}

const LKSiliconMapping::DetectorInfo *LKSiliconMapping::GetDetectorByVectorIndex(int index) const
{
    if (index < 0 || index >= (int) fDetectors.size())
        return nullptr;
    return &fDetectors[index];
}

const LKSiliconMapping::DetectorInfo *LKSiliconMapping::FindDetectorByIndex(int detIndex) const
{
    if (detIndex < 0 || detIndex >= (int) fDetectorVectorIndexByDetIndex.size())
        return nullptr;
    auto vectorIndex = fDetectorVectorIndexByDetIndex[detIndex];
    return GetDetectorByVectorIndex(vectorIndex);
}

const LKSiliconMapping::DetectorInfo *LKSiliconMapping::FindDetectorByNumber(int detNumber) const
{
    if (detNumber < 0 || detNumber >= (int) fDetectorVectorIndexByDetNumber.size())
        return nullptr;
    auto vectorIndex = fDetectorVectorIndexByDetNumber[detNumber];
    return GetDetectorByVectorIndex(vectorIndex);
}

const LKSiliconMapping::ChannelInfo *LKSiliconMapping::GetChannelByVectorIndex(int index) const
{
    if (index < 0 || index >= (int) fChannels.size())
        return nullptr;
    return &fChannels[index];
}

int LKSiliconMapping::FindChannelIndex(int cobo, int asad, int aget, int chan) const
{
    auto iterator = fChannelVectorIndexByCAAC.find(MakeChannelKey(cobo, asad, aget, chan));
    if (iterator == fChannelVectorIndexByCAAC.end())
        return -1;
    return iterator->second;
}

const LKSiliconMapping::ChannelInfo *LKSiliconMapping::FindChannel(int cobo, int asad, int aget, int chan) const
{
    return GetChannelByVectorIndex(FindChannelIndex(cobo, asad, aget, chan));
}

int LKSiliconMapping::FindDetectorIndex(int cobo, int asad, int aget, int chan) const
{
    auto channel = FindChannel(cobo, asad, aget, chan);
    if (channel == nullptr)
        return -1;
    return channel->detIndex;
}

int LKSiliconMapping::FindDetectorNumber(int cobo, int asad, int aget, int chan) const
{
    auto channel = FindChannel(cobo, asad, aget, chan);
    if (channel == nullptr)
        return -1;
    return channel->detNumber;
}

void LKSiliconMapping::Print(Option_t *option) const
{
    TString options(option);
    options.ToLower();

    bool printSummary  = options.Contains("summary");
    bool printDetector = options.Contains("detector") || options.Contains("det");
    bool printChannel  = options.Contains("channel")  || options.Contains("chan") || options.Contains("ch");
    bool printGroup    = options.Contains("group");
    bool printAll      = options.Contains("all");

    if (options.IsNull() || printAll) {
        printSummary = true;
        printDetector = true;
        printChannel = true;
    }
    if (printGroup) {
        printDetector = false;
        printChannel = false;
    }

    if (printSummary)
        PrintSummary();
    if (printDetector) {
        std::cout << std::endl;
        PrintDetectors(-1);
    }
    if (printChannel) {
        std::cout << std::endl;
        PrintChannels(-1);
    }
    if (printGroup) {
        std::cout << std::endl;
        PrintDetectorChannels(-1);
    }
}

void LKSiliconMapping::PrintSummary() const
{
    std::cout << "== LKSiliconMapping ==========================================================" << std::endl;
    std::cout << "  detector mapping file : " << fDetectorMappingFileName << std::endl;
    std::cout << "  channel  mapping file : " << fChannelMappingFileName << std::endl;
    std::cout << "  detector style        : " << (fDetectorStyleIsNew ? "new" : "legacy") << std::endl;
    std::cout << "  channel  style        : " << (fChannelStyleIsNew ? "new" : "legacy") << std::endl;
    std::cout << "  number of detectors   : " << fDetectors.size() << std::endl;
    std::cout << "  number of channels    : " << fChannels.size() << std::endl;
    std::cout << "  max detector number   : " << fMaxDetectorNumber << std::endl;
}

void LKSiliconMapping::PrintDetectors(int numPrint) const
{
    if (numPrint < 0 || numPrint > (int) fDetectors.size())
        numPrint = fDetectors.size();

    bool legacy = HasLegacyDetector();

    std::cout << "== detectors (" << numPrint << "/" << fDetectors.size() << ") =================================================" << std::endl;
    printf("%4s %8s %8s %6s %10s %8s %8s %5s %6s %10s %10s %4s %5s %10s %10s %10s %5s %5s %5s",
           "i", "det_idx", "det_num", "type", "thickness", "width", "height",
           "ring#", "ringT", "ring_r", "ring_z", "dEE", "phi#", "phi", "phi1", "phi2",
           "cobo", "asad", "nch");
    if (legacy)
        printf(" %6s %6s %6s %6s %4s %6s", "zapJ", "zapO", "mark", "markID", "fb", "polarID");
    printf("\n");

    for (auto iDetector = 0; iDetector < numPrint; ++iDetector) {
        auto detector = GetDetectorByVectorIndex(iDetector);
        if (detector == nullptr)
            continue;

        int cobo = -1;
        int asad = -1;
        GetDetectorElectronics(detector->detIndex, cobo, asad);

        printf("%4d %8d %8d %6s %10.4f %8.4f %8.4f %5d %6s %10.4f %10.4f %4s %5d %10.4f %10.4f %10.4f %5d %5d %5d",
               iDetector, detector->detIndex, detector->detNumber, detector->detType.Data(),
               detector->detThickness, detector->detWidth, detector->detHeight,
               detector->ringNumber, detector->ringType.Data(), detector->ringRadius, detector->ringZ,
               detector->dEE.Data(), detector->phiNumber, detector->phi, detector->phi1, detector->phi2,
               cobo, asad, GetNumChannelsOfDetector(detector->detIndex));
        if (legacy)
            printf(" %6s %6s %6s %6d %4d %6d",
                   detector->zapJNo.Data(), detector->zapONo.Data(), detector->markNo.Data(),
                   detector->markID, detector->fb, detector->polarID);
        printf("\n");
    }
}

void LKSiliconMapping::PrintChannels(int numPrint) const
{
    if (numPrint < 0 || numPrint > (int) fChannels.size())
        numPrint = fChannels.size();

    bool legacy = HasLegacyChannel();

    std::cout << "== channels (" << numPrint << "/" << fChannels.size() << ") ==================================================" << std::endl;
    printf("%5s %7s %5s %5s %5s %5s %8s %8s %6s %5s %5s %6s",
           "i", "ch_idx", "cobo", "asad", "aget", "chan",
           "det_idx", "det_num", "type", "phi#", "side", "strip");
    if (legacy)
        printf(" %6s %4s %10s %10s", "chan2", "lr", "det_r", "det_dist");
    printf("\n");

    for (auto iChannel = 0; iChannel < numPrint; ++iChannel) {
        auto channel = GetChannelByVectorIndex(iChannel);
        if (channel == nullptr)
            continue;
        printf("%5d %7d %5d %5d %5d %5d %8d %8d %6s %5d %5d %6d",
               iChannel, channel->channelIndex, channel->cobo, channel->asad, channel->aget, channel->chan,
               channel->detIndex, channel->detNumber, channel->detType.Data(),
               channel->phiNumber, channel->side, channel->strip);
        if (legacy)
            printf(" %6d %4d %10.4f %10.4f", channel->chan2, channel->lr, channel->detRadius, channel->detDistance);
        printf("\n");
    }
}

void LKSiliconMapping::PrintDetector(int detIndex) const
{
    auto detector = FindDetectorByIndex(detIndex);
    if (detector == nullptr) {
        std::cout << "no detector with det_idx=" << detIndex << std::endl;
        return;
    }
    detector -> Print();
}

void LKSiliconMapping::PrintDetectorChannels(int detIndex) const
{
    if (detIndex >= 0) {
        PrintDetector(detIndex);
        return;
    }

    std::cout << "== channels grouped by detector ==============================================" << std::endl;
    for (auto iDetector = 0; iDetector < (int) fDetectors.size(); ++iDetector) {
        auto detector = GetDetectorByVectorIndex(iDetector);
        if (detector == nullptr)
            continue;
        detector -> Print();
    }
}

void LKSiliconMapping::PrintChannelLookup(int cobo, int asad, int aget, int chan) const
{
    std::cout << "# Lookup by CAAC" << std::endl;
    std::cout << "query: (" << cobo << "," << asad << "," << aget << "," << chan << ")" << std::endl;

    auto channel = FindChannel(cobo, asad, aget, chan);
    if (channel == nullptr) {
        std::cout << "channel not found" << std::endl;
        return;
    }

    std::cout << "query result ";
    channel->Print();

    auto detector = FindDetectorByIndex(channel->detIndex);
    if (detector != nullptr) {
        std::cout << "matched detector ";
        detector->Print("line");
    }
}

void LKSiliconMapping::PrintTest(int queryCobo, int queryAsad, int queryAget, int queryChan, int numPrintDetectors, int numPrintChannels) const
{
    PrintSummary();
    std::cout << std::endl;
    PrintDetectors(numPrintDetectors);
    std::cout << std::endl;
    PrintChannels(numPrintChannels);
    std::cout << std::endl;
    PrintChannelLookup(queryCobo, queryAsad, queryAget, queryChan);
}
