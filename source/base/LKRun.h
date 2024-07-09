#ifndef LKRUN_HH
#define LKRUN_HH

#include <map>
#include <vector>
#include <fstream>
#include <stdlib.h>
using namespace std;

#include "TH1D.h"
#include "TFile.h"
#include "TTree.h"
#include "TError.h"
#include "TGraph.h"
#include "TChain.h"
#include "TObject.h"
#include "TCanvas.h"
#include "TClonesArray.h"
#include "TSysEvtHandler.h"

#include "LKCompiled.h"
#include "LKTask.h"
#include "LKLogger.h"
#include "LKParameterContainer.h"

#include "LKDetectorSystem.h"
#include "LKDetector.h"

/**
 *  If output file name is not set, output file name will be named as below.
 *  In general:   [run_name]_[run_id(4-digits)].[lilak_version].root;
 *  If tag exist: [run_name]_[run_id(4-digits)].[user-tag].[lilak_version].root;
 *  If splited:   [run_name]_[run_id(4-digits)]_s[split_no].[user-tag].[lilak_version].root;
 */
class LKRun : public LKTask
{
    public:
        static LKRun* GetRun(); ///< Get LKRun static pointer.

        LKRun(TString runName, Int_t id, Int_t division, TString tag);
        LKRun() : LKRun("run",-1,-1,"") {}
        virtual ~LKRun() {};

        static void PrintLILAK(); ///< Print compiled LILAK information

        /**
         * ## options
         * - all  : Print all (below)
         * - gen  : Print general information
         * - par  : Print parameter information
         * - out  : Print output information
         * - in   : Print input information
         * - det  : Print detector information
         * - task : Print detector information
         */
        void Print(Option_t *option="all") const;

        /// Add task to be executed every event
        void Add(LKTask *task);
        void AddEveTask(LKTask *task);

        /// Set EventTrigger which give signal to LKRun that evnet has started.
        /// EventTrigger should call LKRun::ExecuteNextEvent() when ever event start, from Exec() method.
        /// EventTrigger should Return from the Exec() method after all events has been executed.
        void SetEventTrigger(LKTask *task);

        /// Print element of each branch in event
        /// @param entry Entry number. -1 will use the current entry.
        /// @param branchNames ex) Track:Hit:RawData
        /// @param numPrintCut The number of elements to be printed at most
        void PrintEvent(Long64_t entry=-1, TString branchNames="", Int_t numPrintCut=-1);
        void PrintEvent(TString branchNames, Int_t numPrintCut=-1) { PrintEvent(-1, branchNames, numPrintCut); }

        /// Run name/id
        /// This is equivalent to setting parameter
        /// LKRun/RunName [name] [id] [division(optional)].[tag(optional)]
        void SetRunName(TString name, Int_t id=0, Int_t division=-1, TString tag=""); ///< Set Run name and id.
        const char* GetRunName() const { return fRunName.Data(); }
        Int_t GetRunID() const { return fRunID; }
        Int_t GetDivision() const { return fDivision; }
        TString GetTag() const { return fTag; }
        TString MakeFullRunName() const;

        /// Set data directory path. Default directory : path/to/LILAK/data
        /// Data path will not be used if full path of the input/ouput file is given.
        /// This is equivalent to setting parameter LKRun/DataPath.
        /// Hoever SetDataPath method has the higher priority over setting parameter
        void SetDataPath(TString path) { fOutputPath = path; }
        TString GetDataPath() { return fOutputPath; }

        bool ConfigureRunFromFileName(TString inputName);
        TString ConfigureFileName();

        /// Input file
        void AddInputList(TString fileName, TString treeName = "event"); ///< Add file to input file
        void AddInputFile(TString fileName, TString treeName = "event"); ///< Add file to input file
        void SetInputFile(TString fileName, TString treeName = "event") { return AddInputFile(fileName, treeName); } ///< Add file to input file
        void SetInputTreeName(TString treeName) { fInputTreeName = treeName; } ///< Set input tree name
        TFile  *GetInputFile() { return fInputFile; }
        TTree  *GetInputTree() const { return (TTree *) fInputTree; }
        TChain *GetInputChain() const { return fInputTree; }

