#include "LKBinning.h"
#include "LKLogger.h"
#include <iostream>
using namespace std;

ClassImp(LKBinning);

LKBinning::LKBinning(LKBinning const & binn)
{
    SetName(binn.GetName());
    SetTitle(binn.GetTitle());
    SetXNMM(binn.nx(), binn.x1(), binn.x2());
    SetYNMM(binn.ny(), binn.y1(), binn.y2());
}

LKBinning::LKBinning(LKBinning1 const & binn)
{
    SetName(binn.GetName());
    SetTitle(binn.GetTitle());
    SetXNMM(binn.nx(), binn.x1(), binn.x2());
}

LKBinning::LKBinning(LKBinning1 const & binnx, LKBinning1 const & binny)
{
    SetName(Form("%s_%s",binnx.GetName(),binny.GetName()));
    SetTitle(Form("%s;%s",binnx.GetTitle(),binny.GetTitle()));
    SetXNMM(binnx.nx(), binnx.x1(), binnx.x2());
    SetYNMM(binny.nx(), binny.x1(), binny.x2());
}

LKBinning::LKBinning(const char* name, const char* title, int nx, double x1, double x2, int ny, double y1, double y2)
{
    SetName(name);
    SetTitle(title);
    SetXNMM(nx, x1, x2);
    SetYNMM(ny, y1, y2);
}

LKBinning::LKBinning(TH1 *hist)
{
    SetBinning(hist);
}

LKBinning::LKBinning(TGraph *graph)
{
    SetBinning(graph);
}

void LKBinning::operator=(const LKBinning binn) {
    SetName(binn.GetName());
    SetTitle(binn.GetTitle());
    SetXNMM(binn.nx(), binn.x1(), binn.x2());
    SetYNMM(binn.ny(), binn.y1(), binn.y2());
}

LKBinning LKBinning::operator*(const LKBinning binn) {
    return LKBinning(fName+"_"+binn.GetName(), fTitle+";"+binn.GetTitle(), fBinningX.nx(), fBinningX.x1(), fBinningX.x2(), binn.nx(), binn.x1(), binn.x2());
}

TH1D* LKBinning::NewH1(TString name, TString title)
{
    if (name.IsNull()) name = fName;
    if (name.IsNull()) name = "hist1";

    TString title1 = Form(";%s",fTitle.Data());
    if (title.IsNull()) title = title1;
    else if (title.Index(";")<0) title += title1;

    auto hist = new TH1D(name, title, fBinningX.nx(), fBinningX.x1(), fBinningX.x2());
    return hist;
}

TH2D* LKBinning::NewH2(TString name, TString title)
{
    if (name.IsNull()) name = fName;
    if (name.IsNull()) name = "hist2";

    TString title1 = Form(";%s",fTitle.Data());
    if (title.IsNull()) title = title1;
    else if (title.Index(";")<0) title += title1;

    auto hist = new TH2D(name, title, fBinningX.nx(), fBinningX.x1(), fBinningX.x2(), fBinningY.nx(), fBinningY.x1(), fBinningY.x2());
    return hist;
}

void LKBinning::SetBinning(TH1 *hist)
{
    if (hist->InheritsFrom(TH2::Class())) {
        TString title = hist -> GetXaxis() -> GetTitle();
        fBinningX.SetTitle(title);
        int    nx = hist -> GetNbinsX();
        double x1 = hist -> GetXaxis() -> GetBinLowEdge(1);
        double x2 = hist -> GetXaxis() -> GetBinUpEdge(nx);
        fBinningX.SetXNMM(nx,x1,x2);
        TString titley = hist -> GetYaxis() -> GetTitle();
        fBinningY.SetTitle(titley);
        int    ny = hist -> GetNbinsY();
        double y1 = hist -> GetYaxis() -> GetBinLowEdge(1);
        double y2 = hist -> GetYaxis() -> GetBinUpEdge(ny);
        fBinningY.SetXNMM(ny,y1,y2);
    }
    else if (hist->InheritsFrom(TH1::Class())) {
        TString title = hist -> GetXaxis() -> GetTitle();
        fBinningX.SetTitle(title);
        int    nx = hist -> GetNbinsX();
        double x1 = hist -> GetXaxis() -> GetBinLowEdge(1);
        double x2 = hist -> GetXaxis() -> GetBinUpEdge(nx);
        fBinningX.SetXNMM(nx,x1,x2);
    }
}

