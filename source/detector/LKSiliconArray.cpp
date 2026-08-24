#include "LKCompiled.h"
#include "LKSiliconArray.h"
#include "LKSiDetector.h"
#include "LKSiChannel.h"
#include "GETChannel.h"

#include <cfloat>
#include <climits>

ClassImp(LKSiliconArray)

namespace
{
TString NormalizeDetTypeName(TString detType)
{
    detType.ToLower();
    return detType;
}

void ConfigureDetectorType(
        TString detType,
        int &numJunctionStrips,
        int &numOhmicStrips,
        bool &useJunctionLR,
        bool &useOhmicLR)
{
    detType.ToLower();

    if (detType == "x6") {
        numJunctionStrips = 8;
        numOhmicStrips = 4;
        useJunctionLR = true;
        useOhmicLR = false;
        return;
    }

    if (detType == "qqq5") {
        numJunctionStrips = 32;
        numOhmicStrips = 4;
        useJunctionLR = false;
        useOhmicLR = false;
        return;
    }

    if (detType == "bb10") {
        numJunctionStrips = 8;
        numOhmicStrips = 1;
        useJunctionLR = false;
        useOhmicLR = false;
        return;
    }

    if (detType == "csd") {
        numJunctionStrips = 8;
        numOhmicStrips = 1;
        useJunctionLR = false;
        useOhmicLR = false;
        return;
    }

    numJunctionStrips = 8;
    numOhmicStrips = 1;
    useJunctionLR = false;
    useOhmicLR = false;
}
}

LKSiliconArray::LKSiliconArray()
    : LKSiliconArray("LKSiliconArray", "Silicon array plane using LKSiliconMapping")
{
}

LKSiliconArray::LKSiliconArray(const char *name, const char *title)
    : LKEvePlane(name, title)
{
    fDetectorArray = new TObjArray();
    fChannelArray = new TObjArray();
    fGSelJOSideDisplay = new TGraph();
    fUserDrawingArray = new TObjArray();

    fPaletteNumber = 1;
    fCtrlBinTextSize = 8.0;
    fCtrlLabelSize = 0.25;
}

