#ifndef LKDETECTOR_HH
#define LKDETECTOR_HH

#include "LKGear.h"
#include "LKRun.h"
#include "LKPulseGenerator.h"
#include "LKChannelAnalyzer.h"

#include "TNamed.h"
#include "TGeoManager.h"

#include "TObjArray.h"

class LKDetectorSystem;
class LKDetectorPlane;

class LKDetector : public TNamed, public LKGear
{
    public:
        LKDetector();
        LKDetector(const char *name, const char *title);
        virtual ~LKDetector() {}

        virtual void Print(Option_t *option="") const;
        virtual bool Init();

        TGeoVolume *CreateGeoTop(TString name = "TOP");
        void FinishGeometry();

        TGeoManager *GetGeoManager();
        void SetGeoManager(TGeoManager *);
        void SetTransparency(Int_t transparency);

        virtual bool IsInBoundary(Double_t x, Double_t y, Double_t z) { return true; }

        void AddPlane(LKDetectorPlane *plane, Int_t planeID=0);
        Int_t GetNumPlanes();
        LKDetectorPlane *GetDetectorPlane(Int_t idx = 0);

        LKDetectorSystem *GetParent();
        void SetParent(LKDetectorSystem *system);

        void SetRun(LKRun *run);

        virtual bool GetEffectiveDimension(Double_t &x1, Double_t &y1, Double_t &z1, Double_t &x2, Double_t &y2, Double_t &z2) { return false; }

        virtual LKChannelAnalyzer* GetChannelAnalyzer(int id=0);

    protected:
        virtual bool BuildGeometry() = 0;
        virtual bool BuildDetectorPlane() = 0;

        TGeoManager *fGeoManager = nullptr;

        Int_t fNumPlanes = 0;
        TObjArray *fDetectorPlaneArray;

        LKPulseGenerator *fPulseGenerator = nullptr;
        LKDetectorSystem *fParent = nullptr;

        LKChannelAnalyzer* fChannelAnalyzer0;

    ClassDef(LKDetector, 1)
};

#endif