        LKParameterContainer* GetRunHeader() { return fRunHeader; }

        void AddFriend(TString fileName); ///< Add file to input file
        TChain *GetFriendChain(Int_t iFriend) const;

        /// Output file
        void SetOutputFile(TString name, bool addVersion=false) { fOutputFileName = name; fAddVersion = addVersion; }
        TFile* GetOutputFile() { return fOutputFile; }
        TTree* GetOutputTree() { return fOutputTree; }
        void SetDivision(Int_t division) { fDivision = division; }
        void SetTag(TString tag) { fTag = tag; }
        void SetSplit(Int_t split, Long64_t numEventsInSplit) { fSplit = split; fNumEventsInSplit = numEventsInSplit; }
        TString GetOutputFileName() const { return fOutputFileName; }

        void AlwaysPrintMessage() { fEventCountForMessage = 1; }
        void SetEventCountForMessage(Long64_t val) { fEventCountForMessage = val; }
        void SetNumPrintMessage(Long64_t num) { fNumPrintMessage = num; }

        void AddParAfter(TString fname) { fParAddAfter = fname; }

        /**
         * Initailize LKRun.
         * Configure input and output files(trees and branches), input parameters and detectors
         * Init() must be done before Run().
         */
        bool Init();

        /**
         * Initailize LKRun and collect parameters.
         * Print collected parameters to fileName.
         * Print out to screen if fileName is null.
         */
        void InitAndCollectParameters(TString fileName="");

        LKParameterContainer *GetG4ProcessTable() const { return fG4ProcessTable; }
        LKParameterContainer *GetG4SDTable() const { return fG4SDTable; }
        LKParameterContainer *GetG4VolumeTable() const { return fG4VolumeTable; }

        /**
         * Register obj as a output branch with given name.
         * obj will not be registered if same name already exist in the branch list and return fail.
         * Object(obj) is recommanded to be TClones array
         * ex) dataArray = new TClonesArray("TDataClass",1000); // 1000, for example should be around expected maximum number of "data"
         * If persistent is true, branch will write it's data to output tree.
         * If persistent is false, branch will not be written. However the data is accessible during the run
         * One can set persistency using the parameter [branchName]/persistency
         */
        //bool RegisterBranch(TString name, TObject *obj, bool persistent=true);

        /**
         * Register obj and write directly to ouput file
         */
        bool RegisterObject(TString name, TObject *obj);

        /**
         * Create, register and return TClonesArray object.
         * TClonesArray object will not be registered if same name already exist in the branch list and return fail.
         * If persistent is true, branch will write it's data to output tree.
         * If persistent is false, branch will not be written. However the data is accessible during the run
         * One can set persistency using the parameter [branchName]/persistency
         */
        TClonesArray* RegisterBranchA(TString name, const char* className, Int_t size=100, bool persistent=true);

        TString GetBranchName(Int_t idx) const;
        //TObject *GetBranch(TString name); ///< Get branch in TObject by name.
        //TObject *GetBranch(Int_t idx);
        //TObject *KeepBranch(TString name);

        TClonesArray *GetBranchA(TString name); ///< Get branch in TClonesArray by name. Return nullptr if branch is not inherited from TClonesArray
        TClonesArray *GetBranchA(Int_t idx);
        TClonesArray *KeepBranchA(TString name);
        Int_t GetNumBranches() const { return fCountBranches; }

        void AddDetector(LKDetector *detector); ///< Set detector
        LKDetector *GetDetector(Int_t idx=0) const;
        LKDetectorSystem *GetDetectorSystem() const;
        LKDetectorPlane *GetDetectorPlane(Int_t iDetector=0, Int_t iPlane=0);

