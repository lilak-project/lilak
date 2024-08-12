#ifndef LKDETECTOR_HH
#define LKDETECTOR_HH

#include "LKGear.h"
#include "LKRun.h"
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
        virtual bool EndOfRun();

        TGeoVolume *CreateGeoTop(TString name = "TOP");
        void FinishGeometry();

        TGeoManager *GetGeoManager();
        void SetGeoManager(TGeoManager *);
        void SetTransparency(Int_t transparency);

        virtual bool IsInBoundary(Double_t x, Double_t y, Double_t z);

        void AddPlane(LKDetectorPlane *plane, Int_t planeID=0);
        Int_t GetNumPlanes();
        LKDetectorPlane *GetDetectorPlane(Int_t idx = 0);

        LKDetectorSystem *GetParent();
        void SetParent(LKDetectorSystem *system);

        void SetRun(LKRun *run);

        virtual bool GetEffectiveDimension(Double_t &x1, Double_t &y1, Double_t &z1, Double_t &x2, Double_t &y2, Double_t &z2);

        virtual LKChannelAnalyzer* GetChannelAnalyzer(int id=0);

    protected:
        virtual bool BuildGeometry() { return false; }
        virtual bool BuildDetectorPlane() { return false; }

        TGeoManager *fGeoManager = nullptr;

        Int_t fNumPlanes = 0;
        TObjArray *fDetectorPlaneArray;

        LKDetectorSystem *fParent = nullptr;

        LKChannelAnalyzer* fChannelAnalyzer0 = nullptr;

        int fNX;
        double fX1;
        double fX2;

        int fNY;
        double fY2;
        double fY1;

        int fNZ;
        double fZ1;
        double fZ2;

        bool fUsePixelSpace;


    ClassDef(LKDetector, 1)
};

#endif
