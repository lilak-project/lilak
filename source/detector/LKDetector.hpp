#ifndef LKDETECTOR_HH
#define LKDETECTOR_HH

#include "LKGear.hpp"
#include "LKDetectorPlane.hpp"

#include "TNamed.h"
#include "TGeoManager.h"

#include "TObjArray.h"

class LKDetectorSystem;

class LKDetector : public TNamed, public LKGear
{
    public:
        LKDetector();
        LKDetector(const char *name, const char *title);
        virtual ~LKDetector() {}

        virtual void Print(Option_t *option="") const;
        virtual bool Init() = 0;

        TGeoVolume *CreateGeoTop(TString name = "TOP");
        void FinishGeometry();

        TGeoManager *GetGeoManager();
        void SetGeoManager(TGeoManager *);
        void SetTransparency(Int_t transparency);

        void AddPlane(LKDetectorPlane *plane);
        Int_t GetNumPlanes();
        LKDetectorPlane *GetDetectorPlane(Int_t idx = 0);

        LKDetectorSystem *GetParent();
        void SetParent(LKDetectorSystem *system);

    protected:
        virtual bool BuildGeometry() = 0;
        virtual bool BuildDetectorPlane() = 0;

        TGeoManager *fGeoManager = nullptr;

        Int_t fNumPlanes = 0;
        TObjArray *fDetectorPlaneArray;

        LKDetectorSystem *fParent = nullptr;

        ClassDef(LKDetector, 0)
};

#endif