bool LKSiliconArray::Init()
{
    fName = "LKSiliconArray";
    fDetName = "si_array";

    LKEvePlane::Init();

    fUserDrawingArrayIndex = new int***[4];
    for (int i=0; i<4; ++i) {
        fUserDrawingArrayIndex[i] = new int**[10];
        for (int j=0; j<10; ++j) {
            fUserDrawingArrayIndex[i][j] = new int*[fMaxDetectors];
            for (int k=0; k<fMaxDetectors; ++k) {
                fUserDrawingArrayIndex[i][j][k] = new int[2];
                for (int l=0; l<2; ++l)
                    fUserDrawingArrayIndex[i][j][k][l] = -1;
            }
        }
    }

    fUserDrawingLeastNDraw = new int***[4];
    for (int i=0; i<4; ++i) {
        fUserDrawingLeastNDraw[i] = new int**[10];
        for (int j=0; j<10; ++j) {
            fUserDrawingLeastNDraw[i][j] = new int*[fMaxDetectors];
            for (int k=0; k<fMaxDetectors; ++k) {
                fUserDrawingLeastNDraw[i][j][k] = new int[2];
                for (int l=0; l<2; ++l)
                    fUserDrawingLeastNDraw[i][j][k][l] = -1;
            }
        }
    }

    for (int i=0; i<4; ++i)
        for (int j=0; j<10; ++j) {
            fUserDrawingName[i][j] = "";
            fUserDrawingType[i][j] = -1;
        }

    fMapCAACToChannelIndex = new int***[fNumCobo];
    for (int i=0; i<fNumCobo; ++i) {
        fMapCAACToChannelIndex[i] = new int**[fNumAsad];
        for (int j=0; j<fNumAsad; ++j) {
            fMapCAACToChannelIndex[i][j] = new int*[fNumAget];
            for (int k=0; k<fNumAget; ++k) {
                fMapCAACToChannelIndex[i][j][k] = new int[fNumChan];
                for (int l=0; l<fNumChan; ++l)
                    fMapCAACToChannelIndex[i][j][k][l] = -1;
            }
        }
    }

    fMapCAACToDetectorIndex = new int***[fNumCobo];
    for (int i=0; i<fNumCobo; ++i) {
        fMapCAACToDetectorIndex[i] = new int**[fNumAsad];
        for (int j=0; j<fNumAsad; ++j) {
            fMapCAACToDetectorIndex[i][j] = new int*[fNumAget];
            for (int k=0; k<fNumAget; ++k) {
                fMapCAACToDetectorIndex[i][j][k] = new int[fNumChan];
                for (int l=0; l<fNumChan; ++l)
                    fMapCAACToDetectorIndex[i][j][k][l] = -1;
            }
        }
    }

    TString mappingPath = "";
    fPar -> Require(fDetName+"/MappingPath","","name of silicon mapping directory or detector mapping file","t");
    fPar -> Require(fDetName+"/Mapping1","","name of detector mapping file","t");
    fPar -> Require(fDetName+"/Mapping2","","name of channel mapping file","t");
    fPar -> UpdatePar(mappingPath, fDetName+"/MappingPath");
    fPar -> UpdatePar(fDetectorParName, fDetName+"/Mapping1");
    fPar -> UpdatePar(fMappingFileName, fDetName+"/Mapping2");

    auto forceMappingParName = fDetName + "/ForceMapping";
    fPar -> Require(forceMappingParName,"","","t/");
    //if (fPar -> CheckPar(forceMappingParName) == false && fDetName != "stark" && fPar -> CheckPar("stark/ForceMapping"))
        //forceMappingParName = "stark/ForceMapping";
    if (fPar -> CheckPar(forceMappingParName)) {
        auto nForce = fPar -> GetParN(forceMappingParName);
        if (nForce >= 2) {
            fDetectorParName = fPar -> GetParString(forceMappingParName,0);
            fMappingFileName = fPar -> GetParString(forceMappingParName,1);
            mappingPath = "";
        }
        else if (nForce >= 1) {
            mappingPath = fPar -> GetParString(forceMappingParName,0);
            fDetectorParName = "";
            fMappingFileName = "";
        }
        lk_note << "YOU FORCED MAPPING FILES BY USING PARAMETER " << forceMappingParName << endl;
        if (mappingPath.IsNull() == false)
            lk_note << mappingPath << endl;
        else {
            lk_note << fDetectorParName << endl;
            lk_note << fMappingFileName << endl;
        }
    }

    if (mappingPath.IsNull() && (fDetectorParName.IsNull() || fMappingFileName.IsNull())) {
        fDetectorParName = TString(LILAK_PATH) + "/common/starkmap_input.RAON2024_40Arp.txt";
        fMappingFileName = TString(LILAK_PATH) + "/common/starkmap_output.RAON2024_40Arp.txt";
        lk_warning << "Mapping is not set. Using default files:" << endl;
        lk_warning << "  detector: " << fDetectorParName << endl;
        lk_warning << "  channel : " << fMappingFileName << endl;
    }

    bool loaded = false;
    if (mappingPath.IsNull() == false) {
        loaded = fSiliconMapping.Load(mappingPath);
        if (loaded)
            lk_info << "mapping path: " << mappingPath << endl;
    }
    else {
        loaded = fSiliconMapping.Load(fDetectorParName, fMappingFileName);
        if (loaded) {
            lk_info << "detector parameter: " << fDetectorParName << endl;
            lk_info << "channel mapping: " << fMappingFileName << endl;
        }
    }

    if (!loaded) {
        lk_error << "Failed to load silicon mapping." << endl;
        return false;
    }

    fMaxLayerIndex = -INT_MAX;
    fMinPhi = DBL_MAX;
    fMaxPhi = -DBL_MAX;
    ResetEPairMapping();

    for (int iDetector = 0; iDetector < fSiliconMapping.GetNumDetectors(); ++iDetector)
    {
        auto detectorInfo = fSiliconMapping.GetDetectorByVectorIndex(iDetector);
        if (detectorInfo == nullptr)
            continue;

        auto detTypeName = NormalizeDetTypeName(detectorInfo->detType);
        if (fDetectorTypeArray.CheckPar(detTypeName) == false)
            fDetectorTypeArray.AddPar(detTypeName, 1);
        int detTypeIndex = fDetectorTypeArray.GetParIndex(detTypeName);

        int numJunctionStrips = 8;
        int numOhmicStrips = 1;
        bool useJunctionLR = false;
        bool useOhmicLR = false;
        ConfigureDetectorType(detTypeName, numJunctionStrips, numOhmicStrips, useJunctionLR, useOhmicLR);

        double detWidth = detectorInfo->detWidth > 0 ? detectorInfo->detWidth : 40.3;
        double detHeight = detectorInfo->detHeight > 0 ? detectorInfo->detHeight : 75.0;
        double detRadius = detectorInfo->ringRadius;
        double detDistance = detectorInfo->ringZ;
        double phi1 = detectorInfo->phi1;
        double phi2 = detectorInfo->phi2;
        double phi0 = detectorInfo->phi;
        double tta1 = 0;
        double tta2 = 0;
        if (detDistance != 0 || detRadius != 0) {
            tta1 = TMath::ATan2(detRadius, detDistance-0.5*detHeight) * TMath::RadToDeg();
            tta2 = TMath::ATan2(detRadius, detDistance+0.5*detHeight) * TMath::RadToDeg();
        }

        auto siDetector = new LKSiDetector();
        int detIndex = detectorInfo->detIndex >= 0 ? detectorInfo->detIndex : iDetector;
        int detNumber = detectorInfo->detNumber;
        siDetector -> SetSiType(detTypeName, detTypeIndex, detIndex, detNumber, 2, numJunctionStrips, numOhmicStrips, useJunctionLR, useOhmicLR);
        int layer = detectorInfo->ringNumber;
        int row = detectorInfo->phiNumber;
        TVector3 position(detRadius,0,0);
        position.SetPhi(phi0);
        position.SetZ(detDistance);
        siDetector -> SetSiPosition(position, layer, row, phi1, phi2, tta1, tta2);
        siDetector -> SetWidth(detWidth);
        siDetector -> SetHeight(detHeight);

        bool isDE = detectorInfo->dEE.EqualTo("dE", TString::kIgnoreCase);
        if (isDE) siDetector -> SetIsdEDetector();
        else      siDetector -> SetIsEDetector();

        if (detectorInfo->ringType.Contains("12") && row >= 0)
            SetEPairMapping(row, isDE ? 1 : 0, detIndex);

        double layer1 = fLayerYScale * (layer + fLayerYOffset);
        double layer2 = fLayerYScale * (layer + 1 - fLayerYOffset);
        TString name = Form("hist_%s_%d_junction",detTypeName.Data(),detNumber);
        TString title = Form("%s(%d) Junction (%d,%d)",detTypeName.Data(),detNumber,numJunctionStrips,(useJunctionLR?2:1));
        double phiCenter = 0.5*(phi1+phi2);
        double phi1User = phiCenter - abs(phiCenter-phi1);
        double phi2User = phiCenter + abs(phiCenter-phi2);
        siDetector -> CreateHistJunction(name, title, phi1User, phi2User, layer1, layer2, "!axis");
        name = Form("hist_%s_%d_ohmic",detTypeName.Data(),detNumber);
        title = Form("%s(%d) Ohmic (%d,%d)",detTypeName.Data(),detNumber,(useOhmicLR?2:1),numOhmicStrips);
        siDetector -> CreateHistOhmic(name, title, phi1User, phi2User, layer1, layer2, "!axis");
        fDetectorArray -> Add(siDetector);

        auto histJ = siDetector -> GetHistJunction();
        auto histO = siDetector -> GetHistOhmic();
        histJ -> SetMarkerSize(fCtrlBinTextSize*0.3);
        histO -> SetMarkerSize(fCtrlBinTextSize*0.3);
        if (fMaxLayerIndex < layer) fMaxLayerIndex = layer;
        if (fMinPhi > phi1) fMinPhi = phi1;
        if (fMaxPhi < phi2) fMaxPhi = phi2;
    }

    int globalChannelIndex = 0;
    for (int iChannel = 0; iChannel < fSiliconMapping.GetNumChannels(); ++iChannel)
    {
        auto channelInfo = fSiliconMapping.GetChannelByVectorIndex(iChannel);
        if (channelInfo == nullptr)
            continue;

        int cobo = channelInfo->cobo;
        int asad = channelInfo->asad;
        int aget = channelInfo->aget;
        int chan = channelInfo->chan;
        int detIndex = channelInfo->detIndex;

        if (cobo < 0 || cobo >= fNumCobo || asad < 0 || asad >= fNumAsad || aget < 0 || aget >= fNumAget || chan < 0 || chan >= fNumChan)
            continue;
        if (detIndex < 0 || detIndex >= fDetectorArray->GetEntriesFast())
            continue;

        auto detectorInfo = fSiliconMapping.FindDetectorByIndex(detIndex);
        if (detectorInfo == nullptr)
            continue;

        auto detTypeName = NormalizeDetTypeName(detectorInfo->detType);
        int numJunctionStrips = 8;
        int numOhmicStrips = 1;
        bool useJunctionLR = false;
        bool useOhmicLR = false;
        ConfigureDetectorType(detTypeName, numJunctionStrips, numOhmicStrips, useJunctionLR, useOhmicLR);

        int side = (channelInfo->side == 1 ? 0 : 1);
        int strip = channelInfo->strip - 1;
        int lr = 0;
        int localID = strip;
        if (side == 0 && useJunctionLR) {
            localID = strip;
            lr = strip % 2;
            strip = strip / 2;
        }

        fMapCAACToChannelIndex[cobo][asad][aget][chan] = globalChannelIndex;
        fMapCAACToDetectorIndex[cobo][asad][aget][chan] = detIndex;

        int detTypeIndex = fDetectorTypeArray.GetParIndex(detTypeName);
        auto siChannel = new LKSiChannel();
        fChannelArray -> Add(siChannel);
        siChannel -> SetCobo(cobo);
        siChannel -> SetAsad(asad);
        siChannel -> SetAget(aget);
        siChannel -> SetChan(chan);
        siChannel -> SetDetType(detTypeIndex);
        siChannel -> SetDetID(detIndex);
        siChannel -> SetDetNum(detectorInfo->detNumber);
        siChannel -> SetChannelID(globalChannelIndex);
        siChannel -> SetLocalID(localID);
        siChannel -> SetSide(side);
        siChannel -> SetStrip(strip);
        siChannel -> SetDirection(lr);
        if (detTypeName=="x6" && side==0 && useJunctionLR) siChannel -> SetIsPairedChannel();
        else                                                siChannel -> SetIsStandaloneChannel();

        auto siDetector = (LKSiDetector*) fDetectorArray -> At(detIndex);
        siDetector -> RegisterChannel(siChannel);

        TVector3 position0 = siDetector -> GetPosition();
        TVector3 position1 = siDetector -> GetPosition();
        TVector3 position2 = siDetector -> GetPosition();
        double phi = siDetector -> GetPhi0();
        double theta = siDetector -> GetTheta0();
        double width = siDetector -> GetHeight();
        double height = siDetector -> GetWidth();
        if (side==0) {
            auto numStrips = siDetector -> GetNumJunctionStrips();
            position0.SetPhi(0);
            position0.SetY(0.5*width-(localID+0.5)*(width/(useJunctionLR ? numStrips*2 : numStrips))); position0.RotateZ(phi);
            position1.SetY(0.5*width-(localID    )*(width/(useJunctionLR ? numStrips*2 : numStrips))); position1.RotateZ(phi);
            position2.SetY(0.5*width-(localID+1  )*(width/(useJunctionLR ? numStrips*2 : numStrips))); position2.RotateZ(phi);
        }
        if (side==1) {
            auto numStrips = siDetector -> GetNumOhmicStrips();
            position0.SetZ(position0.Z()+0.5*height-(localID+0.5)*(height/numStrips));
            position1.SetZ(position1.Z()+0.5*height-(localID    )*(height/numStrips));
            position2.SetZ(position2.Z()+0.5*height-(localID+1  )*(height/numStrips));
        }
        siChannel -> SetPosition(position0);
        siChannel -> SetPhi1(position1.Phi());
        siChannel -> SetPhi2(position2.Phi());
        siChannel -> SetTheta1(theta);
        siChannel -> SetTheta2(theta);

        ++globalChannelIndex;
    }

    lk_info << globalChannelIndex << " channels are mapped!" << endl;

    return true;
}

