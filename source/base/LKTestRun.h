#ifndef LKTESTRUN_HH
#define LKTESTRUN_HH

#include <vector>
using namespace std;

#include "LKTask.h"
#include "LKVirtualRun.h"
#include "LKParameter.h"
#include "LKParameterContainer.h"

class LKTestRun : public LKVirtualRun
{
    public:
        LKTestRun();

        virtual void Print(Option_t *option="") const;
        virtual void Add(LKTask *task);

        virtual bool Init();
        virtual void InitAndCollectParameters(TString fileName="");

        virtual bool RegisterObject(TString name, TObject *obj);
        virtual TClonesArray* RegisterBranchA(TString name, const char* className, Int_t size=100, bool persistent=true);
        virtual TString GetBranchName(Int_t idx) const;
        virtual TClonesArray *GetBranchA(TString name, bool complainIfDoNotExist=true);
        virtual TClonesArray *GetBranchA(Int_t idx);
        virtual TClonesArray *KeepBranchA(TString name);
        virtual Int_t GetNumBranches() const { return 0; }

        virtual void Add(LKDetector *detector) { AddDetector(detector); }
        virtual void Add(LKDetectorPlane *plane) { AddDetectorPlane(plane); }
        virtual void AddDetector(LKDetector *detector);
        virtual void AddDetectorPlane(LKDetectorPlane *plane);
        virtual LKDetector *GetDetector(Int_t idx=0) const { return (LKDetector*) 0; }
        virtual LKDetectorSystem *GetDetectorSystem() const { return (LKDetectorSystem*) 0; }
        virtual LKDetectorPlane *GetDetectorPlane(Int_t iDetector=0, Int_t iPlane=0) { return (LKDetectorPlane*) 0; }
        virtual LKDetector *FindDetector(const char *name) { return (LKDetector*) 0; }
        virtual LKDetectorPlane *FindDetectorPlane(const char *name) { return (LKDetectorPlane*) 0; }

        virtual void AddInputList(TString fileName, TString treeName = "event") {}
        virtual void AddInputFile(TString fileName, TString treeName = "event") {}
        virtual void SetInputFile(TString fileName, TString treeName = "event") {}
        virtual void SetInputTreeName(TString treeName) {}
        virtual TFile  *GetInputFile() { return (TFile*) 0; }
        virtual TTree  *GetInputTree() const { return (TTree*) 0; }
        virtual TChain *GetInputChain() const { return (TChain*) 0; }
        virtual LKParameterContainer* GetRunHeader() { return (LKParameterContainer*) 0; }

        virtual void AddFriend(TString fileName) {}
        virtual TChain *GetFriendChain(Int_t iFriend) const { return (TChain*) 0; }
        virtual void SetOutputFile(TString name, bool addVersion=false) {}
        virtual TFile* GetOutputFile() { return (TFile*) 0; }
        virtual TTree* GetOutputTree() { return (TTree*) 0; }
        virtual void SetDivision(Int_t division) { fDivision = division; }
        virtual void SetTag(TString tag) { fTag = tag; }
        virtual void SetSplit(Int_t split, Long64_t numEventsInSplit) {}
        virtual TString GetOutputFileName() const { return ""; }

        virtual const char* GetRunName() const { return fRunName.Data(); }
        virtual Int_t GetRunID() const { return fRunID; }
        virtual Int_t GetDivision() const { return fDivision; }
        virtual TString GetMainName() const { return fMainName; }
        virtual TString GetTag() const { return fTag; }
        TString MakeFullRunName(int useparated=false) const { return "TestRun"; }

        virtual void SetDataPath(TString path) {}
        virtual TString GetDataPath() { return ""; }

        virtual bool AddDrawing(TObject* drawing, TString label, int i=-1) { return false; }
        virtual void PrintDrawings() {}
        virtual TObjArray* GetUserDrawingArray() { return (TObjArray*) 0; }

