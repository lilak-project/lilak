#include "globals.hh"
#include "G4UImanager.hh"
//#include "G4GDMLParser.hh"
#include "G4UIExecutive.hh"
#include "G4strstreambuf.hh"
#include "G4VisExecutive.hh"
#include "G4ProcessTable.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4SolidStore.hh"
#include "G4RegionStore.hh"
//#include "G4MaterialStore.hh"
#include "Randomize.hh"

#include "TRandom.h"
#include "TSystem.h"
#include "TObjString.h"

#include "LKCompiled.h"
#include "LKEventAction.h"
#include "LKG4RunManager.h"
#include "LKG4RunMessenger.h"
#include "LKTrackingAction.h"
#include "LKStackingAction.h"
#include "LKSteppingAction.h"
#include "LKPrimaryGeneratorAction.h"
#include "LKG4DetectorConstructionFactory.h"

#include <cmath>

LKG4RunManager::LKG4RunManager()
:G4RunManager()
{
    new LKG4RunMessenger(this);

    fVolumes = new LKParameterContainer();
    fVolumes -> SetName("Volumes");

    fSensitiveDetectors = new LKParameterContainer();
    fSensitiveDetectors -> SetName("SensitiveDetectors");

    fProcessTable = new LKParameterContainer();
    fProcessTable -> SetName("ProcessTable");
}

LKG4RunManager::~LKG4RunManager()
{
}

void LKG4RunManager::AddParameterContainer(TString fname)
{
    LKGear::AddParameterContainer(fname);

    fPar -> Require("collect_par", "collected_parameters", "set this parameter if user wants to collect parameters", "/");

    if (fPar -> CheckPar("collect_par")) {
        fCollecteParAndPrintTo = fPar -> GetParString("collect_par");
        if (fCollecteParAndPrintTo.IsNull()) fCollecteParAndPrintTo = "collected_geant4_sim";
        fPar -> SetCollectParameters(true);
    }

    fUseVisMode           = fPar -> InitPar(false, "LKG4Manager/VisMode ?? # set true to use geant4 visual mode");
    fSuppressInitMessage  = fPar -> InitPar(true,  "LKG4Manager/SuppressG4InitMessage ?? # set true to suppress the long initial messages from geant4");
    fSetAutoUpdateCopyNo  = fPar -> InitPar(true,  "LKG4Manager/SetAutoUpdateCopyNo ?? # If true, manager will automatically assign unique CopyNo to sensitive physical volumes");
    fG4CommandFileName    = fPar -> InitPar("",    "LKG4Manager/G4CommandFile # geant4 macro");
    fOutputFileName       = fPar -> InitPar("g4output.root","LKG4Manager/G4OutputFile ?? # name of the output file this will contain branches of TClonesArray defined by LILAK containers");
    fRandomSeed           = fPar -> InitPar((Long64_t)time(0),"LKG4Manager/RandomSeed ?? # set random seed to root and geant4 random generator. Random seed will be time(0) if this parameter is not set");
    fSDNames              = fPar -> InitPar(std::vector<TString>{},"LKG4Manager/SensitiveDetectors ?? # not really used at the moment");
    fGeneratorFileName    = fPar -> InitPar(TString(), "LKG4Manager/PGAGeneratorFile ?? # Generator file");
    fDetectorConstructionName = fPar -> InitPar("", "LKG4Manager/DetectorCosntruction ?? # Name of the geant4 DC (Detector Construction) name. To add DC through this parameter, user must place DC-class either in geant4 or nptool in project-directory. Class name (which should be same as the class file name) must contain \"DetectorConstruction\" or \"DC\".");
    fWriteTextFile = fPar -> InitPar(false, "LKG4Manager/WriteTextFile ??");

    fStepPersistency        = fPar -> InitPar(true,"persistency/MCStep"       );
    fEdepSumPersistency     = fPar -> InitPar(true,"persistency/MCEdepSum"    );
    fSecondaryPersistency   = fPar -> InitPar(true,"persistency/MCSecondary"  );
    fTrackVertexPersistency = fPar -> InitPar(true,"persistency/MCTrackVertex");
}

