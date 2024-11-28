#include "LKBinning.h"
#include "LKLogger.h"
#include <iostream>
using namespace std;

ClassImp(LKBinning);

LKBinning::LKBinning(LKBinning const & binn)
    : fNX(binn.fNX), fX1(binn.fX1), fX2(binn.fX2), fWX((fX2-fX1)/fNX), fNY(binn.fNY), fY1(binn.fY1), fY2(binn.fY2), fWY((fY2-fY1)/fNY)
{}

LKBinning::LKBinning(const char* name, const char* title, int nx, double x1, double x2, int ny, double y1, double y2)
    : fNX(nx), fX1(x1), fX2(x2), fNY(ny), fY1(y1), fY2(y2)
{
    SetName(name);
    SetTitle(title);
    fWX = (fX2-fX1)/fNX;
    fWY = (fY2-fY1)/fNY;
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
    fNX = binn.fNX;
    fX1 = binn.fX1;
    fX2 = binn.fX2;
    fWX = binn.fWX;
    fNY = binn.fNY;
    fY1 = binn.fY1;
    fY2 = binn.fY2;
    fWY = binn.fWY;
}

LKBinning LKBinning::operator*(const LKBinning binn) {
    TString namey = binn.GetName(); if (namey.IsNull()) namey = "y";
    TString namex = fName.Data();  if (namex.IsNull()) namex = "x";
    TString newName = Form("%s_vs_%s",namey.Data(),namex.Data());
    return LKBinning(newName, "", fNX ,fX1 ,fX2 ,binn.fNX ,binn.fX1 ,binn.fX2);
}

TH1D* LKBinning::NewH1(TString name, TString title)
{
    if (name.IsNull()) name = fName;
    if (name.IsNull()) name = "hist1";
    if (title.IsNull()) title = fTitle;
    auto hist = new TH1D(name, title, fNX, fX1, fX2);
    return hist;
}

TH2D* LKBinning::NewH2(TString name, TString title)
{
    if (name.IsNull()) name = fName;
    if (name.IsNull()) name = "hist2";
    if (title.IsNull()) title = fTitle;
    auto hist = new TH2D(name, title, fNX, fX1, fX2, fNY, fY1, fY2);
    return hist;
}

void LKBinning::SetBinning(TH1 *hist)
{
    if (hist->InheritsFrom(TH2::Class())) {
        fNX = hist -> GetNbinsX();
        fX1 = hist -> GetXaxis() -> GetBinLowEdge(1);
        fX2 = hist -> GetXaxis() -> GetBinUpEdge(fNX);
        fWX = (fX2-fX1)/fNX;
        fNY = hist -> GetNbinsY();
        fY1 = hist -> GetYaxis() -> GetBinLowEdge(1);
        fY2 = hist -> GetYaxis() -> GetBinUpEdge(fNY);
        fWY = (fY2-fY1)/fNY;
    }
    else if (hist->InheritsFrom(TH1::Class())) {
        fNX = hist -> GetNbinsX();
        fX1 = hist -> GetXaxis() -> GetBinLowEdge(1);
        fX2 = hist -> GetXaxis() -> GetBinUpEdge(fNX);
        fWX = (fX2-fX1)/fNX;
    }
}

void LKBinning::SetBinning(TGraph *graph)
{
    fNX = graph -> GetN();
    fNY = 100;
    double x1,x2,y1,y2;
    graph -> ComputeRange(x1,y1,x2,y2);
    double xe1 = (x2-x1)/(fNX-1)/2.;
    double xe2 = (x2-x1)/(fNX-1)/2.;
    double ye1 = (y2-y1)/(fNX-1)/2.;
    double ye2 = (y2-y1)/(fNX-1)/2.;
    if (graph->InheritsFrom(TGraphErrors::Class())) {
        xe1 = graph -> GetErrorX(0);
        xe2 = graph -> GetErrorX(fNX-1);
        ye1 = graph -> GetErrorY(0);
        ye2 = graph -> GetErrorY(fNX-1);
    }
    fX1 = x1 - xe1;
    fX2 = x2 + xe2;
    fWX = (fX2-fX1)/fNX;
    fY1 = y1 - ye1;
    fY2 = y2 + ye2;
    fWY = (fY2-fY1)/fNY;
}

