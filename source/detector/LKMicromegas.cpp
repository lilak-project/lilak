#include <fstream>
using namespace std;

#include "LKMicromegas.h"
#include "GETChannel.h"
#include "TStyle.h"

ClassImp(LKMicromegas)

LKMicromegas::LKMicromegas()
    :LKMicromegas("LKMicromegas","Micromegas for LKRectangularATTPC employing GET")
{
}

LKMicromegas::LKMicromegas(const char *name, const char *title)
    :LKEvePlane(name, title)
{
    fAxis1 = LKVector3::kZ;
    fAxis2 = LKVector3::kX;
    fAxis3 = LKVector3::kY;
    fAxisDrift = LKVector3::kY;

    fPadAxis1[0] = LKVector3::kZ;
    fPadAxis2[0] = LKVector3::kX;

    fPadAxis1[1] = LKVector3::kZ;
    fPadAxis2[1] = LKVector3::kY;

    fChannelAnalyzer = nullptr;
    fPaletteNumber = 1;
}

bool LKMicromegas::Init()
{
    LKEvePlane::Init();

    fPar -> UpdatePar(fDZPad,fName+"/padsize_z  4");
    fPar -> UpdatePar(fDXPad,fName+"/padsize_x  4");
    fPar -> UpdateBinning(fDetName+"/binning_x  64,  -128, -128  # binning, range throuh x-axis", fNX, fX1, fX2);
    fPar -> UpdateBinning(fDetName+"/binning_y  512, 0,    -512  # binning, range throuh y-axis", fNY, fY2, fY1);
    fPar -> UpdateBinning(fDetName+"/binning_z  72,  0,     288  # binning, range throuh z-axis", fNZ, fZ1, fZ2);

    fPar -> UpdatePar(fMappingFileName,fDetName+"/Mapping {lilak_common}/micromegas_mapping_caac_zx.txt # cobo asad aget chan iz ix");
    fPar -> UpdatePar(fUsePixelSpace,fDetName+"/UsePixelSpace false");
    fPar -> UpdatePar(fThreshold,fDetName+"/EveThreshold 300");

    fPar -> UpdatePar(fNumCobo,fDetName+"/MaxCobo 6   # maximum number of cobos");

    fPosition = 0;
    fTbToLength = 1;
    if (!fUsePixelSpace) {
        if (fPar -> CheckPar(fDetName+"/tb_to_length  1  # mm")) {
            fTbToLength = fPar -> GetParDouble(fDetName+"/tb_to_length");
        }
    }

    fMapCAACToPadID = new int***[fNumCobo];
    for(int i=0; i<fNumCobo; ++i) {
        fMapCAACToPadID[i] = new int**[fNumAsad];
        for(int j=0; j<fNumAsad; ++j) {
            fMapCAACToPadID[i][j] = new int*[fNumAget];
            for(int k=0; k<fNumAget; ++k) {
                fMapCAACToPadID[i][j][k] = new int[fNumChan];
                for(int l=0; l<fNumChan; ++l) {
                    fMapCAACToPadID[i][j][k][l] = -1;
                }
            }
        }
    }

    ifstream fileCAACMap(fMappingFileName);
    if (!fileCAACMap.is_open()) {
        lk_error << "Cannot open " << fMappingFileName << endl;
        return false;
    }
    lk_info << "mapping: " << fMappingFileName << endl;

    for (auto ix=0; ix<fNX; ++ix) {
        for (auto iz=0; iz<fNZ; ++iz) {
            auto padID = iz + ix*fNZ;
            LKPad* pad = new LKPad();
            pad -> SetPadID(padID);
            pad -> SetPlaneID(0);
            pad -> SetSection(0);
            pad -> SetLayer(iz);
            pad -> SetRow(ix);
            auto posz = (iz+0.5)*fDZPad;
            auto posx = (ix-fNX/2+0.5)*fDXPad;
            auto errz = 0.5*fDZPad;
            auto errx = 0.5*fDXPad;
            if (fUsePixelSpace) {
                posz = iz;
                posx = ix;
                errz = 1;
                errx = 1;
                pad -> SetPosition(posz,posx);
                pad -> SetPosError(errz,errx);
                pad -> AddPadCorner(iz,ix);
                pad -> AddPadCorner(iz+1,ix);
                pad -> AddPadCorner(iz+1,ix+1);
                pad -> AddPadCorner(iz,ix+1);
            }
            else {
                pad -> SetPosition(posz,posx);
                pad -> SetPosError(errz,errx);
                pad -> AddPadCorner(posz-0.5*fDZPad,posx-0.5*fDXPad);
                pad -> AddPadCorner(posz+0.5*fDZPad,posx-0.5*fDXPad);
                pad -> AddPadCorner(posz+0.5*fDZPad,posx+0.5*fDXPad);
                pad -> AddPadCorner(posz-0.5*fDZPad,posx+0.5*fDXPad);
            }
            fChannelArray -> Add(pad);
        }
    }

    int countMap = 0;
    int cobo, asad, aget, chan, iz, ix;
    while (fileCAACMap >> cobo >> asad >> aget >> chan >> iz >> ix)
    {
        auto padID = iz + ix*fNZ;
        fMapCAACToPadID[cobo][asad][aget][chan] = padID;
        auto pad = (LKPad*) fChannelArray -> At(padID);
        pad -> SetCobo(cobo);
        pad -> SetAsad(asad);
        pad -> SetAget(aget);
        pad -> SetChan(chan);
        ++countMap;
    }

    lk_info << countMap << " pads are mapped!" << endl;

    return true;
}