void LKG4RunManager::Initialize()
{
    fTryInitialization = true;
    InitHead();
    InitPreAddActions();

    G4RunManager::Initialize();

    InitPostAddActions();
    InitGeneratorFile();
    InitOutputFile();
    InitSummary();
    fInitialized = true;
}

void LKG4RunManager::InitPreAddActions()
{
    if (GetUserDetectorConstruction() == nullptr)
    {
        g4man_warning << "No user detector construction is set!" << endl;
        LKG4DetectorConstructionFactory dcf;
        if (!fDetectorConstructionName.IsNull()) {
            g4man_info << "Adding " << fDetectorConstructionName << endl;
            SetUserInitialization(dcf.GetDetectorConstruction(fDetectorConstructionName));
        }
        else {
            g4man_info << "Adding LKDetectorConstruction " << endl;
            SetUserInitialization(dcf.GetDetectorConstruction("LKDetectorConstruction"));
        }
    }
    if (GetUserPrimaryGeneratorAction() == nullptr) { g4man_info << "Adding LKPrimaryGeneratorAction " << endl; SetUserAction(new LKPrimaryGeneratorAction()); }
    if (GetUserEventAction()    == nullptr) { g4man_info << "Adding LKEventAction"    << endl; SetUserAction(new LKEventAction   (this)); }
    if (GetUserTrackingAction() == nullptr) { g4man_info << "Adding LKTrackingAction" << endl; SetUserAction(new LKTrackingAction(this)); }
    if (GetUserStackingAction() == nullptr) { g4man_info << "Adding LKStackingAction" << endl; SetUserAction(new LKStackingAction(this)); }
    if (GetUserSteppingAction() == nullptr) { g4man_info << "Adding LKSteppingAction" << endl; SetUserAction(new LKSteppingAction(this)); }
}

void LKG4RunManager::InitHead()
{
    g4man_info << "Setting random with seed " << fRandomSeed << endl;
    gRandom -> SetSeed(fRandomSeed);
    G4Random::setTheSeed(fRandomSeed);
    G4Random::setTheEngine(new CLHEP::RanecuEngine);

    if (fSDNames.size()>0)
    {
        g4man_info << "Setting sensitive detectors" << endl;
        for (auto sdName : fSDNames) {
            g4man_info << "    " << sdName << endl;
            if (sdName.Index("!")!=0) {
                sdName.ReplaceAll("!","");
                fLFAssemblySD.push_back(sdName);
            }
            else
                fLFSingleSD.push_back(sdName);
        }
    }
}

void LKG4RunManager::InitSummary()
{
    auto procNames = G4ProcessTable::GetProcessTable() -> GetNameList();
    Int_t idx = 0;
    fProcessTable -> AddPar("Primary", idx++);
    for (auto name : *procNames)
        if (fProcessTable -> CheckPar(name) == false)
            fProcessTable -> AddPar(name, idx++);

    if (!fCollecteParAndPrintTo.IsNull()) {
        fPar -> PrintCollection(fCollecteParAndPrintTo);
        fPar -> SetCollectParameters(false);
    }

    g4man_info << "End of initialization" << endl;
}

void LKG4RunManager::InitializeGeometry()
{
    g4man_info << "InitializeGeometry" << endl;
    if (fSuppressInitMessage) {
        G4strstreambuf* suppressMessage = dynamic_cast<G4strstreambuf*>(G4cout.rdbuf(0));
        // Suppress print outs in between here ------------->
        G4RunManager::InitializeGeometry();
        // <------------- to here
        G4cout.rdbuf(suppressMessage);
    }
    else
        G4RunManager::InitializeGeometry();
    g4man_info << "InitializeGeometry" << endl;
}

