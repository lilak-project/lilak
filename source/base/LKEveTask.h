#ifndef LKEVETASK_HH
#define LKEVETASK_HH

#ifdef ACTIVATE_EVE
#include "TEveEventManager.h"
#endif

#include "LKTask.h"
#include "LKContainer.h"
#include "LKTracklet.h"
#include "LKHit.h"
#include "LKDetectorSystem.h"
#include "LKDetector.h"

class LKEveTask : public LKTask
{ 
    public:
        LKEveTask();
        virtual ~LKEveTask() {}

        bool Init();
        void Exec(Option_t*);

        void DrawEve3D();
        void DrawDetectorPlanes();
        void ConfigureDetectorPlanes();
        void ClickSelectedPlane();
        void DrawPadByPosition(Double_t x, Double_t y);

        bool SelectTrack(LKTracklet *track);

#ifdef ACTIVATE_EVE
        void ConfigureDisplayWindow();

        void SetEveLineAtt(TEveElement *el, TString branchName);
        void SetEveMarkerAtt(TEveElement *el, TString branchName);
        bool SelectHit(LKHit *hit);

        void AddEveElementToEvent(LKContainer *eveObj, bool permanent = true);
        void AddEveElementToEvent(TEveElement *element, bool permanent = true);
#endif

    private:
        vector<Int_t> fSelTrkIDs;
        vector<Int_t> fIgnTrkIDs;
        vector<Int_t> fSelPntIDs;
        vector<Int_t> fIgnPntIDs;
        vector<Int_t> fSelPDGs;
        vector<Int_t> fIgnPDGs;
        vector<Int_t> fSelMCIDs;
        vector<Int_t> fIgnMCIDs;
        vector<Int_t> fSelHitPntIDs;
        vector<Int_t> fIgnHitPntIDs;
        vector<TString> fSelBranchNames;

        int fNumBranches;
        int fNumSelectedBranches;

        TObjArray *fCvsDetectorPlaneArray = nullptr;

        LKDetectorSystem *fDetectorSystem = nullptr;

        TObjArray *fEveEventManagerArray = nullptr;
#ifdef ACTIVATE_EVE
        TEveEventManager *fEveEventManager = nullptr;
        std::vector<TEveElement *> fEveElementList;
        std::vector<TEveElement *> fPermanentEveElementList;
#endif

        Double_t fEveScale = 1;

    ClassDef(LKEveTask, 1)
};

#endif
