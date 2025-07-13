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

#include "TSystem.h"

LKNPToolRunManager::LKNPToolRunManager()
:LKG4RunManager()
{
}

LKNPToolRunManager::~LKNPToolRunManager()
{
}

void LKNPToolRunManager::CheckNPToolInitializationFiles()
{
    TString physicsListFileName = "PhysicsListOption.txt";
    if (TString(gSystem -> Which(".", physicsListFileName)).IsNull())
    {
        g4man_warning << physicsListFileName << " do not exist." << endl;
        TString commonPath = TString(LILAK_PATH)+"/common/";
        TString nppar_file_name = commonPath+"nptool_parameter_PhysicsListOption.mac";
        LKParameterContainer nptool_parameter(nppar_file_name);
        g4man_info << "Example " << physicsListFileName << " is:" << endl;
        nptool_parameter.Print("e:c:l:%");
    }
    TString projectConfigFileName = "project.config";
    if (TString(gSystem -> Which(".", projectConfigFileName)).IsNull())
    {
        g4man_warning << projectConfigFileName << " do not exist." << endl;
        TString commonPath = TString(LILAK_PATH)+"/common/";
        TString nppar_file_name = commonPath+"nptool_parameter_project_config.mac";
        LKParameterContainer nptool_parameter(nppar_file_name);
        g4man_info << "Example " << projectConfigFileName << " is:" << endl;
        nptool_parameter.Print("e:c:l:%");
    }
}

void LKNPToolRunManager::CheckNPToolReactionFile()
{
    TString keyword = "Reaction";
    TString commonPath = TString(LILAK_PATH)+"/common/";
    LKParameterContainer nptool_parameter_list(commonPath+"nptool_parameter_list.mac");
    auto lfReactions = nptool_parameter_list.GetParVString(keyword);
    TString reaction_comments = "";
    for (auto reaction0 : lfReactions)
        reaction_comments += reaction0 + ",";
    reaction_comments = reaction_comments(0,reaction_comments.Sizeof()-2);

    fPar -> SetCollectParameters(true);
    auto Reaction = fPar -> InitPar("Beam", Form("NPTool/%s/Type ?? # %s",keyword.Data(),reaction_comments.Data()));
    bool reactionParameterExist = fPar -> CheckPar(Form("NPTool/%s/Type",keyword.Data()));
    vector<TString> lfCollectedReactions;
    lfCollectedReactions.push_back(Reaction);
    if (reactionParameterExist==false) {
        for (auto reaction0 : lfReactions)
            lfCollectedReactions.push_back(reaction0);
    }

    fReactionFileName = commonPath + "nptool_dummy_reaction.reaction";

    bool firstSample = true;
    for (auto reaction0 : lfCollectedReactions)
    {
        TString nppar_file_name = commonPath+Form("nptool_parameter_%s_%s.mac",keyword.Data(),reaction0.Data());
        LKParameterContainer nptool_parameter(nppar_file_name);
        TIter next(&nptool_parameter);
        LKParameter* parameter;
        while (parameter=(LKParameter*) next()) {
            TString name = parameter -> GetLastName();
            TString value = parameter -> GetValue();
            TString comment = parameter -> GetComment();
            TString parname = Form("NPTool/%s/%s/%s %s #%s",keyword.Data(),reaction0.Data(),name.Data(),value.Data(),comment.Data());
            fPar -> UpdatePar(value, parname);
            parameter -> SetValue(value);
        }
        if (firstSample) nptool_parameter.SaveAs(fReactionFileName,"nptool");
        else             nptool_parameter.SaveAs(fReactionFileName,"nptool:app:%");
        firstSample = false;
    }
}

void LKNPToolRunManager::AddParameterContainer(TString fname)
{
    LKG4RunManager::AddParameterContainer(fname);

    fNPToolMode       = fPar -> InitPar(false, "#LKG4Manager/NPToolMode ?? # set true if users are using nptool libraries (not used anymore)");
    fReactionFileName = fPar -> InitPar("",    "NPTool/Reaction/File ?? # name of the nptool reaction file");

    if (!fReactionFileName.IsNull())
        if (TString(gSystem -> Which(".", fReactionFileName.Data())).IsNull())
            fReactionFileName = "";
    if (fReactionFileName.IsNull())
        CheckNPToolReactionFile();
}

void LKNPToolRunManager::InitPreAddActions()
{
    g4man_info << "Using nptool mode!!" << endl;

    CheckNPToolInitializationFiles();

    if (GetUserDetectorConstruction() == nullptr)
    {
        g4man_warning << "No user detector construction is set!" << endl;
        LKNPDetectorConstructionFactory dcf;
        G4VUserDetectorConstruction* detectorConstruction;
        if (!fDetectorConstructionName.IsNull()) {
            detectorConstruction = dcf.GetDetectorConstruction(fDetectorConstructionName);
            if (detectorConstruction!=nullptr) {
                g4man_info << "Adding " << fDetectorConstructionName << endl;
                SetUserInitialization(detectorConstruction);
            }
            else {
                g4man_error << "Cannot find " << fDetectorConstructionName << " in LKNPDetectorConstructionFactory" << endl;
                g4man_error << "Trying NPTool DetectorConstruction" << endl;
                g4man_error << "This mode is in development ..." << endl;
                fUsingNPToolDetectorConstruction = true;
                SetUserInitialization(new DetectorConstruction);
            }
        }
        else {
            g4man_info << "Adding default LKNPDetectorConstruction" << endl;
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
        g4man_error << "No nptool reaction file! (parameter NPTool/Reaction/File)" << endl;
    NPOptionManager::getInstance() -> SetIsSimulation();
    NPOptionManager::getInstance() -> SetReactionFile(fReactionFileName.Data());
    if (fUsingNPToolDetectorConstruction)
        NPOptionManager::getInstance() -> SetDetectorFile(fDetectorConstructionName.Data());
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
