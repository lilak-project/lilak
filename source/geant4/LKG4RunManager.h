#ifndef LKG4RUNMANAGER_HH
#define LKG4RUNMANAGER_HH

#define g4man_info LKLogger("LKG4RunManager",__FUNCTION__,0,2)
#define g4man_warning LKLogger("LKG4RunManager",__FUNCTION__,0,3)
#define g4man_error LKLogger("LKG4RunManager",__FUNCTION__,0,4)

#include <iostream>
#include <vector>
#include <map>
using namespace std;

#include "G4RunManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include "TObjArray.h"
#include "TString.h"

#include "LKGear.h"
#include "LKMCTrack.h"
#include "LKMCStep.h"
#include "LKParameter.h"
#include "LKParameterContainer.h"

class LKG4RunManager : public G4RunManager, public LKGear
{
    public:
        LKG4RunManager();
        virtual ~LKG4RunManager();

        virtual void AddParameterContainer(TString fname);
        virtual void SetNumEvents(Int_t numEvents);
        virtual void SetSuppressInitMessage(bool val);

        virtual void SetSensitiveDetector(G4VPhysicalVolume *physicalVolume, TString assemblyName="");
        virtual void SetVolume(G4VPhysicalVolume *physicalVolume);

        virtual LKParameterContainer *GetVolumes();
        virtual LKParameterContainer *GetSensitiveDetectors();
        virtual LKParameterContainer *GetProcessTable();

        virtual void AddMCTrack(Int_t trackID, Int_t parentID, Int_t pdg, Int_t detectorID, Int_t processID, Double_t vx, Double_t vy, Double_t vz, Double_t px, Double_t py, Double_t pz, Double_t energy);
        virtual void AddTrackVertex(Int_t detectorID, Int_t processID, Double_t vx, Double_t vy, Double_t vz, Double_t px, Double_t py, Double_t pz, Double_t energy);
        virtual void AddMCStep(Int_t dID, Double_t x, Double_t y, Double_t z, Double_t t, Double_t e);

        virtual void InitPreAddActions();
        virtual void InitPostAddActions() {}
        virtual void InitHead();
        virtual void InitSummary();
        //virtual void InitializeAndRun(G4int argc=0, char **argv=nullptr, const G4String &type="");
        virtual bool InitGeneratorFile();
        virtual void InitOutputFile();

        virtual void Run(G4int argc=0, char **argv=nullptr, const G4String &type="");
        virtual void BeamOnAll();
        virtual void NextEvent();
        virtual void ClearEvent();
        virtual void WriteToFile(TObject *obj);
        virtual void EndOfRun();
        virtual void WriteTextFile();

        virtual void SetCollectPar(TString name) { fCollecteParAndPrintTo = "collected_parameters"; }

    public:
        virtual void Initialize();
        virtual void InitializeGeometry();
        virtual void InitializePhysics();

        virtual void BeamOn(G4int numEvents, const char *macroFile=0, G4int numSelect=-1);

    protected:
        bool fSuppressInitMessage = false;

        TFile* fFile;
        TTree* fTree;
        TClonesArray *fTrackArray;
        TObjArray *fStepArrayList;
        std::vector<TString> fStepNameList;

        Double_t fEdepSumArray[200] = {0};

        std::vector<Int_t> fListOfCopyNo;
        std::map<Int_t, Int_t> fIdxOfCopyNo;
        Int_t fNumActiveVolumes = 0;

        bool fStepPersistency = false;
        bool fEdepSumPersistency = false;
        bool fSecondaryPersistency = false;
        bool fTrackVertexPersistency = false;

        LKParameterContainer *fVolumes;
        LKParameterContainer *fSensitiveDetectors;
        LKParameterContainer *fProcessTable;

        vector<TString> fLFAssemblySD;
        vector<TString> fLFSingleSD;

        Int_t fMaxCopyNo = 0;
        Int_t fNumSDAssemblyInTree = 0;
        map<Int_t, Int_t> fMapSDToAssembly;
        map<Int_t, Int_t> fMapSDToStep;

        Int_t fTrackID = 0;
        LKMCTrack* fCurrentTrack = nullptr;

        Int_t fNumEvents;

        bool fTryInitialization = false;
        bool fInitialized = false;
        bool fInitEventGenerator = false;
        bool fSetAutoUpdateCopyNo = true;
        bool fUseVisMode = false;
        TString fG4CommandFileName;
        TString fOutputFileName;

        Long64_t fRandomSeed = 0;
        std::vector<TString> fSDNames;
        TString fCollecteParAndPrintTo;
        TString fGeneratorFileName;
        TString fDetectorConstructionName;
        bool fWriteTextFile = false;
};

#endif
