#include "LKDetectorSystem.hpp"
#include "TBrowser.h"

#include <iostream>
using namespace std;

ClassImp(LKDetectorSystem)

LKDetectorSystem::LKDetectorSystem()
    :LKDetectorSystem("LKDetectorSystem")
{
}

LKDetectorSystem::LKDetectorSystem(const char *name)
{
    fName = name;
}

void LKDetectorSystem::Print(Option_t *) const
{
    TIter next(this);
    LKDetector *detector;
    while ((detector = (LKDetector *) next()))
        detector -> Print();
}

bool LKDetectorSystem::Init()
{
    TString title("Detector System containing");
    SetDetectorPar();

    TGeoVolume *top = new TGeoVolumeAssembly("TOP");
    fGeoManager -> SetTopVolume(top);
    fGeoManager -> SetTopVisible(true);

    TIter next(this);
    LKDetector *detector;
    while ((detector = (LKDetector *) next())) {
        SetDetector(detector);
        detector -> Init();
        title = title + ", " + detector -> GetName();
    }
    top = fGeoManager -> GetTopVolume();
    top -> CheckOverlaps();

    if (!fGeoManager -> IsClosed()) {
        fGeoManager -> SetTitle(title);
        fGeoManager -> CloseGeometry();
    }

    lk_info << title << " initialized"<< endl;

    return true;
}

TGeoManager *LKDetectorSystem::GetGeoManager() const { return fGeoManager; }
TGeoVolume *LKDetectorSystem::GetGeoTopVolume() const { return fGeoManager -> GetTopVolume(); }
void LKDetectorSystem::SetGeoManager(TGeoManager *man)
{
    fGeoManager = man;

    TIter next(this);
    LKDetector *detector;
    while ((detector = (LKDetector *) next()))
        detector -> SetGeoManager(man);
}

void LKDetectorSystem::SetTransparency(Int_t transparency)
{
    TObjArray* listVolume = gGeoManager -> GetListOfVolumes();
    Int_t nVolumes = listVolume -> GetEntries();
    for (Int_t iVolume = 0; iVolume < nVolumes; iVolume++)
        ((TGeoVolume*) listVolume -> At(iVolume)) -> SetTransparency(transparency);
}

void LKDetectorSystem::AddDetector(LKDetector *detector)
{
    SetDetector(detector);
    detector -> AddPar(fPar);
    Add(detector);
}

Int_t LKDetectorSystem::GetNumDetectors() const { return GetEntries(); }
LKDetector *LKDetectorSystem::GetDetector(Int_t idx) const { return (LKDetector *) At(idx); }

LKTpc *LKDetectorSystem::GetTpc() const
{
  TIter next(this);
  TObject *detector;
  while ((detector = next()))
    if (detector -> InheritsFrom("LKTpc"))
      return (LKTpc *) detector;

  return (LKTpc *) nullptr;
}

Int_t LKDetectorSystem::GetNumPlanes() const
{
    Int_t numPlanes = 0;

    TIter next(this);
    LKDetector *detector;
    while ((detector = (LKDetector *)next()))
        numPlanes += detector -> GetNumPlanes();

    return numPlanes;
}

LKDetectorPlane *LKDetectorSystem::GetDetectorPlane(Int_t idx) const
{
    Int_t countPlanes = 0;

    TIter next(this);
    LKDetector *detector;
    while ((detector = (LKDetector *)next()))
    {
        Int_t numPlanes0 = countPlanes;
        countPlanes += detector -> GetNumPlanes();
        if (idx>countPlanes-1)
            continue;
        return (LKDetectorPlane *) detector -> GetDetectorPlane(idx-numPlanes0);
    }

    return (LKDetectorPlane *) nullptr;
}

void LKDetectorSystem::SetDetector(LKDetector *detector)
{
    if (fGeoManager == nullptr) {
        lk_info << "Closing Geometry" << endl;
        fGeoManager = new TGeoManager();
        fGeoManager -> SetVerboseLevel(0);
        fGeoManager -> SetName(fName);
    }

    detector -> SetGeoManager(fGeoManager);
    detector -> SetRank(fRank+1);
    detector -> SetParent(this);
}

void LKDetectorSystem::SetDetectorPar()
{
    TIter next(this);
    LKDetector *detector;
    while ((detector = (LKDetector *) next()))
        detector -> AddPar(fPar);
}