void LKBinning::SetBinning(TGraph *graph)
{
    int nx = graph -> GetN();
    double x1,x2,y1,y2;
    graph -> ComputeRange(x1,y1,x2,y2);
    double xe1 = (x2-x1)/(nx-1)/2.;
    double xe2 = (x2-x1)/(nx-1)/2.;
    double ye1 = (y2-y1)/(nx-1)/2.;
    double ye2 = (y2-y1)/(nx-1)/2.;
    if (graph->InheritsFrom(TGraphErrors::Class())) {
        xe1 = graph -> GetErrorX(0);
        xe2 = graph -> GetErrorX(nx-1);
        ye1 = graph -> GetErrorY(0);
        ye2 = graph -> GetErrorY(nx-1);
    }
    double fx1 = x1 - xe1;
    double fx2 = x2 + xe2;
    double fy1 = y1 - ye1;
    double fy2 = y2 + ye2;
    fBinningX.SetXNMM(nx,fx1,fx2);
    fBinningY.SetXNMM(100,fy1,fy2);
}

TString LKBinning::Print(bool show) const {
    TString line;
    if (fName.IsNull()==false) line = line + fName;
    if (fTitle.IsNull()==false) line = line + " | " + fTitle;
    if (line.IsNull()==false) line = line + "  ";
    line = line + "x=";
    line = line + fBinningX.Print(false);
    if (fBinningY.IsEmpty()==false) {
        line = line + ", y=";
        line = line + fBinningY.Print(false);
    }
    if (fBinningProjection.IsEmpty()==false) {
        line = line + ", projection=";
        line = line + fBinningProjection.Print(false);
    }
    if (show)
        cout << line << endl;
    return line;
}

void LKBinning::SetProjectionBinningValues(int n_proj, double x1_proj, double x2_proj)
{
    if (x1_proj==0&&x2_proj==0) {
        x1_proj = fBinningX.x1();
        x2_proj = fBinningX.x2();
    }
    else {
        if (x1_proj<fBinningX.x1()) {
            e_warning << "projection range x1 is smaller than fX1" << endl;
            x1_proj = fBinningX.x1();
        }
        if (x2_proj>fBinningX.x2()) {
            e_warning << "projection range x2 is larger than fX2" << endl;
            x2_proj = fBinningX.x2();
        }
    }
    int b1_proj = FindBin(x1_proj);
    int b2_proj = FindBin(x2_proj);
    int nb_proj = (b2_proj - b1_proj);
    if (nb_proj%n_proj!=0) {
        e_warning << "projection binning is not equal binning!" << endl;
        e_warning << "nbins=" << nb_proj << "(" << b1_proj << "," << b2_proj << "), n_proj=" << n_proj << endl;
    }
    //lk_debug << "nbins=" << nb_proj << "(" << b1_proj << "," << b2_proj << "), n_proj=" << n_proj << endl;
    fBinningProjection.SetXNMM(n_proj, b1_proj, b2_proj);
    ResetNextProjection();
}

void LKBinning::SetProjectionBinningBins(int n_proj, int b1_proj, int b2_proj)
{
    if (b1_proj==0&&b2_proj==0) {
        b1_proj = 0;
        b2_proj = fBinningX.nx();
    }
    int nb_proj = (b2_proj - b1_proj);
    if (nb_proj%n_proj!=0) {
        e_warning << "projection binning is not equal binning!" << endl;
        e_warning << "nbins=" << nb_proj << "(" << b1_proj << "," << b2_proj << "), n_proj=" << n_proj << endl;
    }
    //lk_debug << "nbins=" << nb_proj << "(" << b1_proj << "," << b2_proj << "), n_proj=" << n_proj << endl;
    fBinningProjection.SetXNMM(n_proj, b1_proj, b2_proj);
    ResetNextProjection();
}

void LKBinning::FindProjectionRange(int i_proj, int &bin1, int &bin2) const
{
    bin1 = int(fBinningProjection.x1()) + int(fBinningProjection.wx())*(i_proj);
    bin2 = int(fBinningProjection.x1()) + int(fBinningProjection.wx())*(i_proj+1) - 1;
}

TH1D* LKBinning::ProjectionX(TH2D* hist2, int i_proj)
{
    int bin1, bin2;
    FindProjectionRange(i_proj, bin1, bin2);
    TString name = hist2 -> GetName();
    name = name + "_" + i_proj;
    auto histProj = hist2 -> ProjectionX(name,bin1,bin2);
    TString title = histProj -> GetTitle();
    if (title.IsNull()==false) title += " ";
    double x1 = GetBinLowEdge(bin1);
    double x2 = GetBinUpEdge(bin2);
    title += Form("(%f,%f)",x1,x2);
    histProj -> SetTitle(title);
    return histProj;
}

