#include "LKDetectorSystem.h"
#include "LKDetector.h"

#include <iostream>
using namespace std;

ClassImp(LKDetector)

LKDetector::LKDetector()
    :LKDetector("LKDetector","default detector class")
{
}

LKDetector::LKDetector(const char *name, const char *title)
    :TNamed(name, title), fDetectorPlaneArray(new TObjArray())
{
}

void LKDetector::Print(Option_t *) const
{
    lk_info << fName << " " << fTitle << endl;
    for (auto iPlane = 0; iPlane < fNumPlanes; ++iPlane) {
        auto plane = (LKDetectorPlane *) fDetectorPlaneArray -> At(iPlane);
        plane -> Print();
    }
}

bool LKDetector::Init()
{
    fPar -> UpdateBinning(fName+"/binning_x  64,  -128, -128  # binning, range throuh x-axis", fNX, fX1, fX2);
    fPar -> UpdateBinning(fName+"/binning_y  512, 0,    -512  # binning, range throuh y-axis", fNY, fY2, fY1);
    fPar -> UpdateBinning(fName+"/binning_z  72,  0,     288  # binning, range throuh z-axis", fNZ, fZ1, fZ2);

    BuildDetectorPlane();
    for (auto iPlane = 0; iPlane < fNumPlanes; ++iPlane) {
        auto plane = (LKDetectorPlane *) fDetectorPlaneArray -> At(iPlane);
        plane -> SetPar(fPar);
        bool planeIsInitialized = plane -> Init();
        if (planeIsInitialized==false) {
            lk_error << "Error while initializing " << plane -> GetName() << endl;
            return false;
        }
    }

    fPar -> UpdatePar(fUsePixelSpace,fName+"/UsePixelSpace false");

    return true;
}

bool LKDetector::EndOfRun()
{
    bool eor = true;
    TIter next(fDetectorPlaneArray);
    LKDetectorPlane *plane;
    while ((plane = (LKDetectorPlane *) next()))
        eor = (eor && plane -> EndOfRun());
    return eor;
}

TGeoManager *LKDetector::GetGeoManager() { return fGeoManager; }
void LKDetector::SetGeoManager(TGeoManager *man) { fGeoManager = man; }

void LKDetector::SetTransparency(Int_t transparency)
{
    TObjArray* listVolume = gGeoManager -> GetListOfVolumes();
    Int_t nVolumes = listVolume -> GetEntries();
    for (Int_t iVolume = 0; iVolume < nVolumes; iVolume++)
        ((TGeoVolume*) listVolume -> At(iVolume)) -> SetTransparency(transparency);
}

void LKDetector::AddPlane(LKDetectorPlane *plane, Int_t planeID)
{
    plane -> SetPlaneID(planeID);
    plane -> SetPar(fPar);
    plane -> SetRank(fRank+1);
    plane -> SetDetector(this);
    plane -> SetDetectorName(fName);
    if (fRun!=nullptr)
        plane -> SetRun(fRun);
    fDetectorPlaneArray -> Add(plane);
    fNumPlanes = fDetectorPlaneArray -> GetEntries();
}

Int_t LKDetector::GetNumPlanes() { return fNumPlanes; }

LKDetectorPlane *LKDetector::GetDetectorPlane(Int_t idx) { return (LKDetectorPlane *) fDetectorPlaneArray -> At(idx); }

LKDetectorSystem *LKDetector::GetParent() { return fParent; }
void LKDetector::SetParent(LKDetectorSystem *system) { fParent = system; }

TGeoVolume *LKDetector::CreateGeoTop(TString name)
{
    if (fParent == nullptr) {
        TGeoVolume *top = new TGeoVolumeAssembly(name);
        fGeoManager -> SetTopVolume(top);
        fGeoManager -> SetTopVisible(true);
        lk_info << "Creating Geometry " << name << endl;
        return top;
    }

    return fParent -> GetGeoTopVolume();
}

void LKDetector::FinishGeometry()
{
    if (fParent == nullptr) {
        if (fGeoManager -> IsClosed())
            lk_info << "Geometry is closed already" << endl;
        else {
            fGeoManager -> CloseGeometry();
            lk_info << "Closing geometry " << endl;
        }
    }
}

void LKDetector::SetRun(LKVirtualRun *run)
{
    fRun = run;
    for (auto iPlane = 0; iPlane < fNumPlanes; ++iPlane) {
        auto plane = (LKDetectorPlane *) fDetectorPlaneArray -> At(iPlane);
        plane -> SetRun(run);
    }
}

bool LKDetector::IsInBoundary(Double_t x, Double_t y, Double_t z)
{
    if (x>=fX1 && x<=fX2 && y>=fY1 && y<=fY2 && z>=fZ1 && z<=fZ2)
        return true;
    return false;
}

bool LKDetector::GetEffectiveDimension(Double_t &x1, Double_t &y1, Double_t &z1, Double_t &x2, Double_t &y2, Double_t &z2)
{
    if (fUsePixelSpace)
    {
        x1 = 0;
        x2 = fNX;
        y1 = -fNY;
        y2 = 0;
        z1 = 0;
        z2 = fNZ;
    }
    else {
        x1 = fX1;
        x2 = fX2;
        y1 = fY1;
        y2 = fY2;
        z1 = fZ1;
        z2 = fZ2;
    }
    return true;
}

LKChannelAnalyzer* LKDetector::GetChannelAnalyzer(int)
{
    if (fChannelAnalyzer0==nullptr)
    {
        if (fPar->CheckPar("pulseFile")==false)
            fPar -> AddLine("pulseFile {lilak_common}/pulseReference_PulseExtraction.root");
        TString pulseFileName = fPar -> GetParString("pulseFile");
        fChannelAnalyzer0 = new LKChannelAnalyzer();
        fChannelAnalyzer0 -> SetPulse(pulseFileName);
    }
    return fChannelAnalyzer0;
}