void LKG4RunManager::InitializePhysics()
{
    g4man_info << "InitializePhysics" << endl;
    if (fSuppressInitMessage) {
        G4strstreambuf* suppressMessage = dynamic_cast<G4strstreambuf*>(G4cout.rdbuf(0));
        // Suppress print outs in between here ------------->
        G4RunManager::InitializePhysics();
        // <------------- to here
        G4cout.rdbuf(suppressMessage);
    }
    else
        G4RunManager::InitializePhysics();
    g4man_info << "InitializePhysics" << endl;
}

void LKG4RunManager::Run(G4int argc, char **argv, const G4String &type)
{
    if (fTryInitialization==false) {
        g4man_info << "Run manager is not inialized." << endl;
        g4man_info << "Running Initialize() ..." << endl;
        Initialize();
    }

    G4cout << endl;

    G4cout << "==== List of LogicalVolume ====" << G4endl;
    for (const auto& lv : *G4LogicalVolumeStore::GetInstance()) {
        TString regionName;
        if (lv->GetRegion()!=nullptr)
            regionName = lv -> GetRegion() -> GetName();
        G4cout << lv->GetName() << " : material=" << lv->GetMaterial()->GetName() << " region=" << regionName << G4endl;
    }
    G4cout << endl;

    G4cout << "==== List of PhysicalVolume ====" << G4endl;
    for (const auto& pv : *G4PhysicalVolumeStore::GetInstance()) {
        TString mlgName;
        if (pv->GetMotherLogical()!=nullptr)
            mlgName = pv -> GetMotherLogical() -> GetName();
        G4cout << pv->GetName() << " (" << pv->GetCopyNo() << ") : mother_lg=" << mlgName << G4endl;
    }
    G4cout << endl;

    G4cout << "==== List of Solid ====" << G4endl;
    for (const auto& solid : *G4SolidStore::GetInstance()) G4cout << solid->GetName() << G4endl;
    G4cout << endl;

    G4cout << "==== List of Region ====" << G4endl;
    for (const auto& region : *G4RegionStore::GetInstance()) G4cout << region->GetName() << G4endl;
    G4cout << endl;

    //G4cout << "==== List of Material ====" << G4endl;
    //for (const auto& material : *G4MaterialStore::GetInstance()) G4cout << material->GetName() << G4endl;
    //G4cout << endl;

    if (fInitialized==false) {
        g4man_error << "Unable to initialize." << endl;
        return;
    }

    G4UImanager* uiManager = G4UImanager::GetUIpointer();

    LKParameterContainer* g4CommandContainer = nullptr;
    g4CommandContainer = new LKParameterContainer();
    if (fG4CommandFileName.IsNull()==false) {
        g4man_info << "Adding geant4 command macro " << fG4CommandFileName << endl;
        // TODO
        int count_parameters = g4CommandContainer -> AddFile(fG4CommandFileName);
        if (count_parameters==0)
            count_parameters = g4CommandContainer -> AddFile(TString(LILAK_PATH)+"/common/"+fG4CommandFileName);
    }
    auto g4CommandContainer2 = fPar -> CreateGroupContainer("G4");
    if (g4CommandContainer2->GetEntries()>0) {
        g4man_info << "Adding geant4 commands from G4/ group" << endl;
        g4CommandContainer -> AddParameterContainer(g4CommandContainer2);
    }

    g4man_info << "Input G4 commands" << endl;
    //g4CommandContainer -> Print();

    TIter next(g4CommandContainer);

    LKParameter *parameter = nullptr;
    if (1)
    {
        G4VisManager* visManager = nullptr;
        G4UIExecutive* uiExecutive = nullptr;
        bool startedVisMode = false;
        while ((parameter = (LKParameter*) next()))
        {
            TString command = parameter -> GetLine();
            if (command[0]=='#') continue;
            if (command[0]!='/') command = Form("/%s",command.Data());
            if (command.Index("/vis/open")==0 && startedVisMode==false && fUseVisMode)
            {
                visManager = new G4VisExecutive;
                visManager -> Initialize();
                uiExecutive = new G4UIExecutive(argc,argv,type);
                startedVisMode = true;
            }
            g4man_info << command << endl;
            uiManager -> ApplyCommand(command);
        }
        if (startedVisMode) {
            uiExecutive -> SessionStart();
            delete uiExecutive;
            delete visManager;
        }
    }
    else
    {
        if (fUseVisMode) {
            G4VisManager* visManager = new G4VisExecutive;
            visManager -> Initialize();

            G4UIExecutive* uiExecutive = new G4UIExecutive(argc,argv,type);
            while ((parameter = (LKParameter*) next())) {
                TString command = parameter -> GetLine();
                if (command[0]=='#') continue;
                if (command[0]!='/') command = Form("/%s",command.Data());
                g4man_info << command << endl;
                uiManager -> ApplyCommand(command);
            }
            uiExecutive -> SessionStart();

            delete uiExecutive;
            delete visManager;
        }
        else {
            while ((parameter = (LKParameter*) next()))
            {
                TString command = parameter -> GetLine();
                if (command[0]=='#') continue;
                if (command[0]!='/') command = Form("/%s",command.Data());
                g4man_info << command << endl;
                uiManager -> ApplyCommand(command);
            }
        }
    }

    WriteToFile(fProcessTable);
    WriteToFile(fSensitiveDetectors);
    WriteToFile(fVolumes);
    EndOfRun();
}