void LKSiliconArray::ResetEPairMapping()
{
    for (auto i=0; i<40; ++i)
        for (auto j=0; j<2; ++j)
            fdEEPairMapping[i][j] = -1;
}

void LKSiliconArray::SetEPairMapping(int pair, int i, int detID)
{
    if (pair < 0 || pair >= 40)
        return;
    if (i < 0 || i >= 2)
        return;
    fdEEPairMapping[pair][i] = detID;
}

int LKSiliconArray::FindPadID(int cobo, int asad, int aget, int chan)
{
    return fMapCAACToChannelIndex[cobo][asad][aget][chan];
}

LKSiChannel* LKSiliconArray::GetSiChannel(int idx)
{
    TObject *channel = nullptr;
    if (idx >= 0 && idx < fChannelArray -> GetEntriesFast())
        channel = fChannelArray -> At(idx);
    return (LKSiChannel *) channel;
}

LKSiChannel* LKSiliconArray::GetSiChannel(int cobo, int asad, int aget, int chan)
{
    auto id = FindSiChannelID(cobo, asad, aget, chan);
    if (id < 0)
        return (LKSiChannel*) nullptr;
    return GetSiChannel(id);
}

bool LKSiliconArray::SetSiChannelData(LKSiChannel* siChannel, GETChannel* channel)
{
    auto cobo = channel -> GetCobo();
    auto asad = channel -> GetAsad();
    auto aget = channel -> GetAget();
    auto chan = channel -> GetChan();
    auto dummyChannel = GetSiChannel(cobo, asad, aget, chan);
    if (dummyChannel == nullptr)
        return false;

    siChannel -> SetCobo(cobo);
    siChannel -> SetAsad(asad);
    siChannel -> SetAget(aget);
    siChannel -> SetChan(chan);
    siChannel -> SetDetType(dummyChannel -> GetDetType());
    siChannel -> SetDetID(dummyChannel -> GetDetID());
    siChannel -> SetDetNum(dummyChannel -> GetDetNum());
    siChannel -> SetChannelID(dummyChannel -> GetChannelID());
    siChannel -> SetLocalID(dummyChannel -> GetLocalID());
    siChannel -> SetSide(dummyChannel -> GetSide());
    siChannel -> SetStrip(dummyChannel -> GetStrip());
    siChannel -> SetDirection(dummyChannel -> GetDirection());
    siChannel -> SetInverted(dummyChannel -> GetInverted());
    siChannel -> SetPairArrayIndex(dummyChannel -> GetPairArrayIndex());
    siChannel -> SetEnergy2(dummyChannel -> GetEnergy2());
    siChannel -> SetPosition(dummyChannel -> GetPosition());
    siChannel -> SetTheta1(dummyChannel -> GetTheta1());
    siChannel -> SetTheta2(dummyChannel -> GetTheta2());
    siChannel -> SetPhi1(dummyChannel -> GetPhi1());
    siChannel -> SetPhi2(dummyChannel -> GetPhi2());

    if (dummyChannel -> IsStandaloneChannel()) siChannel -> SetIsStandaloneChannel();
    if (dummyChannel -> IsPairedChannel())     siChannel -> SetIsPairedChannel();

    siChannel -> SetTime(channel -> GetTime());
    siChannel -> SetEnergy(channel -> GetEnergy());
    siChannel -> SetPedestal(channel -> GetPedestal());
    siChannel -> SetNoiseScale(channel -> GetNoiseScale());
    siChannel -> SetBuffer(channel -> GetBuffer());
    return true;
}

