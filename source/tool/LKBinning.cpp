#include "LKBinning.h"
#include "LKLogger.h"
#include <iostream>
using namespace std;

ClassImp(LKBinning);

LKBinning::LKBinning(LKBinning const & binn)
    : fN(binn.fN), fMin(binn.fMin), fMax(binn.fMax), fBinWidth((fMax-fMin)/fN)
{}

LKBinning::LKBinning(const char* name, const char* title, int n, double min, double max, double w)
    : fN(n), fMin(min), fMax(max), fBinWidth(w)
{
    SetName(name);
    SetTitle(title);
    Init();
}

void LKBinning::Init() {
    if (fBinWidth>0&&fN<=0) fN = int((fMax-fMin)/fBinWidth);
    if (fN>0&&fBinWidth<=0) fBinWidth = (fMax-fMin)/fN;
}

LKBinning::LKBinning(TH1 *hist, int i) {
    if (i==2) {
        fN = hist -> GetNbinsY();
        fMin = hist -> GetYaxis() -> GetBinLowEdge(1);
        fMax = hist -> GetYaxis() -> GetBinUpEdge(fN);
    } else if (i==3) {
        fN = hist -> GetNbinsZ();
        fMin = hist -> GetZaxis() -> GetBinLowEdge(1);
        fMax = hist -> GetZaxis() -> GetBinUpEdge(fN);
    } else {
        fN = hist -> GetNbinsX();
        fMin = hist -> GetXaxis() -> GetBinLowEdge(1);
        fMax = hist -> GetXaxis() -> GetBinUpEdge(fN);
    }

    fBinWidth = (fMax-fMin)/fN;
}

LKBinning::LKBinning(TGraph *graph, int i)
{
    fN = graph -> GetN();
    double x1,x2,y1,y2;
    graph -> ComputeRange(x1,y1,x2,y2);
    double xe1 = (x2-x1)/(fN-1)/2.;
    double xe2 = (x2-x1)/(fN-1)/2.;
    double ye1 = (y2-y1)/(fN-1)/2.;
    double ye2 = (y2-y1)/(fN-1)/2.;
    if (graph->InheritsFrom(TGraphErrors::Class())) {
        xe1 = graph -> GetErrorX(0);
        xe2 = graph -> GetErrorX(fN-1);
        ye1 = graph -> GetErrorY(0);
        ye2 = graph -> GetErrorY(fN-1);
    }
    if (i==2) {
        fMin = y1 - ye1;
        fMax = y2 + ye2;
    } else {
        fMin = x1 - xe1;
        fMax = x2 + xe2;
    }
    fBinWidth = (fMax-fMin)/fN;
}

void LKBinning::operator=(const LKBinning binn) {
    fN = binn.fN;
    fMin = binn.fMin;
    fMax = binn.fMax;
    fBinWidth = binn.fBinWidth;
}

void LKBinning::SetN(double n) { fN = n; fBinWidth = (fMax-fMin)/fN; }
void LKBinning::SetW(double w) { fBinWidth = w; fN = int((fMax-fMin)/fBinWidth); }
void LKBinning::SetMin(double min) { fMin = min; fBinWidth = (fMax-fMin)/fN; }
void LKBinning::SetMax(double max) { fMax = max; fBinWidth = (fMax-fMin)/fN; }
void LKBinning::SetMM(double min, double max) { fMin = min; fMax = max; fBinWidth = (fMax-fMin)/fN; }
void LKBinning::SetNMM(int n, double min, double max) { fN = n; fMin = min; fMax = max; fBinWidth = (fMax-fMin)/fN; }
void LKBinning::SetMMW(double min, double max, double w) { fBinWidth = w; fMin = min; fMax = max; fN = int((fMax-fMin)/fBinWidth); }

void LKBinning::Reset() { fIterationIndex = -1; }
void LKBinning::End  () { fIterationIndex = fN; }
bool LKBinning::Next () { if (fIterationIndex>fN-2) return false; fValue = fMin + ((fIterationIndex++)+1) * fBinWidth; return true; }
bool LKBinning::Back () { if (fIterationIndex<1   ) return false; fValue = fMin + ((fIterationIndex--)-1) * fBinWidth; return true; }

int    LKBinning::GetBin(double invalue) const { return int((invalue-fMin)/fBinWidth); }
double LKBinning::GetLowEdge(int i)      const { return fMin+(i-1)*(fMax-fMin)/fN; }
double LKBinning::GetUpEdge(int i)       const { if (i==-1) i=fN; return fMin+(i)*(fMax-fMin)/fN; }
double LKBinning::GetBinCenter(int bin)  const { return (fMin + (bin-.5)*fBinWidth); }
double LKBinning::GetCenter()            const { return .5*(fMax + fMin); }
double LKBinning::GetFullWidth()         const { return (fMax - fMin); }

TString LKBinning::Print(bool show) const {
    TString line;
    line = TString("(")+fN+","+fMin+","+fMax+";"+fBinWidth+")";
    if (show)
        cout << line << endl;
    return line;
}