void LKG4RunManager::BeamOnAll()
{
    g4man_info << "BeamOnAll " << fNumEvents << endl;
    BeamOn(fNumEvents);
}

void LKG4RunManager::BeamOn(G4int numEvents, const char *macroFile, G4int numSelect)
{
    g4man_info << "BeamOn" << endl;
    if(numEvents<=0) { fakeRun = true; }
    else { fakeRun = false; }
    G4bool cond = ConfirmBeamOnCondition();
    g4man_info << "BeamOn condition: " << cond << endl;
    if (cond)
    {
        numberOfEventToBeProcessed = numEvents;
        numberOfEventProcessed = 0;
        ConstructScoringWorlds();

        if (fSuppressInitMessage) {
            G4strstreambuf* suppressMessage = dynamic_cast<G4strstreambuf*>(G4cout.rdbuf(0));
            // Suppress print outs in between here ------------->
            RunInitialization();
            // <------------- to here
            G4cout.rdbuf(suppressMessage);
        }
        else
            RunInitialization();

        g4man_info << "DoEventLoop" << endl;
        //g4man_info << numEvents << " " << macroFile << " " << numSelect << endl;
        DoEventLoop(numEvents,macroFile,numSelect);
        g4man_info << "RunTermination" << endl;
        RunTermination();
    }
    fakeRun = false;
}

void LKG4RunManager::SetSuppressInitMessage(bool val) { fSuppressInitMessage = val; }

bool LKG4RunManager::InitGeneratorFile()
{
    if (fGeneratorFileName.IsNull()) {
        g4man_error << "Event generator is not set!" << endl;
        return false;
    }
    auto pga = (LKPrimaryGeneratorAction *) userPrimaryGeneratorAction;
    if (!pga -> SetEventGenerator(fGeneratorFileName.Data())) {
        g4man_error << "Event generator cannot be initialized!" << endl;
        return false;
    }
    return true;
}

