#include "LKBinning1.h"
#include "LKBinning.h"
#include "LKLogger.h"
#include <iostream>
using namespace std;

ClassImp(LKBinning1);

LKBinning1::LKBinning1(LKBinning1 const & binn)
    : fNX(binn.fNX), fX1(binn.fX1), fX2(binn.fX2), fWX((fX2-fX1)/fNX)
{}

LKBinning1::LKBinning1(const char* name, const char* title, int nx, double x1, double x2)
    : fNX(nx), fX1(x1), fX2(x2)
{
    SetName(name);
    SetTitle(title);
    fWX = (fX2-fX1)/fNX;
}

LKBinning1::LKBinning1(TH1 *hist)
{
    SetBinning(hist);
}

LKBinning1::LKBinning1(TGraph *graph)
{
    SetBinning(graph);
}

void LKBinning1::operator=(const LKBinning1 binn) {
    fNX = binn.fNX;
    fX1 = binn.fX1;
    fX2 = binn.fX2;
    fWX = binn.fWX;
}

LKBinning LKBinning1::operator*(const LKBinning1 binn) {
    return LKBinning("", "", fNX ,fX1 ,fX2 ,binn.fNX ,binn.fX1 ,binn.fX2);
}

TH1D* LKBinning1::NewH1(TString name, TString title)
{
    if (name.IsNull()) name = "hist1";
    if (title.IsNull()) title = fTitle;
    auto hist = new TH1D(name, title, fNX, fX1, fX2);
    return hist;
}

void LKBinning1::SetBinning(TH1 *hist, int i)
{
    if (hist->InheritsFrom(TH2::Class())) {
        if (i==0) {
            fNX = hist -> GetNbinsX();
            fX1 = hist -> GetXaxis() -> GetBinLowEdge(1);
            fX2 = hist -> GetXaxis() -> GetBinUpEdge(fNX);
            fWX = (fX2-fX1)/fNX;
        }
        else {
            fNX = hist -> GetNbinsY();
            fX1 = hist -> GetYaxis() -> GetBinLowEdge(1);
            fX2 = hist -> GetYaxis() -> GetBinUpEdge(fNX);
            fWX = (fX2-fX1)/fNX;
        }
    }
    else if (hist->InheritsFrom(TH1::Class())) {
        fNX = hist -> GetNbinsX();
        fX1 = hist -> GetXaxis() -> GetBinLowEdge(1);
        fX2 = hist -> GetXaxis() -> GetBinUpEdge(fNX);
        fWX = (fX2-fX1)/fNX;
    }
}

void LKBinning1::SetBinning(TGraph *graph)
{
    fNX = graph -> GetN();
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
}

void LKBinning1::SetNX(double nx) { fNX = nx; fWX = (fX2-fX1)/fNX; }
void LKBinning1::SetWX(double w) { fWX = w; fNX = int((fX2-fX1)/fWX); }
void LKBinning1::SetX1(double x1) { fX1 = x1; fWX = (fX2-fX1)/fNX; }
void LKBinning1::SetX2(double x2) { fX2 = x2; fWX = (fX2-fX1)/fNX; }
void LKBinning1::SetXMM(double x1, double x2) { fX1 = x1; fX2 = x2; fWX = (fX2-fX1)/fNX; }
void LKBinning1::SetXNMM(int nx, double x1, double x2) { fNX = nx; fX1 = x1; fX2 = x2; fWX = (fX2-fX1)/fNX; }
void LKBinning1::SetXMMW(double x1, double x2, double w) { fWX = w; fX1 = x1; fX2 = x2; fNX = int((fX2-fX1)/fWX); }

void LKBinning1::Reset() { fIterationIndex = -1; }
void LKBinning1::End  () { fIterationIndex = fNX; }
bool LKBinning1::Next () { if (fIterationIndex>fNX-2) return false; fValue = fX1 + ((fIterationIndex++)+1) * fWX; return true; }
bool LKBinning1::Back () { if (fIterationIndex<1    ) return false; fValue = fX1 + ((fIterationIndex--)-1) * fWX; return true; }

int    LKBinning1::FindIndex(double value) const { return int((value-fX1)/fWX); }
int    LKBinning1::FindBin(double value)   const { return (FindIndex(value) + 1); }
double LKBinning1::GetIdxLowEdge(int idx)  const { return fX1+(idx)*(fX2-fX1)/fNX; }
double LKBinning1::GetIdxUpEdge(int idx)   const { if (idx==-1) idx=fNX; return fX1+(idx+1)*(fX2-fX1)/fNX; }
double LKBinning1::GetIdxCenter(int idx)   const { return (fX1 + (idx+.5)*fWX); }
double LKBinning1::GetBinLowEdge(int bin)  const { return GetIdxLowEdge(bin-1); }
double LKBinning1::GetBinUpEdge(int bin)   const { return GetIdxUpEdge(bin-1); }
double LKBinning1::GetBinCenter(int bin)   const { return GetIdxCenter(bin-1); }
double LKBinning1::GetCenter()             const { return .5*(fX2 + fX1); }
double LKBinning1::GetFullWidth()          const { return (fX2 - fX1); }
double LKBinning1::Lerp(double r)          const { return fX1 + r*(fX2-fX1); }

TString LKBinning1::Print(bool show) const {
    TString line;
    line = line + "("+fNX+", "+fX1+", "+fX2+")";
    if (fIterationIndex>=0)
        line = line + "["+fIterationIndex+"]";
    if (show)
        cout << line << endl;
    return line;
}

bool LKBinning1::IsEmpty() const
{
    if (fNX==1 && fX1==0 && fX2==0 && fWX==0)
        return true;
    return false;
}

TGraphErrors* LKBinning1::MakeGraph(TArrayD* array)
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
