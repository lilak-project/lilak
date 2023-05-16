#ifndef LKDETECTORSYSTEM_HH
#define LKDETECTORSYSTEM_HH

#include "LKGear.hpp"
#include "LKDetectorPlane.hpp"
#include "LKDetector.hpp"

#include "TObjArray.h"
#include "TGeoManager.h"

#include "TObjArray.h"

class LKRun;
class LKDetector;

class LKDetectorSystem : public TObjArray, public LKGear
{
    public:
        LKDetectorSystem();
        LKDetectorSystem(const char *name);
        virtual ~LKDetectorSystem() {}

        virtual void Print(Option_t *option="") const;
        virtual bool Init();

        TGeoManager *GetGeoManager() const;
        TGeoVolume *GetGeoTopVolume() const;
        void SetGeoManager(TGeoManager *);
        void SetTransparency(Int_t transparency);

        void AddDetector(LKDetector *detector);

        Int_t GetNumDetectors() const;
        LKDetector *GetDetector(Int_t idx = 0) const;

        Int_t GetNumPlanes() const;
        LKDetectorPlane *GetDetectorPlane(Int_t idx = 0) const;

    protected:
        TGeoManager *fGeoManager = nullptr;

        void SetDetector(LKDetector *detector);
        void SetDetectorPar(); 

    ClassDef(LKDetectorSystem, 1)
};

#endif