        virtual void Draw(Option_t* option="");
        virtual LKDrawingGroup* GetTopDrawingGroup();
        virtual LKDrawingGroup* FindGroup(TString name="")      { return GetTopDrawingGroup() -> FindGroup(name,0); }
        virtual LKDrawingGroup* CreateGroup(TString name="", bool addToList=true)  { return GetTopDrawingGroup() -> CreateGroup(name, addToList); }
        virtual void            AddGroup(LKDrawingGroup *sub)   {        GetTopDrawingGroup() -> AddGroup(sub); }
        virtual LKDrawing*      CreateDrawing(TString name="")  { return GetTopDrawingGroup() -> CreateDrawing(name); }
        virtual void            AddDrawing(LKDrawing* drawing)  { CreateGroup(drawing->GetName()) -> AddDrawing(drawing); }
        virtual void            AddGraph(TGraph* graph)         { CreateGroup(graph  ->GetName()) -> AddGraph(graph); }
        virtual void            AddHist(TH1 *hist)              { CreateGroup(hist   ->GetName()) -> AddHist(hist); }

        virtual void AlwaysPrintMessage() {}
        virtual void SetEventCountForMessage(Long64_t val) {}
        virtual Long64_t GetEventCountForMessage() const { return 0; }
        virtual void SetNumPrintMessage(Long64_t num) {}

        virtual void SetNumEvents(Long64_t num) {}
        virtual Long64_t GetNumEvents() const { return 0; }
        virtual Long64_t GetNumRunEntries() const { return 0; }
        virtual Int_t GetEvent(Long64_t entry) { return 0; }
        virtual bool GetNextEvent() { return false; }

        virtual Long64_t GetStartEventID()   const { return 0; }
        virtual Long64_t GetEndEventID()     const { return 0; }
        virtual Long64_t GetCurrentEventID() const { return 0; }
        virtual Long64_t GetEventCount()     const { return 0; }

        virtual void Run(Long64_t numEvents) {}
        virtual void Run(Long64_t startID, Long64_t endID) {}
        virtual bool RunEvent(Long64_t eventID) { return false; }
        virtual bool RunSelectedEvent(TString selection) { return false; }
        virtual void RunOnline(Long64_t numEvents) {}

        virtual void ClearArrays() {};

        virtual bool ExecuteEvent(Long64_t eventID) { return false; }
        virtual bool ExecuteNextEvent() { return false; }
        virtual bool ExecutePreviousEvent() { return false; }
        virtual bool ExecuteFirstEvent() { return false; }
        virtual bool ExecuteLastEvent() { return false; }

    private:
        vector<TString> fDetectorArray;
        vector<TString> fDetectorPlaneArray;
        vector<TString> fTaskArray;
        vector<TString> fKeepBranchArray;
        vector<TString> fKeepClassArray;
        vector<TString> fInputBranchArray;
        vector<TString> fInputClassArray;
        vector<TString> fOutputBranchArray;
        vector<TString> fOutputClassArray;

        LKDataViewer* fDataViewer = nullptr;
        LKDrawingGroup* fTopDrawingGroup = nullptr;

        bool fRunNameIsSet = false;
        TString fRunName = "TestRun";
        Int_t   fRunID = 0;
        TString fMainName = "TestRun";
        vector<Int_t> fRunIDList;
        Int_t   fDivision = -1;
        TString fTag = "test";

        TString fInputFileName = "";
        TString fInputTreeName = "";
        std::vector<TString> fInputFileNameArray;
        std::vector<TString> fInputPathArray;
        TString fSearchOption = "";

        Int_t fCountBranches = 0;
        TClonesArray **fBranchPtr;
        std::vector<TString> fBranchNames;
        std::map<TString, TClonesArray*> fBranchPtrMap;

        Int_t fCountRunObjects = 0;
        TObject **fRunObjectPtr;
        TString fRunObjectName[20];
        std::map<TString, TObject*> fRunObjectPtrMap;

        vector<TString> SearchRunFiles(int runNo, TString searchOption, TString searchOption2="");

    ClassDef(LKTestRun, 1)
};

#endif
