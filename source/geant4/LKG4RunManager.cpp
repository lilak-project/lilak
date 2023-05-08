#include "globals.hh"
#include "G4UImanager.hh"
//#include "G4GDMLParser.hh"
#include "G4UIExecutive.hh"
#include "G4strstreambuf.hh"
#include "G4VisExecutive.hh"
#include "G4ProcessTable.hh"

#include "TSystem.h"
#include "TObjString.h"

#include "LKEventAction.hpp"
#include "LKG4RunManager.hpp"
#include "LKG4RunMessenger.hpp"
#include "LKTrackingAction.hpp"
#include "LKSteppingAction.hpp"
#include "LKPrimaryGeneratorAction.hpp"
#include "LKPrimaryGeneratorAction.hpp"

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
    if (GetUserPrimaryGeneratorAction() == nullptr) SetUserAction(new LKPrimaryGeneratorAction());
    if (GetUserEventAction() == nullptr)    SetUserAction(new LKEventAction(this));
    if (GetUserTrackingAction() == nullptr) SetUserAction(new LKTrackingAction(this));
    if (GetUserSteppingAction() == nullptr) SetUserAction(new LKSteppingAction(this));

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
    SetGeneratorFile(fPar->GetParString("LKG4Manager/G4InputFile").Data());

    auto procNames = G4ProcessTable::GetProcessTable() -> GetNameList();
    Int_t idx = 0;
    fProcessTable -> SetPar("Primary", idx++);
    for (auto name : *procNames)
        if (fProcessTable -> CheckPar(name) == false)
            fProcessTable -> SetPar(name, idx++);
    fProcessTable -> Print();

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
    G4UImanager* uiManager = G4UImanager::GetUIpointer();
    TString command("/control/execute ");

    if (fPar->CheckPar("LKG4Manager/G4VisFile")) {
        auto fileName = fPar -> GetParString("LKG4Manager/G4VisFile");

        G4VisManager* visManager = new G4VisExecutive;
        visManager -> Initialize();

        G4UIExecutive* uiExecutive = new G4UIExecutive(argc,argv,type);
        g4man_info << "Initializing Geant4 run with viewer macro " << fileName << endl;
        uiManager -> ApplyCommand(command+fileName);
        uiExecutive -> SessionStart();

        delete uiExecutive;
        delete visManager;
    }
    else if (fPar->CheckPar("LKG4Manager/G4MacroFile")) {
        auto fileName = fPar -> GetParString("LKG4Manager/G4MacroFile");
        g4man_info << "Initializing Geant4 run with macro " << fileName << endl;
        uiManager -> ApplyCommand(command+fileName);
    }

    WriteToFile(fProcessTable);
    WriteToFile(fSensitiveDetectors);
    WriteToFile(fVolumes);
    EndOfRun();
}

void LKG4RunManager::BeamOnAll()
{
    BeamOn(fNumEvents);
}

void LKG4RunManager::BeamOn(G4int numEvents, const char *macroFile, G4int numSelect)
{
    if(numEvents<=0) { fakeRun = true; }
    else { fakeRun = false; }
    G4bool cond = ConfirmBeamOnCondition();
    if(cond)
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

        DoEventLoop(numEvents,macroFile,numSelect);
        RunTermination();
    }
    fakeRun = false;
}

void LKG4RunManager::SetSuppressInitMessage(bool val) { fSuppressInitMessage = val; }

void LKG4RunManager::SetGeneratorFile(TString value)
{
    auto pga = (LKPrimaryGeneratorAction *) userPrimaryGeneratorAction;
    //fPar -> ReplaceEnvironmentVariable(value);
    pga -> SetEventGenerator(value.Data());
}