int LKSiliconArray::FindSiDetectorID(int cobo, int asad, int aget, int chan)
{
    return fMapCAACToDetectorIndex[cobo][asad][aget][chan];
}

LKSiDetector* LKSiliconArray::GetSiDetector(int idx)
{
    TObject *detector = nullptr;
    if (idx >= 0 && idx < fDetectorArray -> GetEntriesFast())
        detector = fDetectorArray -> At(idx);
    return (LKSiDetector *) detector;
}

LKSiDetector* LKSiliconArray::GetSiDetector(int cobo, int asad, int aget, int chan)
{
    auto id = FindSiDetectorID(cobo, asad, aget, chan);
    if (id >= 0)
        return GetSiDetector(id);
    return (LKSiDetector*) nullptr;
}

int LKSiliconArray::FindEPairDetectorID(int det)
{
    auto detector = GetSiDetector(det);
    if (detector == nullptr)
        return -1;
    auto polarID = detector -> GetRow();
    if (detector -> GetLayer() > 1)
        return -1;
    if (detector -> IsEDetector())
        return fdEEPairMapping[polarID][1];
    return fdEEPairMapping[polarID][0];
}

bool LKSiliconArray::AddUserDrawingArray(TString label, int detID, int, TObjArray* userDrawingArray, int)
{
    auto numDrawings = userDrawingArray -> GetEntries();
    for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing) {
        auto drawing = userDrawingArray -> At(iDrawing);
        AddDrawing(drawing, label, detID);
    }
    return true;
}

