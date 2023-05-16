#include "LKDetectorSystem.hpp"
#include "LKDetector.hpp"

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
    lk_info << fTitle << endl;
    for (auto iPlane = 0; iPlane < fNumPlanes; ++iPlane) {
        auto plane = (LKDetectorPlane *) fDetectorPlaneArray -> At(iPlane);
        plane -> Print();
    }
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

void LKDetector::AddPlane(LKDetectorPlane *plane)
{
    plane -> SetRank(fRank+1);
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

void LKDetector::SetRun(LKRun *run) {
    fRun = run;
    for (auto iPlane = 0; iPlane < fNumPlanes; ++iPlane) {
        auto plane = (LKDetectorPlane *) fDetectorPlaneArray -> At(iPlane);
        plane -> SetRun(run);
    }
};