void LKG4RunManager::InitOutputFile()
{
    //fPar -> ReplaceEnvironmentVariable(fOutputFileName);

    //if (fPar -> CheckPar("MCStep/persistency"       )) { fStepPersistency        = fPar -> GetParBool("MCStep/persistency"       ); }
    //if (fPar -> CheckPar("MCEdepSum/persistency"    )) { fEdepSumPersistency     = fPar -> GetParBool("MCEdepSum/persistency"    ); }
    //if (fPar -> CheckPar("MCSecondary/persistency"  )) { fSecondaryPersistency   = fPar -> GetParBool("MCSecondary/persistency"  ); }
    //if (fPar -> CheckPar("MCTrackVertex/persistency")) { fTrackVertexPersistency = fPar -> GetParBool("MCTrackVertex/persistency"); }

    //if (fPar -> CheckPar("persistency/MCStep"       )) { fStepPersistency        = fPar -> GetParBool("persistency/MCStep"       ); }
    //if (fPar -> CheckPar("persistency/MCEdepSum"    )) { fEdepSumPersistency     = fPar -> GetParBool("persistency/MCEdepSum"    ); }
    //if (fPar -> CheckPar("persistency/MCSecondary"  )) { fSecondaryPersistency   = fPar -> GetParBool("persistency/MCSecondary"  ); }
    //if (fPar -> CheckPar("persistency/MCTrackVertex")) { fTrackVertexPersistency = fPar -> GetParBool("persistency/MCTrackVertex"); }

    g4man_info << "Setting output file " << fOutputFileName << endl;
    fFile = new TFile(fOutputFileName,"recreate");
    fTree = new TTree("event", fOutputFileName);

    g4man_info << "Adding mctrack branch MCTrack" << endl;
    fTrackArray = new TClonesArray("LKMCTrack", 100);
    fTree -> Branch("MCTrack", &fTrackArray);

    fStepArrayList = new TObjArray();

    if (fStepPersistency)
    {
        g4man_info << "Step is persistent!" << endl;

        TIter itDetectors(fSensitiveDetectors);
        LKParameter *parameter;

        while ((parameter = dynamic_cast<LKParameter*>(itDetectors())))
        {
            TString detName = parameter -> GetName();
            Int_t copyNo = parameter -> GetInt();

            bool detectorPersistency = true;
            fPar -> UpdatePar(detectorPersistency,Form("persistency/%s true",detName.Data()));
            if (detectorPersistency==false)
                continue;

            if (detName.Index("_PARTOFASSEMBLY")>0) {
                g4man_info << "Detector " << detName << " is part of assembly" << endl;
                continue;
            }

            TString stepName = TString("MCStep") + detName;
            TString esumName = TString("MCESum") + detName;

            g4man_info << "Adding new step branch " << stepName << " for detector " << detName << " (" << copyNo << ")" << endl;

            auto stepArray = new TClonesArray("LKMCStep", 10000);
            fTree -> Branch(stepName, &stepArray);
            fStepArrayList -> Add(stepArray);
            fStepNameList.push_back(stepName);

            fListOfCopyNo.push_back(copyNo);
            fIdxOfCopyNo[copyNo] = fNumActiveVolumes;
            //g4man_info << "Adding new esum branch " << esumName << " for detector " << detName << endl;
            //fTree -> Branch(esumName, &fEdepSumArray[fNumActiveVolumes]); // TODO
            ++fNumActiveVolumes;
        }
    }
    else
        g4man_info << "Step is NOT persistent!" << endl;
}

void LKG4RunManager::SetVolume(G4VPhysicalVolume *physicalVolume)
{
    TString name = physicalVolume -> GetName().data();

    for (auto assembly : fLFAssemblySD) {
        assembly.ReplaceAll("!","");
        TObjArray *sdAssemblyNames = assembly.Tokenize("+");
        Int_t numsda = sdAssemblyNames -> GetEntries();
        if (numsda<2)
            continue;

        TString assemblyName = ((TObjString *) sdAssemblyNames  -> At(0)) -> GetString();
        for (auto isda=1; isda<numsda; ++isda) {
            TString partName = ((TObjString *) sdAssemblyNames  -> At(isda)) -> GetString();

            if (partName.Index("*")>=1) {
                partName.ReplaceAll("*","");
                if (name.Index(partName)>=0) {
                    SetSensitiveDetector(physicalVolume, assemblyName);
                    return;
                }
            }
            else {
                if (name == partName) {
                    SetSensitiveDetector(physicalVolume, assemblyName);
                    return;
                }
            }

        }
    }

    for (auto sdName : fLFSingleSD) {
        if (sdName.Index("*")>=1) {
            sdName.ReplaceAll("*","");
            if (name.Index(sdName)>=0) {
                SetSensitiveDetector(physicalVolume);
                return;
            }
        }
        else {
            if (name == sdName) {
                SetSensitiveDetector(physicalVolume);
                return;
            }
        }
    }

    Int_t copyNo = physicalVolume -> GetCopyNo();

    fVolumes -> AddPar(name, copyNo);
}

