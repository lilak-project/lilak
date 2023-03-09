#ifndef LKRUN_HH
#define LKRUN_HH

#include "LKCompiled.h"
#include "LKLogger.hh"
#include "LKTask.hh"
#include "LKParameterContainer.hh"
#ifdef LKDETCTORSYSTEM_HH
#include "LKDetectorSystem.hh"
#include "LKDetector.hh"
#endif

#include "TDatabasePDG.h"
#include "TError.h"
#include "TH1D.h"
#include "TGraph.h"
#include "TFile.h"
#include "TTree.h"
#include "TChain.h"
#include "TObject.h"
#include "TClonesArray.h"
#include "TCanvas.h"

#include <map>
#include <vector>
#include <fstream>
#include <stdlib.h>
using namespace std;

#include "TSysEvtHandler.h"

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

    LKRun(); ///< Do not use this constructor. Use GetRun() only.
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

    /// Run name/id
    void SetRunName(TString name, Int_t id=-1); ///< Set Run name and id.
    TString GetRunName() const { return fRunName; }
    Int_t GetRunID() const { return fRunID; }

    /// Data path
    void SetDataPath(TString path) { fDataPath = path; } ///< Set data directory path. Default directory : path/to/LILAK/data
    TString GetDataPath() { return fDataPath; }

    /// Input file
    void AddInputFile(TString fileName, TString treeName = "event"); ///< Add file to input file
    void SetInputTreeName(TString treeName) { fInputTreeName = treeName; } ///< Set input tree name
    TFile  *GetInputFile() { return fInputFile; }
    TTree  *GetInputTree() const { return (TTree *) fInputTree; }
    TChain *GetInputChain() const { return fInputTree; }

    /// Output file
    void SetOutputFile(TString name, bool addVersion=false) { fOutputFileName = name; fAddVersion = addVersion; }
    TFile* GetOutputFile() { return fOutputFile; }
    TTree* GetOutputTree() { return fOutputTree; }
    void SetTag(TString tag) { fTag = tag; }
    void SetSplit(Int_t split, Long64_t numSplitEntries) { fSplit = split; fNumSplitEntries = numSplitEntries; }

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

    void CollectParFromInit(TString name); ///< Collect parameters from Init() methods using LKParameterContainer collecting mode

    /**
     * Register obj as a output branch with given name.
     * obj will not be registered if same name already exist in the branch list and return fail.
     * Object(obj) is recommanded to be TClones array
     * ex) dataArray = new TClonesArray("TDataClass",1000); // 1000, for example should be around expected maximum number of "data"
     * If persistent is true, branch will write it's data to output tree.
     * If persistent is false, branch will not be written. However the data is accessible during the run
     */
    bool RegisterBranch(TString name, TObject *obj, bool persistent);
    TObject *GetBranch(TString name); ///< Get branch in TObject by name.
    TClonesArray *GetBranchA(TString name); ///< Get branch in TClonesArray by name. Return nullptr if branch is not inherited from TClonesArray

#ifdef LKDETCTORSYSTEM_HH
    void AddDetector(LKDetector *detector) { fDetectorSystem -> AddDetector(detector); } ///< Add detector
    LKDetector *GetDetector(Int_t i) const { return (LKDetector *) fDetectorSystem -> At(i); }
    LKDetectorSystem *GetDetectorSystem() const { return fDetectorSystem; }

    void SetGeoManager(TGeoManager *gm) { fDetectorSystem -> SetGeoManager(gm); }
    TGeoManager *GetGeoManager() const { return fDetectorSystem -> GetGeoManager(); }
    void SetGeoTransparency(Int_t transparency) { fDetectorSystem -> SetTransparency(transparency); }
#endif

    void SetEntries(Long64_t num) { fNumEntries = num; } ///< Set total number of entries. Use only input do not exist.
    Long64_t GetEntries() const { return fNumEntries; } ///< Get total number of entries
    /// GetEntry current from input tree. For options 
    /// For options see TTree::GetEntry : https://root.cern.ch/doc/master/classTTree.html#a14c88179bd5fd2116228707d6addea9f
    Int_t GetEntry(Long64_t entry = 0, Int_t getall = 0);

    void SetNumEvents(Long64_t num) { SetEntries(num); } ///< Equavalent to SetEntries
    Long64_t GetNumEvents() const { return GetNumEvents(); } ///< Equavalent to GetNumEvents
    Int_t GetEvent(Long64_t entry) { return GetEntry(entry); } ///< Equavalent to GetEntry
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
    void RunEvent(Long64_t eventID); ///< Run just one event of eventID.
    //void RuSplit(Long64_t eventID);

    /**
     * Run ExecTasks() for single event. This will not write any object.
     * In general Run() should be used instead of Event(). See descriptions in Run() for detail.
     * If you want to write trees after using Event() method, 
     */
    void Event(Long64_t eventID); ///< Run single event 
    void NextEvent() { Event(fCurrentEventID+1); } ///< Run next event (will not write any object)

    void SignalEndOfRun() { fSignalEndOfRun = true; }
    void SetAutoTermination(Bool_t val) { fAutoTerminate = val; }

    void Terminate(TObject *obj, TString message = "");

    TString GetFileHash(TString name);
    static TString ConfigureDataPath(TString name, bool search = false, TString pathData="", bool addVersion=false);
    static bool CheckFileExistence(TString fileName);

    TDatabasePDG *GetDatabasePDG();
    TParticlePDG *GetParticle(Int_t pdg)        { return GetDatabasePDG() -> GetParticle(pdg); }
    TParticlePDG *GetParticle(const char *name) { return GetDatabasePDG() -> GetParticle(name); }

  private:
    void CheckIn();
    void CheckOut();

  private:
    TString fRunName = "";
    Int_t   fRunID = -1;

    bool fInitialized = false;

    TString fDataPath = "";
    TString fInputVersion = "";
    TString fInputFileName = "";
    TString fInputTreeName = "";
    TFile *fInputFile = nullptr;
    TChain *fInputTree = nullptr;

    std::vector<TString> fInputFileNameArray;

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

    Int_t fNumBranches = 0;
    TObject **fBranchPtr;
    std::vector<TString> fBranchNames;
    std::map<TString, TObject*> fBranchPtrMap;

    Long64_t fNumEntries = 0;


    Long64_t fIdxEntry = 0;
    Long64_t fStartEventID = -1;
    Long64_t fEndEventID = -1;
    Long64_t fCurrentEventID = 0;
    Long64_t fEventCount = 0;
    bool fSignalEndOfRun = false;
    bool fCheckIn = false;

    LKParameterContainer *fRunHeader = nullptr;
    LKParameterContainer *fG4ProcessTable = nullptr; ///< List of Geant4 physics process
    LKParameterContainer *fG4SDTable = nullptr;      ///< List of Geant4 sensitive detectors
    LKParameterContainer *fG4VolumeTable = nullptr;

#ifdef LKDETCTORSYSTEM_HH
    LKDetectorSystem *fDetectorSystem = nullptr;
#endif

    std::vector<TString> fListOfGitBranches;
    std::vector<int> fListOfNumTagsInGitBranches;
    std::vector<TString> fListOfGitHashTags; ///<@todo
    std::vector<TString> fListOfVersionMarks; ///<@todo

    Bool_t fAutoTerminate = true;

  private:
    static LKRun *fInstance;

  ClassDef(LKRun, 1)
};

#endif
