#ifndef LKEVETASK_HH
#define LKEVETASK_HH

#include "TEveEventManager.h"

#include "LKTask.hpp"
#include "LKContainer.hpp"
#include "LKTracklet.hpp"
#include "LKHit.hpp"
#include "LKDetectorSystem.hpp"
#include "LKDetector.hpp"

class LKEveTask : public LKTask
{ 
    public:
        LKEveTask();
        virtual ~LKEveTask() {}

        bool Init();
        void Exec(Option_t*);

        void RunEve();
        void AddEveElementToEvent(LKContainer *eveObj, bool permanent = true);
        void AddEveElementToEvent(TEveElement *element, bool permanent = true);
        void ConfigureDisplayWindow();

        void DrawEve3D();
        void ConfigureDetectorPlanes();
        void DrawDetectorPlanes();
        void SetEveLineAtt(TEveElement *el, TString branchName);
        void SetEveMarkerAtt(TEveElement *el, TString branchName);
        bool SelectTrack(LKTracklet *track);
        bool SelectHit(LKHit *hit);

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
        TEveEventManager *fEveEventManager = nullptr;
        std::vector<TEveElement *> fEveElementList;
        std::vector<TEveElement *> fPermanentEveElementList;

        Double_t fEveScale = 1;

        ClassDef(LKEveTask, 1)
};

#endif