TH1D* LKBinning::ProjectionY(TH2D* hist2, int i_proj)
{
    int bin1, bin2;
    FindProjectionRange(i_proj, bin1, bin2);
    TString name = hist2 -> GetName();
    name = name + "_" + i_proj;
    auto histProj = hist2 -> ProjectionY(name,bin1,bin2);
    TString title = hist2 -> GetTitle();
    if (title.IsNull()==false) title += " ";
    double x1 = GetBinLowEdge(bin1);
    double x2 = GetBinUpEdge(bin2);
    TString x1String = LKMisc::RemoveTrailing0(Form("%f",x1),true);
    TString x2String = LKMisc::RemoveTrailing0(Form("%f",x2),true);
    title += Form("(%d | %s, %s)",i_proj,x1String.Data(),x2String.Data());
    histProj -> SetTitle(title);
    return histProj;
}

void LKBinning::ResetNextProjection()
{
    fBinningProjection.Reset();
}

int LKBinning::GetCurrentProjectionIt() const
{
    return fBinningProjection.GetItIndex();
}

double LKBinning::GetProjectionCenter(int i_proj) const
{
    int bin1, bin2;
    FindProjectionRange(i_proj, bin1, bin2);
    double x1 = GetBinLowEdge(bin1);
    double x2 = GetBinUpEdge(bin2);
    double center = 0.5*(x1+x2);
    return center;
}

double LKBinning::GetProjectionLowEdge(int i_proj) const
{
    int bin1, bin2;
    FindProjectionRange(i_proj, bin1, bin2);
    double x1 = GetBinLowEdge(bin1);
    return x1;
}

double LKBinning::GetProjectionUpEdge(int i_proj) const
{
    int bin1, bin2;
    FindProjectionRange(i_proj, bin1, bin2);
    double x2 = GetBinUpEdge(bin2);
    return x2;
}

double LKBinning::GetProjectionBinWidth(int i_proj) const
{
    return (GetProjectionUpEdge(i_proj) - GetProjectionLowEdge(i_proj));
}

TH1D* LKBinning::NextProjectionX(TH2D* hist2)
{
    if (fBinningProjection.Next()==false)
        return (TH1D*) nullptr;
    return ProjectionX(hist2,fBinningProjection.GetItIndex());
}

TH1D* LKBinning::NextProjectionY(TH2D* hist2)
{
    if (fBinningProjection.Next()==false)
        return (TH1D*) nullptr;
    return ProjectionY(hist2,fBinningProjection.GetItIndex());
}

LKDrawing* LKBinning::CreateProjectionXGrid()
{
    auto draw = new LKDrawing();
    if (IsEmpty())
        return draw;
    fBinningProjection.Reset();
    while (fBinningProjection.Next())
    {
        int i_proj = fBinningProjection.GetItIndex();
        int bin1, bin2;
        FindProjectionRange(i_proj, bin1, bin2);
        double y1 = GetBinLowEdge(bin1);
        double y2 = GetBinUpEdge(bin2);
        double x1 = fBinningX.x1();
        double x2 = fBinningX.x2();
        auto graph = new TGraph();
        graph -> SetLineColor(kOrange+5);
        graph -> SetName(Form("proj_%d",i_proj));
        graph -> SetPoint(0,x1,y1);
        graph -> SetPoint(1,x2,y1);
        graph -> SetPoint(2,x2,y2);
        graph -> SetPoint(3,x1,y2);
        graph -> SetPoint(4,x1,y1);
        draw -> Add(graph,"",".");
    }
    return draw;
}

LKDrawing* LKBinning::CreateProjectionYGrid()
{
    auto draw = new LKDrawing();
    if (IsEmpty())
        return draw;
    fBinningProjection.Reset();
    while (fBinningProjection.Next())
    {
        int i_proj = fBinningProjection.GetItIndex();
        int bin1, bin2;
        FindProjectionRange(i_proj, bin1, bin2);
        double x1 = GetBinLowEdge(bin1);
        double x2 = GetBinUpEdge(bin2);
        double y1 = fBinningY.x1();
        double y2 = fBinningY.x2();
        auto graph = new TGraph();
        graph -> SetName(Form("proj_%d",i_proj));
        graph -> SetLineColor(kOrange+5);
        graph -> SetPoint(0,x1,y1);
        graph -> SetPoint(1,x2,y1);
        graph -> SetPoint(2,x2,y2);
        graph -> SetPoint(3,x1,y2);
        graph -> SetPoint(4,x1,y1);
        draw -> Add(graph,"",".");
    }
    fBinningProjection.Reset();
    return draw;
}
