#include "globals.hh"
#include "G4UImanager.hh"
//#include "G4GDMLParser.hh"
#include "G4UIExecutive.hh"
#include "G4strstreambuf.hh"
#include "G4VisExecutive.hh"
#include "G4ProcessTable.hh"

#include "TSystem.h"
#include "TObjString.h"

#include "LKEventAction.h"
#include "LKG4RunManager.h"
#include "LKG4RunMessenger.h"
#include "LKTrackingAction.h"
#include "LKStackingAction.h"
#include "LKSteppingAction.h"
#include "LKPrimaryGeneratorAction.h"

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

void LKG4RunManager::Initialize()
{
    fTryInitialized = true;

    if (GetUserPrimaryGeneratorAction() == nullptr) SetUserAction(new LKPrimaryGeneratorAction());
    if (GetUserEventAction() == nullptr)    SetUserAction(new LKEventAction(this));
    if (GetUserTrackingAction() == nullptr) SetUserAction(new LKTrackingAction(this));
    if (GetUserStackingAction() == nullptr) SetUserAction(new LKStackingAction(this));
    if (GetUserSteppingAction() == nullptr) SetUserAction(new LKSteppingAction(this));

    if (fPar -> CheckPar("LKG4Manager/SuppressG4InitMessage"))
        fSuppressInitMessage = fPar -> GetParBool("LKG4Manager/SuppressG4InitMessage");

    if (fPar -> CheckPar("LKG4Manager/SensitiveDetectors")) {
        auto sdNames = fPar -> GetParVString("LKG4Manager/SensitiveDetectors");
        for (auto sdName : sdNames)
        {
            if (sdName.Index("!")!=0)
            {
                sdName.ReplaceAll("!","");
                fSDAssembly.push_back(sdName);
            }
            else
                fSDNames.push_back(sdName);
        }
    }

    G4RunManager::Initialize();

    SetOutputFile(fPar->GetParString("LKG4Manager/G4OutputFile").Data());

    bool nptoolMode = false;
    if(fPar -> CheckPar("LKG4Manager/NPToolMode")){
        if(fPar->GetParBool("LKG4Manager/NPToolMode"))
            nptoolMode = true;
    }
    if(!nptoolMode){
        fInitEventGenerator = SetGeneratorFile(fPar->GetParString("LKG4Manager/G4InputFile").Data());
        if (!fInitEventGenerator) {
            fInitialized = false;
            g4man_error << "Event generator cannot be initialized!" << endl;
            return;
        }
    }

    auto procNames = G4ProcessTable::GetProcessTable() -> GetNameList();
    Int_t idx = 0;
    fProcessTable -> AddPar("Primary", idx++);
    for (auto name : *procNames)
        if (fProcessTable -> CheckPar(name) == false)
            fProcessTable -> AddPar(name, idx++);
    //fProcessTable -> Print();

    /*
    if (fPar->CheckPar("G4ExportGDML"))
    {
        TString fileName = fPar -> GetParString("G4ExportGDML");
        TString name = gSystem -> Which(".", fileName.Data());

        if (name.IsNull()) {
            g4man_info << "Exporting geometry in GMDL format: " << fileName << endl;
            G4GDMLParser parser;
            auto world = G4RunManagerKernel::GetRunManagerKernel() -> GetCurrentWorld();
            parser.Write(fileName.Data(),world);
        }
        else {
            g4man_warning << "The file " << fileName << " exist already." << endl;
            g4man_warning << "Stopped exporting geomtry" << endl;
        }
    }
    */

    fInitialized = true;
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
    if (fTryInitialized==false) {
        g4man_info << "Run manager is not inialized." << endl;
        g4man_info << "Running Initialize() ..." << endl;
        Initialize();
    }

    if (fInitialized==false) {
        g4man_error << "Unable to initialize." << endl;
        return;
    }

    G4UImanager* uiManager = G4UImanager::GetUIpointer();

    bool useVisMode = false;
    if (fPar->CheckPar("LKG4Manager/VisMode")) {
        if (fPar -> GetParBool("LKG4Manager/VisMode")==true)
            useVisMode = true;
    }

    auto g4CommandContainer = fPar -> CreateGroupContainer("G4");
    g4CommandContainer -> Print();

    TIter next(g4CommandContainer);

    LKParameter *parameter = nullptr;
    if (useVisMode) {
        G4VisManager* visManager = new G4VisExecutive;
        visManager -> Initialize();

        G4UIExecutive* uiExecutive = new G4UIExecutive(argc,argv,type);
        while ((parameter = (LKParameter*) next())) {
            auto command = Form("/%s",parameter->GetLine("").Data());
            g4man_info << command << endl;
            uiManager -> ApplyCommand(command);
        }
        uiExecutive -> SessionStart();

        delete uiExecutive;
        delete visManager;
    }
    else {
        while ((parameter = (LKParameter*) next())) {
            auto command = Form("/%s",parameter->GetLine("").Data());
            g4man_info << command << endl;
            uiManager -> ApplyCommand(command);
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

bool LKG4RunManager::SetGeneratorFile(TString value)
{
    auto pga = (LKPrimaryGeneratorAction *) userPrimaryGeneratorAction;
    //fPar -> ReplaceEnvironmentVariable(value);
    if (!pga -> SetEventGenerator(value.Data()))
        return false;
    return true;
}

void LKG4RunManager::SetOutputFile(TString name)
{
    //fPar -> ReplaceEnvironmentVariable(name);

    if (fPar -> CheckPar("MCStep/persistency"       )) { fStepPersistency        = fPar -> GetParBool("MCStep/persistency"       ); }
    if (fPar -> CheckPar("MCEdepSum/persistency"    )) { fEdepSumPersistency     = fPar -> GetParBool("MCEdepSum/persistency"    ); }
    if (fPar -> CheckPar("MCSecondary/persistency"  )) { fSecondaryPersistency   = fPar -> GetParBool("MCSecondary/persistency"  ); }
    if (fPar -> CheckPar("MCTrackVertex/persistency")) { fTrackVertexPersistency = fPar -> GetParBool("MCTrackVertex/persistency"); }

    if (fPar -> CheckPar("persistency/MCStep"       )) { fStepPersistency        = fPar -> GetParBool("persistency/MCStep"       ); }
    if (fPar -> CheckPar("persistency/MCEdepSum"    )) { fEdepSumPersistency     = fPar -> GetParBool("persistency/MCEdepSum"    ); }
    if (fPar -> CheckPar("persistency/MCSecondary"  )) { fSecondaryPersistency   = fPar -> GetParBool("persistency/MCSecondary"  ); }
    if (fPar -> CheckPar("persistency/MCTrackVertex")) { fTrackVertexPersistency = fPar -> GetParBool("persistency/MCTrackVertex"); }

    g4man_info << "Setting output file " << name << endl;
    fFile = new TFile(name,"recreate");
    fTree = new TTree("event", name);

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
            if (fPar -> CheckPar(Form("%s/persistency",detName.Data())))
                detectorPersistency = fPar -> GetParBool(Form("%s/persistency",detName.Data()));
            if (detectorPersistency==false)
                continue;

            if (detName.Index("_PARTOFASSEMBLY")>0) {
                g4man_info << "Detector " << detName << " is part of assembly" << endl;
                continue;
            }

            TString stepName = TString("MCStep") + detName;
            TString esumName = TString("MCESum") + detName;

            g4man_info << "Adding new step branch " << stepName << " for detector " << detName << endl;

            auto stepArray = new TClonesArray("LKMCStep", 10000);
            fTree -> Branch(stepName, &stepArray);
            fStepArrayList -> Add(stepArray);

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

    for (auto assembly : fSDAssembly) {
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

    for (auto sdName : fSDNames) {
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

    if (assemblyName.IsNull()) {
        g4man_info << "New sensitive detector " << name << " (" << copyNo << ")" << endl;
        fSensitiveDetectors -> AddPar(name, copyNo);
        //fSensitiveDetectors -> AddPar(name, name);
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
    fFile -> Close();
    g4man_info << "Output: " << fFile -> GetName() << endl;
}