void LKG4RunManager::SetOutputFile(TString name)
{
    //fPar -> ReplaceEnvironmentVariable(name);

    fSetEdepSumTree         = fPar->GetParBool("LKG4Manager/MCSetEdepSumTree");;
    fStepPersistency        = fPar->GetParBool("LKG4Manager/MCStepPersistency");;
    fSecondaryPersistency   = fPar->GetParBool("LKG4Manager/MCSecondaryPersistency");
    fTrackVertexPersistency = fPar->GetParBool("LKG4Manager/MCTrackVertexPersistency");

    fFile = new TFile(name,"recreate");
    fTree = new TTree("event", name);

    fTrackArray = new TClonesArray("LKMCTrack", 100);
    fTree -> Branch("MCTrack", &fTrackArray);

    fStepArrayList = new TObjArray();

    if (fStepPersistency)
    {
        TIter itDetectors(fSensitiveDetectors);
        TParameter<Int_t> *det;

        while ((det = dynamic_cast<TParameter<Int_t>*>(itDetectors())))
        {
            TString detName = det -> GetName();
            Int_t copyNo = det -> GetVal();

            if (detName.Index("_PARTOFASSEMBLY")>0)
                continue;

            TString branchHeader = "MCStep";
            if (detName.Index("_ASSEMBLY")>0)
                branchHeader = "MCStepAssembly";

            g4man_info << "Set " << detName << " " << copyNo << endl;
            auto stepArray = new TClonesArray("LKMCStep", 10000);
            stepArray -> SetName(branchHeader+Form("%d", copyNo));

            fTree -> Branch(stepArray -> GetName(), &stepArray);
            fStepArrayList -> Add(stepArray);

            TString edepSumName = Form("EdepSum%d", copyNo);
            fIdxOfCopyNo[copyNo] = fNumActiveVolumes;
            fTree -> Branch(edepSumName, &fEdepSumArray[fNumActiveVolumes]);
            ++fNumActiveVolumes;
        }
    }
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

    fVolumes -> SetPar(name, copyNo);
}

void LKG4RunManager::SetSensitiveDetector(G4VPhysicalVolume *physicalVolume, TString assemblyName)
{
    TString name = physicalVolume -> GetName().data();
    Int_t copyNo = physicalVolume -> GetCopyNo();

    if (assemblyName.IsNull())
        fSensitiveDetectors -> SetPar(name, copyNo);
    else {
        Int_t assemblyID;
        if (fSensitiveDetectors -> CheckPar(assemblyName+"_ASSEMBLY")) {
            assemblyID = fSensitiveDetectors -> GetParInt(assemblyName+"_ASSEMBLY");
        }
        else {
            assemblyID = fNumSDAssemblyInTree;
            fSensitiveDetectors -> SetPar(assemblyName+"_ASSEMBLY",assemblyID);
            ++fNumSDAssemblyInTree;
        }
        fMapSDToAssembly[copyNo] = assemblyID;
        fSensitiveDetectors -> SetPar(name+"_PARTOFASSEMBLY",assemblyID);
    }
}

LKParameterContainer *LKG4RunManager::GetVolumes() { return fVolumes; }
LKParameterContainer *LKG4RunManager::GetSensitiveDetectors() { return fSensitiveDetectors; }
LKParameterContainer *LKG4RunManager::GetProcessTable()       { return fProcessTable; }



void LKG4RunManager::AddMCTrack(Int_t trackID, Int_t parentID, Int_t pdg, Double_t px, Double_t py, Double_t pz, Int_t detectorID, Double_t vx, Double_t vy, Double_t vz, Int_t processID)
{
    if (parentID != 0 && !fSecondaryPersistency) {
        fCurrentTrack = nullptr;
        return;
    }

    fTrackID = trackID;
    fCurrentTrack = (LKMCTrack *) fTrackArray -> ConstructedAt(fTrackArray -> GetEntriesFast());
    fCurrentTrack -> SetMCTrack(trackID, parentID, pdg, px, py, pz, detectorID, vx, vy, vz, processID);
}

void LKG4RunManager::AddTrackVertex(Double_t px, Double_t py, Double_t pz, Int_t detectorID, Double_t vx, Double_t vy, Double_t vz)
{
    if (fCurrentTrack == nullptr || !fTrackVertexPersistency)
        return;

    fCurrentTrack -> AddVertex(px, py, pz, detectorID, vx, vy, vz);
}

void LKG4RunManager::AddMCStep(Int_t detectorID, Double_t x, Double_t y, Double_t z, Double_t t, Double_t e)
{
    auto idx = fIdxOfCopyNo[detectorID];

    if (fSetEdepSumTree)
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