TVector3 LKMicromegas::GetPositionError(int padID)
{
    if (padID<0)
        return TVector3(0,0,0);

    auto pad = (LKPad*) fChannelArray -> At(padID);
    if (pad==nullptr)
        return TVector3(0,0,0);

    LKVector3 posError = pad -> GetPosError();
    auto zerr = posError.I();
    auto xerr = posError.J();
    auto yerr = fTbToLength;
    if (fUsePixelSpace)
        yerr = 1;

    return TVector3(xerr,yerr,zerr);
}

TH2* LKMicromegas::GetHistEventDisplay1(Option_t *option)
{
    if (fHistEventDisplay1==nullptr)
    {
        if (fUsePixelSpace) {
            fHistEventDisplay1 = new TH2D("LKMicromegas_Top",fName+" Micromeagas;z;x",fNZ,0,fNZ,fNX,0,fNX);
        }
        else {
            fHistEventDisplay1 = new TH2D("LKMicromegas_Top",fName+" Micromeagas;z;x",fNZ,fZ1,fZ2,fNX,fX1,fX2);
        }
        fHistEventDisplay1 -> GetYaxis() -> SetNdivisions(512);
        fHistEventDisplay1 -> GetXaxis() -> SetNdivisions(512);
        fHistEventDisplay1 -> SetStats(0);
        fHistEventDisplay1 -> GetXaxis() -> SetTickSize(0);
        fHistEventDisplay1 -> GetYaxis() -> SetTickSize(0);

        const int maxPads = fNZ*fNX;
        const int maxBins = (fNZ+2)*(fNX+2);
        fMapBin1ToPadID = new int[maxBins]; for (auto i=0; i<maxBins; ++i) fMapBin1ToPadID[i] = -1;

        for (auto iz=0; iz<fNZ; ++iz) {
            for (auto ix=0; ix<fNX; ++ix) {
                auto bing = fHistEventDisplay1 -> GetBin(iz+1,ix+1);
                auto padID = iz + ix*fNZ;
                if (bing>=0)
                    fMapBin1ToPadID[bing] = padID;
            }
        }
    }

    return (TH2*) fHistEventDisplay1;
}

TH2* LKMicromegas::GetHistEventDisplay2(Option_t *option)
{
    if (fHistEventDisplay2==nullptr)
    {
        fNY = 128;
        if (fUsePixelSpace) {
            //fHistEventDisplay2 = new TH2D(fName+"_Side",fName+" Side;z;512-tb",fNZ,0,fNZ,fNY,0,fNY);
            fHistEventDisplay2 = new TH2D(fName+"_Side",fName+" Side;z;512-tb",fNZ,0,fNZ,fNY,0,512);
            //fHistEventDisplay2 = new TH2D(fName+"_Side",fName+" Side;z;tb",fNZ,0,fNZ,fNY,0,512);
        }
        else {
            TVector3 pos1, pos2;
            double d1, d2;
            DriftElectronBack(0, 512, pos1, d1);
            DriftElectronBack(0, 0  , pos2, d2);
            double y1 = pos1.Y();
            double y2 = pos2.Y();
            fHistEventDisplay2 = new TH2D(fName+"_Side",fName+" Side;z;y",fNZ,fZ1,fZ2,fNY,y1,y2);
        }
        fHistEventDisplay2 -> GetYaxis() -> SetNdivisions(512);
        fHistEventDisplay2 -> GetXaxis() -> SetNdivisions(512);
        fHistEventDisplay2 -> SetStats(0);
        fHistEventDisplay2 -> GetXaxis() -> SetTickSize(0);
        fHistEventDisplay2 -> GetYaxis() -> SetTickSize(0);
        fHistEventDisplay2 -> SetMinimum(fEnergyMin);
    }


    return (TH2*) fHistEventDisplay2;
}