void LKBinning::SetNX(double nx) { fNX = nx; fWX = (fX2-fX1)/fNX; }
void LKBinning::SetWX(double w) { fWX = w; fNX = int((fX2-fX1)/fWX); }
void LKBinning::SetX1(double x1) { fX1 = x1; fWX = (fX2-fX1)/fNX; }
void LKBinning::SetX2(double x2) { fX2 = x2; fWX = (fX2-fX1)/fNX; }
void LKBinning::SetXMM(double x1, double x2) { fX1 = x1; fX2 = x2; fWX = (fX2-fX1)/fNX; }
void LKBinning::SetXNMM(int nx, double x1, double x2) { fNX = nx; fX1 = x1; fX2 = x2; fWX = (fX2-fX1)/fNX; }
void LKBinning::SetXMMW(double x1, double x2, double w) { fWX = w; fX1 = x1; fX2 = x2; fNX = int((fX2-fX1)/fWX); }

void LKBinning::SetNY(double ny) { fNY = ny; fWY = (fY2-fY1)/fNY; }
void LKBinning::SetWY(double w) { fWY = w; fNY = int((fY2-fY1)/fWY); }
void LKBinning::SetY1(double y1) { fY1 = y1; fWY = (fY2-fY1)/fNY; }
void LKBinning::SetY2(double y2) { fY2 = y2; fWY = (fY2-fY1)/fNY; }
void LKBinning::SetYMM(double y1, double y2) { fY1 = y1; fY2 = y2; fWY = (fY2-fY1)/fNY; }
void LKBinning::SetYNMM(int ny, double y1, double y2) { fNY = ny; fY1 = y1; fY2 = y2; fWY = (fY2-fY1)/fNY; }
void LKBinning::SetYMMW(double y1, double y2, double w) { fWY = w; fY1 = y1; fY2 = y2; fNY = int((fY2-fY1)/fWY); }

void LKBinning::Reset() { fIterationIndex = -1; }
void LKBinning::End  () { fIterationIndex = fNX; }
bool LKBinning::Next () { if (fIterationIndex>fNX-2) return false; fValue = fX1 + ((fIterationIndex++)+1) * fWX; return true; }
bool LKBinning::Back () { if (fIterationIndex<1    ) return false; fValue = fX1 + ((fIterationIndex--)-1) * fWX; return true; }

int    LKBinning::FindIndex(double value) const { return int((value-fX1)/fWX); }
int    LKBinning::FindBin(double value)   const { return (FindIndex(value) + 1); }
double LKBinning::GetIdxLowEdge(int idx)  const { return fX1+(idx)*(fX2-fX1)/fNX; }
double LKBinning::GetIdxUpEdge(int idx)   const { if (idx==-1) idx=fNX; return fX1+(idx+1)*(fX2-fX1)/fNX; }
double LKBinning::GetIdxCenter(int idx)   const { return (fX1 + (idx+.5)*fWX); }
double LKBinning::GetBinLowEdge(int bin)  const { return GetIdxLowEdge(bin-1); }
double LKBinning::GetBinUpEdge(int bin)   const { return GetIdxUpEdge(bin-1); }
double LKBinning::GetBinCenter(int bin)   const { return GetIdxCenter(bin-1); }
double LKBinning::GetCenter()             const { return .5*(fX2 + fX1); }
double LKBinning::GetFullWidth()          const { return (fX2 - fX1); }

TString LKBinning::Print(bool show) const {
    TString line;
    if (fName.IsNull()==false) line = line + fName;
    if (fTitle.IsNull()==false) line = line + " | " + fTitle;
    if (line.IsNull()==false) line = line + "  ";
    line = line + "("+fNX+", "+fX1+", "+fX2+")";
    if ((fNY==1&&fY1==0&&fY2==0)==false) line = line + "  ("+fNY+", "+fY1+", "+fY2+")";
    if (show)
        cout << line << endl;
    return line;
}

TGraphErrors* LKBinning::MakeGraph(TArrayD* array)
{
    if (array->GetSize()!=fNX)
        return (TGraphErrors*) nullptr;
    auto graph = new TGraphErrors();
    for (auto i=0; i<fNX; ++i)
    {
        graph -> SetPoint(i,GetIdxCenter(i),array->GetAt(i));
        graph -> SetPoint(i,0.5*GetWX(),0);
    }
    return graph;
}
