#ifndef LKEVETASK_HH
#define LKEVETASK_HH

#ifdef ACTIVATE_EVE
#include "TEveEventManager.h"
#else
#include "TH3D.h"
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
        bool SelectHit(LKHit *hit);

#ifdef ACTIVATE_EVE
        void ConfigureDisplayWindow();
        void AddEveElementToEvent(LKContainer *eveObj, bool permanent = true);
        void AddEveElementToEvent(TEveElement *element, bool permanent = true);
#endif
        void SetEveLineAtt(TAttLine *el, TString branchName);
        void SetEveMarkerAtt(TAttMarker *el, TString branchName);
        void SetGraphAtt(TGraph *graph, TString branchName);
        void SetGraphAtt(TGraph2D *graph, TString branchName);

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
#else
        TCanvas* fCanvas3D = nullptr;
        TH3D* fFrame3D = nullptr;
        TClonesArray *fGraphTrack3DArray = nullptr;
        TClonesArray *fGraphHit3DArray = nullptr;
#endif

        Double_t fEveScale = 1;

    ClassDef(LKEveTask, 1)
};

#endif