void LKG4RunManager::SetSensitiveDetector(G4VPhysicalVolume *physicalVolume, TString assemblyName)
{
    TString name = physicalVolume -> GetName().data();
    Int_t copyNo = physicalVolume -> GetCopyNo();

    if (fSetAutoUpdateCopyNo) {
        if (copyNo<6000) copyNo = 6000;
        if (copyNo<=fMaxCopyNo) copyNo = fMaxCopyNo + 1;
        physicalVolume -> SetCopyNo(copyNo);
    }

    if (assemblyName.IsNull()) {
        g4man_info << "New sensitive detector " << name << " (" << copyNo << ")" << endl;
        fSensitiveDetectors -> AddPar(name, copyNo);
    }
    else {
        Int_t assemblyID;
        if (fSensitiveDetectors -> CheckPar(assemblyName+"_ASSEMBLY")) {
            assemblyID = fSensitiveDetectors -> GetParInt(assemblyName+"_ASSEMBLY");
        }
        else {
            assemblyID = fNumSDAssemblyInTree;
            fSensitiveDetectors -> AddPar(assemblyName+"_ASSEMBLY",assemblyID);
            g4man_info << "New sensitive assembly " << assemblyName+"_ASSEMBLY" << " (" << assemblyID << ")" << endl;
            ++fNumSDAssemblyInTree;
        }
        fMapSDToAssembly[copyNo] = assemblyID;
        fSensitiveDetectors -> AddPar(name+"_PARTOFASSEMBLY",assemblyID);
        g4man_info << "New sensitive assembly part " << name+"_PARTOFASSEMBLY" << " (" << assemblyID << ")" << endl;
    }

    if (fMaxCopyNo<copyNo)
        fMaxCopyNo = copyNo;
}

LKParameterContainer *LKG4RunManager::GetVolumes() { return fVolumes; }
LKParameterContainer *LKG4RunManager::GetSensitiveDetectors() { return fSensitiveDetectors; }
LKParameterContainer *LKG4RunManager::GetProcessTable()       { return fProcessTable; }

void LKG4RunManager::AddMCTrack(Int_t trackID, Int_t parentID, Int_t pdg, Int_t detectorID, Int_t processID, Double_t vx, Double_t vy, Double_t vz, Double_t px, Double_t py, Double_t pz, Double_t energy)
{
    if (parentID != 0 && !fSecondaryPersistency) {
        fCurrentTrack = nullptr;
        return;
    }

    fTrackID = trackID;
    fCurrentTrack = (LKMCTrack *) fTrackArray -> ConstructedAt(fTrackArray -> GetEntriesFast());
    fCurrentTrack -> SetMCTrack(trackID, parentID, pdg, detectorID, processID, vx, vy, vz, px, py, pz, energy);
}

void LKG4RunManager::AddTrackVertex(Int_t detectorID, Int_t processID, Double_t vx, Double_t vy, Double_t vz, Double_t px, Double_t py, Double_t pz, Double_t energy)
{
    if (fCurrentTrack == nullptr || !fTrackVertexPersistency)
        return;

    fCurrentTrack -> AddVertex(detectorID, processID, vx, vy, vz, px, py, pz, energy);
}

void LKG4RunManager::AddMCStep(Int_t detectorID, Double_t x, Double_t y, Double_t z, Double_t t, Double_t e)
{
    bool isSensitiveDetector = false;
    for (auto id : fListOfCopyNo) {
        if (detectorID==id) {
            isSensitiveDetector = true;
            break;
        }
    }
    if (isSensitiveDetector==false)
        return;

    auto idx = fIdxOfCopyNo[detectorID];

    if (fEdepSumPersistency)
        fEdepSumArray[idx] = fEdepSumArray[idx] + e;

    if (fStepPersistency)
    {
        //auto stepArray = (TClonesArray *) fStepArrayList -> FindObject(Form("MCStep%d", detectorID));
        auto stepArray = (TClonesArray *) fStepArrayList -> At(idx);
        if (stepArray == nullptr)
            return;

        LKMCStep *step = (LKMCStep *) stepArray -> ConstructedAt(stepArray -> GetEntriesFast());
        step -> SetMCStep(fTrackID, x, y, z, t, e);
    }
}