        LKDetector *FindDetector(const char *name);
        LKDetectorPlane *FindDetectorPlane(const char *name);

        void SetEntries(Long64_t num) { fNumEntries = num; } ///< Set total number of entries. Use only input do not exist.
        Long64_t GetEntries() const { return fNumEntries; } ///< Get total number of entries
        /// GetEntry current from input tree. For options 
        /// For options see TTree::GetEntry : https://root.cern.ch/doc/master/classTTree.html#a14c88179bd5fd2116228707d6addea9f
        Int_t GetEntry(Long64_t entry = 0, Int_t getall = 0);

        void SetNumEvents(Long64_t num) { SetEntries(num); } ///< Equavalent to SetEntries
        Long64_t GetNumEvents() const { return fNumEntries; } ///< Equavalent to GetNumEvents
        Long64_t GetNumRunEntries() const { return fNumRunEntries; } ///< Number of events used in the run
        Int_t GetEvent(Long64_t entry) { return GetEntry(entry); } ///< Equavalent to GetEntry of input tree
        bool GetNextEvent();

        Long64_t GetStartEventID()   const { return fStartEventID; }   ///< Get starting eventID
        Long64_t GetEndEventID()     const { return fEndEventID; }     ///< Get ending eventID
        Long64_t GetCurrentEventID() const { return fCurrentEventID; } ///< Get current eventID
        Long64_t GetEventCount()     const { return fEventCount; }     ///< Get eventID count [eventa count] = [current event ID] - [start event ID]

        bool WriteOutputFile();

        /**
         * Run all events
         *
         * - Run(nid), Run(id1,id2), RunEvent(id) and RunSelectedEvent(selection):
         *   >> StartOfRun() -> Run ExecTasks() for all event(s) -> Run EndOfRunTasks() -> Write trees, parameters, headers -> EndOfRun()
         *   >> Run Exec() for one event (will not write any object)
         */
        void Run(Long64_t numEvents = -1); ///< Run number of events numEvents from the first point
        void Run(Long64_t startID, Long64_t endID); ///< Run in range: from startID to endID
        bool RunEvent(Long64_t eventID=-2); ///< Run event of eventID
        bool RunSelectedEvent(TString selection); ///< Find event that matches the given selection and run. The selection is set from first call of RunSelectedEvent.
        void RunOnline();
        //void RunSplit(Long64_t eventID);

        /**
         * Run ExecTasks() for single event. This will not write any object.
         * In general Run() should be used instead of Event(). See descriptions in Run() for detail.
         * If you want to write trees after using Event() method, 
         */
        //void Event(Long64_t eventID); ///< Run single event

        bool StartOfRun(Long64_t numEvents = -1);
        void ClearArrays();
        bool EndOfRun();

        void WriteExitLog(TString path);
        bool IsCleanExit() { return fCleanExit; }

        //void EventMessage(const char *message);

        void SignalEndOfRun() { fSignalEndOfRun = true; }
        void SetAutoEndOfRun(Bool_t val) { fAutoEndOfRun = val; }

        void SetAutoTermination(Bool_t val) { fAutoTerminate = val; }
        void Terminate(TObject *obj, TString message = "");

        TString GetFileHash(TString name);
        static TString ConfigureDataPath(TString name, bool search = false, TString pathData="", bool addVersion=false);
        static bool CheckFileExistence(TString fileName);

        bool ExecuteEvent(Long64_t eventID=-1); ///< Run just one event of eventID.
        bool ExecuteNextEvent() { return ExecuteEvent(-3); }
        bool ExecutePreviousEvent() { return ExecuteEvent(-4); }
        bool ExecuteFirstEvent() { return ExecuteEvent(0); }
        bool ExecuteLastEvent() { return ExecuteEvent(-5); }
        void ExecuteEveTasks();

        bool CheckMute() { return (fEventCount==0||fEventCount%fEventCountForMessage!=0); }
        void DoNotFillCurrentEvent() { fFillCurrentEvent = false; }

