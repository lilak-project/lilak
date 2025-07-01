#ifndef LKVIRTUALRUN_HH
#define LKVIRTUALRUN_HH

#include "TString.h"
#include "TFile.h"
#include "TTree.h"
#include "TChain.h"

#include "LKTask.h"
#include "LKDetector.h"
#include "LKDetectorPlane.h"
#include "LKDetectorSystem.h"

class LKDataViewer;
#include "LKDrawingGroup.h"
#include "LKDrawing.h"

class LKVirtualRun : public LKTask
{
    public:
        LKVirtualRun() {}
        LKVirtualRun(const char* name, const char *title) : LKTask(name, title) {}

        virtual void Print(Option_t *option) const = 0;
        virtual void Add(LKTask *task) = 0;

        virtual bool Init() = 0;
        virtual void InitAndCollectParameters(TString fileName="") = 0;

        virtual bool RegisterObject(TString name, TObject *obj) = 0;
        virtual TClonesArray* RegisterBranchA(TString name, const char* className, Int_t size=100, bool persistent=true) = 0;
        virtual TString GetBranchName(Int_t idx) const = 0;
        virtual TClonesArray *GetBranchA(TString name, bool complainIfDoNotExist=true) = 0;
        virtual TClonesArray *GetBranchA(Int_t idx) = 0;
        virtual TClonesArray *KeepBranchA(TString name) = 0;
        virtual Int_t GetNumBranches() const = 0;

        virtual void Add(LKDetector *detector) { AddDetector(detector); }
        virtual void Add(LKDetectorPlane *plane) { AddDetectorPlane(plane); }
        virtual void AddDetector(LKDetector *detector) = 0;
        virtual void AddDetectorPlane(LKDetectorPlane *plane) = 0;
        virtual LKDetector *GetDetector(Int_t idx=0) const = 0;
        virtual LKDetectorSystem *GetDetectorSystem() const = 0;
        virtual LKDetectorPlane *GetDetectorPlane(Int_t iDetector=0, Int_t iPlane=0) = 0;
        virtual LKDetector *FindDetector(const char *name) = 0;
        virtual LKDetectorPlane *FindDetectorPlane(const char *name) = 0;

        virtual void AddInputList(TString fileName, TString treeName = "event") = 0;
        virtual void AddInputFile(TString fileName, TString treeName = "event") = 0;
        virtual void SetInputFile(TString fileName, TString treeName = "event") = 0;
        virtual void SetInputTreeName(TString treeName) = 0;
        virtual TFile  *GetInputFile() = 0;
        virtual TTree  *GetInputTree() const = 0;
        virtual TChain *GetInputChain() const = 0;
        virtual LKParameterContainer* GetRunHeader() = 0;

        virtual void AddFriend(TString fileName) = 0;
        virtual TChain *GetFriendChain(Int_t iFriend) const = 0;
        virtual void SetOutputFile(TString name, bool addVersion=false) = 0;
        virtual TFile* GetOutputFile() = 0;
        virtual TTree* GetOutputTree() = 0;
        virtual void SetDivision(Int_t division) = 0;
        virtual void SetTag(TString tag) = 0;
        virtual void SetSplit(Int_t split, Long64_t numEventsInSplit) = 0;
        virtual TString GetOutputFileName() const = 0;

        virtual const char* GetRunName() const { return "VirtualRun"; }
        virtual Int_t GetRunID() const { return 0; }
        virtual Int_t GetDivision() const { return 0; }
        virtual TString GetMainName() const { return "VirtualRun"; }
        virtual TString GetTag() const { return "virtual"; }
        TString MakeFullRunName(int useparated=false) const { return "VirtualRun"; }

        virtual void SetDataPath(TString path) = 0;
        virtual TString GetDataPath() = 0;

        virtual bool AddDrawing(TObject* drawing, TString label, int i=-1)= 0;
        virtual void PrintDrawings() = 0;
        virtual TObjArray* GetUserDrawingArray() = 0;

        virtual void Draw(Option_t* option) = 0;
        virtual LKDrawingGroup* GetTopDrawingGroup() = 0;
        virtual LKDrawingGroup* FindGroup(TString name) { return GetTopDrawingGroup() -> FindGroup(name,0); }
        virtual LKDrawingGroup* CreateGroup(TString name, bool addToList=true)  { return GetTopDrawingGroup() -> CreateGroup(name, addToList); }
        virtual void            AddGroup(LKDrawingGroup *sub) = 0;
        virtual LKDrawing*      CreateDrawing(TString name) = 0;
        virtual void            AddDrawing(LKDrawing* drawing) = 0;
        virtual void            AddGraph(TGraph* graph) = 0;
        virtual void            AddHist(TH1 *hist) = 0;

        virtual void AlwaysPrintMessage() = 0;
        virtual void SetEventCountForMessage(Long64_t val) = 0;
        virtual Long64_t GetEventCountForMessage() const = 0;
        virtual void SetNumPrintMessage(Long64_t num) = 0;

        virtual void SetNumEvents(Long64_t num) = 0;
        virtual Long64_t GetNumEvents() const = 0;
        virtual Long64_t GetNumRunEntries() const = 0;
        virtual Int_t GetEvent(Long64_t entry) = 0;
        virtual bool GetNextEvent() = 0;

        virtual Long64_t GetStartEventID()   const = 0;
        virtual Long64_t GetEndEventID()     const = 0;
        virtual Long64_t GetCurrentEventID() const = 0;
        virtual Long64_t GetEventCount()     const = 0;

        virtual void Run(Long64_t numEvents) = 0;
        virtual void Run(Long64_t startID, Long64_t endID) = 0;
        virtual bool RunEvent(Long64_t eventID) = 0;
        virtual bool RunSelectedEvent(TString selection) = 0;
        virtual void RunOnline(Long64_t numEvents) = 0;

        virtual bool ExecuteEvent(Long64_t eventID) = 0;
        virtual bool ExecuteNextEvent() = 0;
        virtual bool ExecutePreviousEvent() = 0;
        virtual bool ExecuteFirstEvent() = 0;
        virtual bool ExecuteLastEvent() = 0;

        virtual bool CheckMute(Long64_t eventCount=-1) = 0;
        virtual void SignalEndOfRun() = 0;

    ClassDef(LKVirtualRun, 1)
};

#endif