void LKG4RunManager::SetNumEvents(Int_t numEvents)
{
    fNumEvents = numEvents;
}

void LKG4RunManager::NextEvent()
{
    g4man_info << "End of Event " << fTree -> GetEntries() << endl;
    fTree -> Fill();

    fTrackArray -> Clear("C");
    TIter it(fStepArrayList);

    if (fStepPersistency) {
        while (auto stepArray = (TClonesArray *) it.Next())
            stepArray -> Clear("C");
    }

    memset(fEdepSumArray, 0, sizeof(Double_t)*fNumActiveVolumes);
}

void LKG4RunManager::WriteToFile(TObject *obj)
{
    fFile -> cd();
    g4man_info << "Writing " << obj -> GetName() << " to output file" << endl;
    obj -> Write(obj->GetName(),TObject::kSingleKey);
}

void LKG4RunManager::EndOfRun()
{
    fFile -> cd();
    g4man_info << "Writing " << fTree -> GetName() << " to output file" << endl;
    fTree -> Write();
    g4man_info << "Writing " << fPar -> GetName() << " to output file" << endl;
    fPar -> Write(fPar->GetName(),TObject::kSingleKey);
    g4man_info << "Output: " << fFile -> GetName() << endl;

    if (fWriteTextFile)
        WriteTextFile();

    fFile -> Close();
}

void LKG4RunManager::WriteTextFile()
{
    if (fStepPersistency==false)
        return;

    TString infoFileName = fOutputFileName;
    TString textFileName = fOutputFileName;
    infoFileName.ReplaceAll(".root",".info");
    textFileName.ReplaceAll(".root",".dat");
    ofstream infoFile(infoFileName);
    ofstream textFile(textFileName);
    g4man_info << infoFileName << endl;
    g4man_info << textFileName << endl;

    auto numEvents = fTree -> GetEntries();
    auto numStepBs = fStepNameList.size();
    infoFile << "n_events   " << numEvents << endl;
    infoFile << "n_branches " << numStepBs << endl;
    for (auto iBranch=0; iBranch<numStepBs; ++iBranch)
    {
        auto array = fStepArrayList -> At(iBranch);
        auto name = fStepNameList.at(iBranch);
        infoFile << "branch " << iBranch << " " << name << endl;
    }
    infoFile << "event\tbranch_id\tstep_id\ttrack_id\tx\ty\tz\tt\te" << endl;
    for (auto iEvent=0; iEvent<numEvents; ++iEvent)
    {
        fTree -> GetEntry(iEvent);
        for (auto iBranch=0; iBranch<numStepBs; ++iBranch)
        {
            auto array = (TClonesArray*) fStepArrayList -> At(iBranch);
            auto name = fStepNameList.at(iBranch);
            auto numSteps = array -> GetEntries();
            for (auto iStep=0; iStep<numSteps; ++iStep)
            {
                auto step = (LKMCStep*) array -> At(iStep);
                auto trackID = step -> GetTrackID();
                double x = step -> GetX();
                double y = step -> GetY();
                double z = step -> GetZ();
                double t = step -> GetTime();
                double e = step -> GetEdep();
                if (isnan(x)) x = -99;
                if (isnan(y)) y = -99;
                if (isnan(z)) z = -99;
                if (isnan(t)) t = 0;
                if (isnan(e)) e = -999;
                textFile << iEvent << "\t"
                         << iBranch << "\t"
                         << trackID << "\t"
                         << iStep << "\t"
                         << x << "\t"
                         << y << "\t"
                         << z << "\t"
                         << t << "\t"
                         << e << endl;
            }
        }
    }
}