        /// Search input files with given LKRun/RunID.
        /// Search and return array of matching files -> run_runNo*.[tag].root
        vector<TString> SearchRunFiles(int runNo, TString tag);

    protected:
        void ProcessWriteExitLog();

    private:
        bool fRunNameIsSet = false;
        TString fRunName = "run";
        Int_t   fRunID = -1;
        Int_t   fDivision = -1;

        bool fRunInit = false;
        bool fInitialized = false;
        bool fErrorInputFile = false;

        TString fOutputPath = "";
        TString fInputVersion = "";
        TString fInputFileName = "";
        TString fInputTreeName = "";
        TFile *fInputFile = nullptr;
        TChain *fInputTree = nullptr;
        std::vector<TString> fInputFileNameArray;
        std::vector<TString> fInputPathArray;
        TString fSearchOption = "";

        TString fParAddAfter = "";

        Int_t fNumFriends = 0;
        TObjArray *fFriendTrees = nullptr;
        vector<TString> fFriendFileNameArray;

        TString fOutputFileName = "";
        bool fAddVersion = false;
        TString fOuputHash = "";
        TString fTag = "";
        TString fTagInput = "";
        Int_t fSplit = -1;
        Int_t fSplitInput = -1;
        Long64_t fNumEventsInSplit = -1;
        TFile *fOutputFile = nullptr;
        TTree *fOutputTree = nullptr;

        TObjArray *fPersistentBranchArray = nullptr;
        TObjArray *fTemporaryBranchArray = nullptr;
        TObjArray *fInputTreeBranchArray = nullptr;

        Int_t fCountBranches = 0;
        TClonesArray **fBranchPtr;
        std::vector<TString> fBranchNames;
        std::map<TString, TClonesArray*> fBranchPtrMap;

        Int_t fCountRunObjects = 0;
        TObject **fRunObjectPtr;
        TString fRunObjectName[20];
        std::map<TString, TObject*> fRunObjectPtrMap;

        Long64_t fNumEntries = 0;

        //TTreeFormula* fSelect = nullptr;
        //Long64_t fCurrentEventIDForSelection = 0;

        Int_t fTreeNumber = -1;
        Long64_t fPrevSelEventID = 0;
        Long64_t fCurrSelEventID = 0;
        TEntryList *fSelEntryList = nullptr;
        TString fSelectionString;
        Long64_t fNumSelEntries;

        //std::vector<const char *> fEventMessage;

        Long64_t fIdxEntry = 0;
        Long64_t fStartEventID = -1;
        Long64_t fEndEventID = -1;
        Long64_t fCurrentEventID = 0;
        Long64_t fEventCount = 0;
        Long64_t fNumRunEntries = 0;
        Long64_t fEventCountForMessage = 0;
        Long64_t fNumPrintMessage = 20;
        bool fRunHasStarted = false;
        bool fCleanExit = false;
        bool fSignalEndOfRun = false;

        LKParameterContainer *fRunHeader = nullptr;
        LKParameterContainer *fG4ProcessTable = nullptr; ///< List of Geant4 physics process
        LKParameterContainer *fG4SDTable = nullptr;      ///< List of Geant4 sensitive detectors
        LKParameterContainer *fG4VolumeTable = nullptr;
        LKDetectorSystem *fDetectorSystem = nullptr;

        std::vector<TString> fListOfGitBranches;
        std::vector<Int_t> fListOfNumTagsInGitBranches;
        std::vector<TString> fListOfGitHashTags; ///<@todo
        std::vector<TString> fListOfVersionMarks; ///<@todo

        Bool_t fAutoEndOfRun = true;
        Bool_t fAutoTerminate = true;

        LKTask* fEventTrigger = nullptr;
        bool fUsingEventTrigger = false;

        bool fFillCurrentEvent = true;

        TString fExitLogPath;

        TTask *fEveTask = nullptr;

    private:
        static LKRun *fInstance;

    ClassDef(LKRun, 1)
};

#endif
