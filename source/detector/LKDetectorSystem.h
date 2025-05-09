#ifndef LKDETECTORSYSTEM_HH
#define LKDETECTORSYSTEM_HH

#include "LKGear.h"
#include "LKDetectorPlane.h"
#include "LKDetector.h"

#include "TObjArray.h"
#include "TGeoManager.h"

#include "TObjArray.h"

class LKVirtualRun;
class LKDetector;

class LKDetectorSystem : public TObjArray, public LKGear
{
    public:
        static LKDetectorSystem* GetDS();

        LKDetectorSystem();
        LKDetectorSystem(const char *name);
        virtual ~LKDetectorSystem() {}

        virtual void Print(Option_t *option="") const;
        virtual bool Init();
        bool EndOfRun();

        TGeoManager *GetGeoManager() const;
        TGeoVolume *GetGeoTopVolume() const;
        void SetGeoManager(TGeoManager *);
        void SetTransparency(Int_t transparency);

        void AddDetector(LKDetector *detector);
        void AddDetectorPlane(LKDetectorPlane *plane);

        Int_t GetNumDetectors() const;
        LKDetector *GetDetector(Int_t idx = 0) const;

        Int_t GetNumPlanes() const;
        LKDetectorPlane *GetDetectorPlane(Int_t idx = 0) const;

        LKDetector *FindDetector(const char *name);
        LKDetectorPlane *FindDetectorPlane(const char *name);

    protected:
        TGeoManager *fGeoManager = nullptr;

        void SetDetector(LKDetector *detector);
        void SetDetectorPar(); 

    private:
        static LKDetectorSystem *fInstance;

    ClassDef(LKDetectorSystem, 1)
};

#endif