TH1D* LKMicromegas::GetHistChannelBuffer()
{
    if (fHistChannelBuffer==nullptr)
    {
        fHistChannelBuffer = new TH1D(fName+"_Channel",";tb;y",512,0,512);
        fHistChannelBuffer -> SetStats(0);
        fHistChannelBuffer -> SetLineColor(kBlack);
        fHistChannelBuffer -> GetXaxis() -> SetTitleSize(0.06);
        fHistChannelBuffer -> GetYaxis() -> SetTitleSize(0.06);
        fHistChannelBuffer -> GetYaxis() -> SetTitleOffset(1.0);
        fHistChannelBuffer -> GetXaxis() -> SetLabelSize(0.06);
        fHistChannelBuffer -> GetYaxis() -> SetLabelSize(0.06);
    }
    return fHistChannelBuffer;
}

int LKMicromegas::FindPadID(int cobo, int asad, int aget, int chan)
{
    return fMapCAACToPadID[cobo][asad][aget][chan];
}

LKPad* LKMicromegas::FindPad(int cobo, int asad, int aget, int chan)
{
    LKPad *pad = nullptr;
    auto padID = fMapCAACToPadID[cobo][asad][aget][chan];
    if (padID>=0) {
        pad = (LKPad*) fChannelArray -> At(padID);
        return pad;
    }
    return (LKPad*) nullptr;
}

int LKMicromegas::FindPadIDFromHistEventDisplay1Bin(int hbin)
{
    return fMapBin1ToPadID[hbin];
}

TPad* LKMicromegas::Get3DEventPad()
{
    if (fCurrentView==0)
        return GetPadEventDisplay2();
    else {
        return (TPad *) nullptr;
    }
}

void LKMicromegas::UpdateEventDisplay1()
{
    LKEvePlane::UpdateEventDisplay1();
    fPadEventDisplay1 -> SetGrid();
}

void LKMicromegas::UpdateEventDisplay2()
{
    if (fCurrentView==0) // 3d
        return;

    if (fHistEventDisplay2==nullptr) 
        return;

    fPadEventDisplay2 -> cd();
    fPadEventDisplay2 -> SetGrid();
    if      (fEnergyMax==0) { fHistEventDisplay2 -> SetMinimum(fEnergyMin); fHistEventDisplay2 -> SetMaximum(-1111); }
    else if (fEnergyMax==1) { fHistEventDisplay2 -> SetMinimum(fEnergyMin); fHistEventDisplay2 -> SetMaximum(4200); }
    else
    {
        fHistEventDisplay2 -> SetMinimum(fEnergyMin);
        fHistEventDisplay2 -> SetMaximum(fEnergyMax);
        if (fEnergyMax>100)
            gStyle -> SetNumberContours(100);
        else
            gStyle -> SetNumberContours(fEnergyMax);
    }
    fHistEventDisplay2 -> Draw(fEventDisplayDrawOption);
    fEventDisplayDrawOption = "colz";
}

void LKMicromegas::FillDataToHistEventDisplay2(Option_t *option)
{
    if (fCurrentView==0) // 3d
        return;

    TString optionString(option);
    if (!fFillOptionSelected.IsNull())
        optionString = fFillOptionSelected;
    if (optionString.IsNull())
        optionString = "preview";
    optionString.ToLower();

    Long64_t currentEventID = 0;
    if (fRun!=nullptr)
        currentEventID = fRun -> GetCurrentEventID();
    lk_info << "Filling " << optionString << " (" << currentEventID << ")" << endl;

    TString title;

    if (fAccumulateEvents==0)
        fHistEventDisplay2 -> Reset();

    if (optionString.Index("hit")>=0&&fHitArray!=nullptr)
    {
        title = "Hit";
        TIter nextHit(fHitArray);
        LKHit* hit = nullptr;
        while (hit = (LKHit*) nextHit())
        {
            auto pos = hit -> GetPosition(fAxisDrift);
            auto i = pos.I();
            auto j = pos.J();
            auto k = pos.K();
            auto energy = hit -> GetCharge();
            if (fUsePixelSpace) {
                auto tb = hit -> GetTb();
                fHistEventDisplay2 -> Fill(i,512-tb,energy);
            }
            else {
                auto z = pos.Z();
                auto y = pos.Y();
                fHistEventDisplay2 -> Fill(z,y,energy);
            }
        }
    }
    else if (fRawDataArray!=nullptr)
    {
        title = "Raw Data";
        TIter nextPad(fChannelArray);
        LKPad *pad = nullptr;
        while (pad = (LKPad*) nextPad())
        {
            auto iz = pad -> GetI();
            auto ix = pad -> GetJ();
            auto idx = pad -> GetDataIndex();
            if (idx<0)
                continue;

            auto channel = (GETChannel*) fRawDataArray -> At(idx);
            auto buffer = channel -> GetWaveformY();
            auto pedestal = channel -> GetPedestal();
            auto energy = channel -> GetEnergy();
            auto time = channel -> GetTime();
            //if (energy>0||time>0)
            if (fUsePixelSpace) {
                //fHistEventDisplay2 -> Fill(iz,time,energy);
                fHistEventDisplay2 -> Fill(iz,512-time,energy);
            }
            else
            {
                fChannelAnalyzer -> Analyze(buffer);
                pedestal = fChannelAnalyzer -> GetPedestal();
                for (auto tb=0; tb<512; ++tb) {
                    auto value = buffer[tb] - pedestal;
                    if (value>fThreshold) {

                        TVector3 pos1;
                        double d1;
                        DriftElectronBack(0, tb, pos1, d1);
                        double drift = LKVector3(pos1).At(fAxisDrift);
                        fHistEventDisplay2 -> Fill(iz,drift,value);
                    }
                }
            }
        }
    }
}

