#ifndef LKEVETASK_HH
#define LKEVETASK_HH

#ifdef ACTIVATE_EVE
#include "TEveEventManager.h"
#endif

#include "LKTask.hpp"
#include "LKContainer.hpp"
#include "LKTracklet.hpp"
#include "LKHit.hpp"
#include "LKDetectorSystem.hpp"
#include "LKDetector.hpp"

class LKEveTask : public LKTask
{ 
    public:
        static LKEveTask* GetEve(); ///< Get KBRun static pointer.

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
        TCanvas *fCvsChannelBuffer = nullptr;
        TH1D *fHistChannelBuffer = nullptr;
        TGraph *fGraphChannelBoundary = nullptr;
        TGraph *fGraphChannelBoundaryNb[20] = {0};

        LKDetectorSystem *fDetectorSystem = nullptr;

        TObjArray *fEveEventManagerArray = nullptr;
#ifdef ACTIVATE_EVE
        TEveEventManager *fEveEventManager = nullptr;
        std::vector<TEveElement *> fEveElementList;
        std::vector<TEveElement *> fPermanentEveElementList;
#endif

        Double_t fEveScale = 1;

    private:
        static LKEveTask *fInstance;

    ClassDef(LKEveTask, 1)
};

#endif