bool LKSiliconArray::AddDrawing(TObject* drawing, TString label, int detID)
{
    return fRun -> AddDrawing(drawing, label, detID);
}

void LKSiliconArray::FireStrip(int det, int side, int strip, double energy)
{
    auto siDetector = (LKSiDetector*) fDetectorArray -> At(det);
    siDetector -> Fire(side, strip, energy);
}

void LKSiliconArray::ClearFiredFlags()
{
    int numDetectors = fDetectorArray -> GetEntries();
    for (auto iDetector=0; iDetector<numDetectors; ++iDetector) {
        auto siDetector = (LKSiDetector*) fDetectorArray -> At(iDetector);
        siDetector -> ClearFiredFlags();
    }
}

int LKSiliconArray::GetNumFiredDetectors()
{
    int countFiredDetectors = 0;
    int numDetectors = fDetectorArray -> GetEntries();
    for (auto iDetector=0; iDetector<numDetectors; ++iDetector) {
        auto siDetector = (LKSiDetector*) fDetectorArray -> At(iDetector);
        if (siDetector->GetNumFiredStrips()>=0)
            countFiredDetectors++;
    }
    return countFiredDetectors;
}

LKSiDetector* LKSiliconArray::GetFiredDetector(int iFired)
{
    int countFiredDetectors = 0;
    int numDetectors = fDetectorArray -> GetEntries();
    for (auto iDetector=0; iDetector<numDetectors; ++iDetector) {
        auto siDetector = (LKSiDetector*) fDetectorArray -> At(iDetector);
        if (siDetector->GetNumFiredStrips()>=0) {
            if (countFiredDetectors == iFired)
                return siDetector;
            countFiredDetectors++;
        }
    }
    return (LKSiDetector*) nullptr;
}

bool LKSiliconArray::EndOfRun()
{
    return true;
}