void LKMicromegas::ClickedEventDisplay2(double xOnClick, double yOnClick)
{
    if (fHistEventDisplay2==nullptr)
        return;

    int selectedBin = fHistEventDisplay2 -> FindBin(xOnClick, yOnClick);
    fSelIZ = FindZFromHistEventDisplay2Bin(selectedBin);

    fCountChannelGraph = 0;
    fAccumulateChannel = 3;
    fHistControlEvent2 -> SetBinContent(fBinCtrlAcmltCh, 3);
    fHistControlEvent2 -> SetBinContent(fBinCtrlDrawACh, 3);

    UpdateChannelBuffer();
}

int LKMicromegas::FindZFromHistEventDisplay2Bin(int hbin)
{
    Int_t binx, biny, binz;
    fHistEventDisplay2 -> GetBinXYZ(hbin, binx, biny, binz);
    return (binx - 1);
    //return fMapBin2ToIZ[hbin];
}

void LKMicromegas::UpdateChannelBuffer()
{
    if (fHistChannelBuffer==nullptr) 
        return;

    if (fSelIZ>=0)
    {
        if (fUsePixelSpace)
        {
            double z1 = fSelIZ;
            double z2 = fSelIZ+1;
            double y1 = 0;
            double y2 = 512;
            fGSelEventDisplay2 -> SetPoint(0,z1,y1);
            fGSelEventDisplay2 -> SetPoint(1,z1,y2);
            fGSelEventDisplay2 -> SetPoint(2,z2,y2);
            fGSelEventDisplay2 -> SetPoint(3,z2,y1);
            fGSelEventDisplay2 -> SetPoint(4,z1,y1);
            fGSelEventDisplay2 -> SetLineColor(kRed);
            if (fPaletteNumber==0)
                fGSelEventDisplay2 -> SetLineColor(kGreen);
        }
        fPadEventDisplay2 -> cd();
        fGSelEventDisplay2 -> Draw("samel");

        fPadChannelBuffer -> cd();
        fHistChannelBuffer -> Reset();
        fHistChannelBuffer -> SetTitle(Form("iz = %d",fSelIZ));
        fHistChannelBuffer -> Draw();
        double yMin=DBL_MAX, yMax=-DBL_MAX;

        auto iz0 = fSelIZ;
        for (auto iPad=0; iPad<fNX; iPad++)
        {
            auto padID = iz0 + fNZ*iPad;
            //auto padID = iz + ix*fNZ;
            auto pad = (LKPad*) fChannelArray -> At(padID);
            auto iz = pad -> GetI();
            auto ix = pad -> GetJ();
            auto idx = pad -> GetDataIndex();
            if (idx<0)
                continue;
            auto channel = (GETChannel*) fRawDataArray -> At(idx);
            auto graph = (TGraph*) fChannelGraphArray -> ConstructedAt(fCountChannelGraph);
            fCountChannelGraph++;
            channel -> FillGraph(graph);
            graph -> Draw("plc samel");
            double x0, y0;
            auto n = graph -> GetN();
            for (auto i=0; i<n; ++i) {
                graph -> GetPoint(i,x0,y0);
                if (yMin>y0) yMin = y0;
                if (yMax<y0) yMax = y0;
            }
        }

        if (fCountChannelGraph>0)
        {
            lk_info << fCountChannelGraph << " channels in iz = " << fSelIZ << endl;
            double dy = 0.1*(yMax - yMin);
            yMin = yMin - dy;
            yMax = yMax + dy;
            if (yMin<0) yMin = 0;
            if (yMax>4200) yMax = 4200;
            fHistChannelBuffer -> SetMinimum(yMin);
            fHistChannelBuffer -> SetMaximum(yMax);
            fHistChannelBuffer -> SetTitle(Form("iz = %d (%d)",fSelIZ,fCountChannelGraph));
        }

        fSelIZ = -1;
    }
    else
        LKEvePlane::UpdateChannelBuffer();
}
