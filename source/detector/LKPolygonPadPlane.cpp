#include <fstream>
using namespace std;

#include "TStyle.h"
#include "TH2Poly.h"

#include "LKPolygonPadPlane.h"
#include "LKListFileParser.h"
#include "CAACMapData.h"
#include "GETChannel.h"

ClassImp(LKPolygonPadPlane)

LKPolygonPadPlane::LKPolygonPadPlane()
    :LKPolygonPadPlane("LKPolygonPadPlane","")
{
}

LKPolygonPadPlane::LKPolygonPadPlane(const char *name, const char *title)
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

bool LKPolygonPadPlane::Init()
{
    LKEvePlane::Init();

    if (fDetName.IsNull()) fPar -> UpdatePar(fDetName,"detector_name");
    if (fDetName.IsNull()) fDetName = fName;

    fPar -> UpdateBinning(fDetName+"/binning_x  64,  -128, -128  # binning, range throuh x-axis", fNX, fX1, fX2);
    fPar -> UpdateBinning(fDetName+"/binning_y  512, 0,    -512  # binning, range throuh y-axis", fNY, fY2, fY1);
    fPar -> UpdateBinning(fDetName+"/binning_z  72,  0,     288  # binning, range throuh z-axis", fNZ, fZ1, fZ2);
    fPar -> UpdatePar(fMappingFileName,fDetName+"/Mapping");
    fPar -> UpdatePar(fMappingFormat,fDetName+"/MappingFormat # Refere to LKListFileParser");
    fPar -> UpdatePar(fThreshold,fDetName+"/EveThreshold 300");
    fPar -> UpdatePar(fNumCobo,fDetName+"/MaxCobo 6  # maximum number of cobos");

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

    auto hist = new TH2Poly(fName,fName+" pad plane;z;x",fNZ,0,fNZ,fNX,0,fNX);
    fHistEventDisplay1 = hist;

    LKListFileParser parser;
    parser.SetClass("CAACMapData");
    if (parser.ReadFile(fMappingFileName,fMappingFormat,true))
        return true;
    parser.Print();
    auto dataArray = parser.GetDataArray();
    auto numChannels = dataArray -> GetEntries();
    double xx[8] = {0};
    double zz[8] = {0};
    for (auto i=0; i<numChannels; ++i)
    {
        auto data = (CAACMapData*) dataArray -> At(i);

        LKPad* pad = new LKPad();
        pad -> SetPlaneID(0);
        pad -> SetPadID(data->id);
        pad -> SetCobo(data->cobo);
        pad -> SetAsad(data->asad);
        pad -> SetAget(data->aget);
        pad -> SetChan(data->chan);

        int nv = data -> n;
        double xc = 0.;
        double zc = 0.;
        for (auto i=0; i<nv; ++i)
        {
            double x = data -> x[i];
            double z = data -> z[i];
            xx[i] = x;
            zz[i] = z;
            xc += x;
            zc += z;
            pad -> AddPadCorner(z,x);
        }
        hist -> AddBin(nv, xx, zz);

        double ex = 0.;
        double ez = 0.;
        for (auto i=0; i<nv; ++i)
        {
            double ex0 = abs(xc - data -> x[i]);
            double ez0 = abs(zc - data -> z[i]);
            if (ex0>ex) { ex = ex0; }
            if (ez0>ez) { ez = ez0; }
        }
        xc = xc/nv;
        zc = zc/nv;
        pad -> SetPosition(zc,xc);
        pad -> SetPosError(ez,ex);
        fChannelArray -> Add(pad);
    }

    lk_info << numChannels << " pads are mapped!" << endl;

    return true;
}

TVector3 LKPolygonPadPlane::GetPositionError(int padID)
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

TH2* LKPolygonPadPlane::GetHistEventDisplay1(Option_t *option)
{
    return (TH2*) fHistEventDisplay1;
}

TH2* LKPolygonPadPlane::GetHistEventDisplay2(Option_t *option)
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

TH1D* LKPolygonPadPlane::GetHistChannelBuffer()
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

int LKPolygonPadPlane::FindPadID(int cobo, int asad, int aget, int chan)
{
    return fMapCAACToPadID[cobo][asad][aget][chan];
}

LKPad* LKPolygonPadPlane::FindPad(int cobo, int asad, int aget, int chan)
{
    LKPad *pad = nullptr;
    auto padID = fMapCAACToPadID[cobo][asad][aget][chan];
    if (padID>=0) {
        pad = (LKPad*) fChannelArray -> At(padID);
        return pad;
    }
    return (LKPad*) nullptr;
}

int LKPolygonPadPlane::FindPadIDFromHistEventDisplay1Bin(int hbin)
{
    return fMapBin1ToPadID[hbin];
}

TPad* LKPolygonPadPlane::Get3DEventPad()
{
    if (fCurrentView==0)
        return GetPadEventDisplay2();
    else {
        return (TPad *) nullptr;
    }
}

void LKPolygonPadPlane::UpdateEventDisplay1()
{
    LKEvePlane::UpdateEventDisplay1();
    fPadEventDisplay1 -> SetGrid();
}

void LKPolygonPadPlane::UpdateEventDisplay2()
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

void LKPolygonPadPlane::FillDataToHistEventDisplay2(Option_t *option)
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
        while ((hit = (LKHit*) nextHit()))
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
        while ((pad = (LKPad*) nextPad()))
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

void LKPolygonPadPlane::ClickedEventDisplay2(double xOnClick, double yOnClick)
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

int LKPolygonPadPlane::FindZFromHistEventDisplay2Bin(int hbin)
{
    Int_t binx, biny, binz;
    fHistEventDisplay2 -> GetBinXYZ(hbin, binx, biny, binz);
    return (binx - 1);
    //return fMapBin2ToIZ[hbin];
}

void LKPolygonPadPlane::UpdateChannelBuffer()
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
