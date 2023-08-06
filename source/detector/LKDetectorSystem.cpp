#include "LKDetectorSystem.h"
#include "TBrowser.h"

#include <iostream>
using namespace std;

ClassImp(LKDetectorSystem)

LKDetectorSystem* LKDetectorSystem::fInstance = nullptr;

LKDetectorSystem* LKDetectorSystem::GetDS() {
    if (fInstance != nullptr)
        return fInstance;
    return new LKDetectorSystem();
}

LKDetectorSystem::LKDetectorSystem()
    :LKDetectorSystem("LKDetectorSystem")
{
    fInstance = this;
}

LKDetectorSystem::LKDetectorSystem(const char *name)
{
    fName = name;
    fGeoManager = new TGeoManager();
    fGeoManager -> SetVerboseLevel(0);
    fGeoManager -> SetName(fName);
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

    auto numPlanes = GetNumPlanes();
    for (auto iPlane=0; iPlane<numPlanes; ++iPlane)
    {
        auto plane = GetDetectorPlane(iPlane);
        plane -> SetPlaneID(iPlane);
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
        fGeoManager = new TGeoManager();
        fGeoManager -> SetVerboseLevel(0);
        fGeoManager -> SetName(fName);
    }

    detector -> SetGeoManager(fGeoManager);
    detector -> SetRank(fRank+1);
    detector -> SetParent(this);
    if (fRun!=nullptr)
        detector -> SetRun(fRun);
}

void LKDetectorSystem::SetDetectorPar()
{
    TIter next(this);
    LKDetector *detector;
    while ((detector = (LKDetector *) next()))
        detector -> AddPar(fPar);
}

LKDetector *LKDetectorSystem::FindDetector(const char *name)
{
    TIter iterator(this);
    LKDetector *detector;
    while ((detector = dynamic_cast<LKDetector*>(iterator())))
    {
        if (detector) {
            auto detectorName = detector -> GetName();
            if (parName==givenName)
                return detector;
        }
    }
    return (LKDetector *) nullptr;
}

LKDetectorPlane *LKDetectorSystem::FindDetectorPlane(const char *name) const
{
    TIter iterator(this);
    LKDetector *detector;
    while ((detector = dynamic_cast<LKDetector*>(iterator())))
    {
        if (detector) {
            Int_t numPlanes = detector -> GetNumPlanes();
            for (auto iPlane=0; iPlane<numPlanes; ++iPlane)
            {
                auto plane = detector -> GetDetectorPlane(iPlane);
                TString dpName = plane -> GetName();
                if (dpName==name)
                    return plane;
            }
        }
    }
    return (LKDetectorPlane *) nullptr;
}
