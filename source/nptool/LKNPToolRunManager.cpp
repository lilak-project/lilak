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

#include "LKEventAction.h"
#include "LKNPToolRunManager.h"
//#include "LKNPToolRunMessenger.h"
#include "LKTrackingAction.h"
#include "LKStackingAction.h"
#include "LKSteppingAction.h"
#include "LKPrimaryGeneratorAction.h"
#include "LKNPDetectorConstructionFactory.h"

#include "NPOptionManager.h"
#include "PrimaryGeneratorAction.hh" // nptool
#include "DetectorConstruction.hh"
#include "PhysicsList.hh"

LKNPToolRunManager::LKNPToolRunManager()
:LKG4RunManager()
{
}

LKNPToolRunManager::~LKNPToolRunManager()
{
}

void LKNPToolRunManager::AddParameterContainer(TString fname)
{
    LKG4RunManager::AddParameterContainer(fname);

    fNPToolMode       = fPar -> InitPar(false, "#LKG4Manager/NPToolMode ?? # set true if users are using nptool libraries (not used anymore)");
    fReactionFileName = fPar -> InitPar("",    "NPTool/ReactionFile ?? # name of the nptool reaction file");
}

void LKNPToolRunManager::InitPreAddActions()
{
    g4man_info << "Using nptool mode!!" << endl;
    if (GetUserDetectorConstruction() == nullptr)
    {
        g4man_warning << "No user detector construction is set!" << endl;
        LKNPDetectorConstructionFactory dcf;
        if (!fDetectorConstructionName.IsNull()) {
            g4man_info << "Adding " << fDetectorConstructionName << endl;
            SetUserInitialization(dcf.GetDetectorConstruction(fDetectorConstructionName));
        }
        else {
            g4man_info << "Adding LKNPDetectorConstruction " << endl;
            SetUserInitialization(dcf.GetDetectorConstruction("LKNPDetectorConstruction"));
        }
    }
    G4RunManager::SetUserInitialization(new PhysicsList());
    if (GetUserEventAction()    == nullptr) { g4man_info << "Adding LKEventAction"    << endl; SetUserAction(new LKEventAction   (this)); }
    if (GetUserTrackingAction() == nullptr) { g4man_info << "Adding LKTrackingAction" << endl; SetUserAction(new LKTrackingAction(this)); }
    if (GetUserStackingAction() == nullptr) { g4man_info << "Adding LKStackingAction" << endl; SetUserAction(new LKStackingAction(this)); }
    if (GetUserSteppingAction() == nullptr) { g4man_info << "Adding LKSteppingAction" << endl; SetUserAction(new LKSteppingAction(this)); }

    g4man_info << "nptool reaction file : " << fReactionFileName << endl;
    if (fReactionFileName.IsNull())
        g4man_error << "No nptool reaction file! (parameter NPTool/ReactionFile)" << endl;
    NPOptionManager::getInstance() -> SetIsSimulation();
    NPOptionManager::getInstance() -> SetReactionFile(fReactionFileName.Data());
}

void LKNPToolRunManager::InitPostAddActions()
{
    g4man_info << "nptool mode!" << endl;
    g4man_info << "Adding primaryGeneratorAction" << endl;
    g4man_info << "nptool reaction file : " << fReactionFileName << endl;
    auto primaryGeneratorAction = new PrimaryGeneratorAction((DetectorConstruction*)GetUserDetectorConstruction());
    primaryGeneratorAction -> ReadEventGeneratorFile(fReactionFileName.Data());
    G4RunManager::SetUserAction(primaryGeneratorAction);
}

bool LKNPToolRunManager::InitGeneratorFile()
{
    g4man_info << "SetGeneratorFile(TString) is disabled for nptool mode." << endl;
    return true;
}

void LKNPToolRunManager::SetUserAction(G4VUserPrimaryGeneratorAction *userAction)
{
    g4man_info << "SetUserAction(G4VUserPrimaryGeneratorAction*) is disabled for nptool mode." << endl;
}

void LKNPToolRunManager::SetUserInitialization(G4VUserPhysicsList *userInit)
{
    g4man_info << "SetUserInitialization(G4VUserPhysicsList*) is disabled for nptool mode." << endl;
}
