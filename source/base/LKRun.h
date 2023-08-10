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

        LKRun();
        virtual ~LKRun() {};

        static void PrintLILAK();              ///< Print compiled LILAK information

        /**
         * ## options
         * - all : Print all (below)
         * - gen : Print general information
         * - par : Print parameter information
         * - out : Print output information
         * - in  : Print input information
         * - det : Print detector information
         */
        virtual void Print(Option_t *option="all") const;
        virtual void Add(TTask *task);

        void PrintEvent(Long64_t entry);

        /// Run name/id
        /// This is equivalent to setting parameter
        /// LKRun/RunName [name] [id] [tag(optional)] [split(optional)].
        void SetRunName(TString name, Int_t id=0, TString tag=""); ///< Set Run name and id.
        const char* GetRunName() const { return fRunName.Data(); }
        Int_t GetRunID() const { return fRunID; }

        /// Set data directory path. Default directory : path/to/LILAK/data
        /// Data path will not be used if full path of the input/ouput file is given.
        /// This is equivalent to setting parameter LKRun/DataPath.
        /// Hoever SetDataPath method has the higher priority over setting parameter
        void SetDataPath(TString path) { fDataPath = path; }
        TString GetDataPath() { return fDataPath; }

        bool ConfigureRunFromFileName(TString inputName);
        TString ConfigureFileName();

        /// Input file
        void AddInputList(TString listName, TString treeName = "event"); ///< Add file to input file
        void AddInputFile(TString fileName, TString treeName = "event"); ///< Add file to input file
        void SetInputFile(TString fileName, TString treeName = "event") { return AddInputFile(fileName, treeName); } ///< Add file to input file
        void SetInputTreeName(TString treeName) { fInputTreeName = treeName; } ///< Set input tree name
        TFile  *GetInputFile() { return fInputFile; }
        TTree  *GetInputTree() const { return (TTree *) fInputTree; }
        TChain *GetInputChain() const { return fInputTree; }

        void AddFriend(TString fileName); ///< Add file to input file
        TChain *GetFriendChain(Int_t iFriend) const;

        /// Output file
        void SetOutputFile(TString name, bool addVersion=false) { fOutputFileName = name; fAddVersion = addVersion; }
        TFile* GetOutputFile() { return fOutputFile; }
        TTree* GetOutputTree() { return fOutputTree; }
        void SetTag(TString tag) { fTag = tag; }
        void SetSplit(Int_t split, Long64_t numSplitEntries) { fSplit = split; fNumSplitEntries = numSplitEntries; }

        void SetNumPrintMessage(int num) { fNumPrintMessage = num; }

        /**
         * Initailize LKRun.
         * Configure input and output files(trees and branches), input parameters and detectors
         * Init() must be done before Run().
         * Init
         */
        bool Init();

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
         * One can se persistency using the parameter [branchName]/persistency
         */
        bool RegisterBranch(TString name, TObject *obj, bool persistent=true);

        /**
         * Register obj and write directly to ouput file
         */
        bool RegisterObject(TString name, TObject *obj);

        /**
         * Create, register and return TClonesArray object.
         */
        TClonesArray* RegisterBranchA(TString name, const char* className, Int_t size=100, bool persistent=true);

        TString GetBranchName(int idx) const;
        TObject *GetBranch(TString name); ///< Get branch in TObject by name.
        TObject *GetBranch(int idx);
        TObject *KeepBranch(TString name);
        TClonesArray *GetBranchA(TString name); ///< Get branch in TClonesArray by name. Return nullptr if branch is not inherited from TClonesArray
        TClonesArray *GetBranchA(int idx);
        TClonesArray *KeepBranchA(TString name);
        int GetNumBranches() const { return fCountBranches; }

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
         * # Difference between Run and Event methods
         * - Run(nid), Run(id1,id2) and RunEvent(id):
         *   >> CheckIn() -> Run ExecTasks() for all event(s) -> Run EndOfRunTasks() -> Write trees, parameters, headers -> CheckOut()
         * - Event(id), NextEvent():
         *   >> Run Exec() for one event (will not write any object)
         */
        void Run(Long64_t numEvents = -1);
        void Run(Long64_t startID, Long64_t endID); ///< Run in range from startID to endID
        bool RunEvent(Long64_t eventID=-1); ///< Run just one event of eventID.
        bool RunNextEvent() { return RunEvent(-2); }
        //void RuSplit(Long64_t eventID);

        /**
         * Run ExecTasks() for single event. This will not write any object.
         * In general Run() should be used instead of Event(). See descriptions in Run() for detail.
         * If you want to write trees after using Event() method, 
         */
        //void Event(Long64_t eventID); ///< Run single event

        bool StartOfRun(Long64_t numEvents = -1);
        void ClearArrays();
        bool EndOfRun();

        //void EventMessage(const char *message);

        void SignalEndOfRun() { fSignalEndOfRun = true; }
        void SetAutoEndOfRun(Bool_t val) { fAutoEndOfRun = val; }

        void SetAutoTermination(Bool_t val) { fAutoTerminate = val; }
        void Terminate(TObject *obj, TString message = "");

        TString GetFileHash(TString name);
        static TString ConfigureDataPath(TString name, bool search = false, TString pathData="", bool addVersion=false);
        static bool CheckFileExistence(TString fileName);

    private:
        void CheckIn();
        void CheckOut();

    private:
        bool fRunNameIsSet = false;
        TString fRunName = "run";
        Int_t   fRunID = 0;

        bool fInitialized = false;

        TString fDataPath = "";
        TString fInputVersion = "";
        TString fInputFileName = "";
        TString fInputTreeName = "";
        TFile *fInputFile = nullptr;
        TChain *fInputTree = nullptr;
        std::vector<TString> fInputFileNameArray;

        Int_t fNumFriends = 0;
        TObjArray *fFriendTrees = nullptr;
        vector<TString> fFriendFileNameArray;

        TString fOutputFileName = "";
        bool fAddVersion = false;
        TString fOuputHash = "";
        TString fTag = "";
        Int_t fSplit = -1;
        Long64_t fNumSplitEntries = -1;
        TFile *fOutputFile = nullptr;
        TTree *fOutputTree = nullptr;

        TObjArray *fPersistentBranchArray = nullptr;
        TObjArray *fTemporaryBranchArray = nullptr;
        TObjArray *fInputTreeBranchArray = nullptr;

        Int_t fCountBranches = 0;
        TObject **fBranchPtr;
        std::vector<TString> fBranchNames;
        std::map<TString, TObject*> fBranchPtrMap;

        Int_t fCountRunObjects = 0;
        TObject **fRunObjectPtr;
        TString fRunObjectName[20];
        std::map<TString, TObject*> fRunObjectPtrMap;

        Long64_t fNumEntries = 0;

        //std::vector<const char *> fEventMessage;

        Long64_t fIdxEntry = 0;
        Long64_t fStartEventID = -1;
        Long64_t fEndEventID = -1;
        Long64_t fCurrentEventID = 0;
        Long64_t fEventCount = 0;
        Long64_t fNumRunEntries = 0;
        Long64_t fNumSkipEventsForMessage = 0;
        Int_t fNumPrintMessage = 20;
        bool fSignalEndOfRun = false;
        bool fCheckIn = false;

        LKParameterContainer *fRunHeader = nullptr;
        LKParameterContainer *fG4ProcessTable = nullptr; ///< List of Geant4 physics process
        LKParameterContainer *fG4SDTable = nullptr;      ///< List of Geant4 sensitive detectors
        LKParameterContainer *fG4VolumeTable = nullptr;

        LKDetectorSystem *fDetectorSystem = nullptr;

        std::vector<TString> fListOfGitBranches;
        std::vector<int> fListOfNumTagsInGitBranches;
        std::vector<TString> fListOfGitHashTags; ///<@todo
        std::vector<TString> fListOfVersionMarks; ///<@todo

        Bool_t fAutoEndOfRun = true;
        Bool_t fAutoTerminate = true;

    private:
        static LKRun *fInstance;

    ClassDef(LKRun, 1)
};

#endif
