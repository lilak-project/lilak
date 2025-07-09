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
    CheckNPToolPhysicsListFile();
}

LKNPToolRunManager::~LKNPToolRunManager()
{
}

void LKNPToolRunManager::CheckNPToolPhysicsListFile()
{
    if (TString(gSystem -> Which(".", "PhysicsListOption.txt")).IsNull())
    {
        //g4man_warning << "PhysicsListOption.txt for current project is missing!" << endl;
        //g4man_error << "Creating dummy PhysicsListOption.txt ..." << endl;
        //ofstream physicsListFile("PhysicsListOption.txt");
        /*
        g4man_info << "You may use" << endl;
        physicsListFile << "EmPhysicsList Option4" << endl;
        physicsListFile << "DefaultCutOff 1" << endl;
        physicsListFile << "DriftElectronPhysics 0" << endl;
        physicsListFile << "IonBinaryCascadePhysics 0" << endl;
        physicsListFile << "NPIonInelasticPhysics 0" << endl;
        physicsListFile << "EmExtraPhysics 0" << endl;
        physicsListFile << "HadronElasticPhysics 0" << endl;
        physicsListFile << "StoppingPhysics 0" << endl;
        physicsListFile << "OpticalPhysics 0" << endl;
        physicsListFile << "HadronPhysicsINCLXX 0" << endl;
        physicsListFile << "HadronPhysicsQGSP_BIC_HP 0" << endl;

             if (m_EmList == "INCLXX_EM") { cout << "Choosing to use INCLXX included EMList (probably standard)" << endl; }
        else if (m_EmList == "Option1")   { emPhysicsList = new G4EmStandardPhysics_option1(); cout << "//// Using G4EmStandardPhysics_option1 Physics List ////" << endl; }
        else if (m_EmList == "Option2")   { emPhysicsList = new G4EmStandardPhysics_option2(); cout << "//// Using G4EmStandardPhysics_option2 Physics List ////" << endl; }
        else if (m_EmList == "Option3")   { emPhysicsList = new G4EmStandardPhysics_option3(); cout << "//// Using G4EmStandardPhysics_option3 Physics List ////" << endl; }
        else if (m_EmList == "Option4")   { emPhysicsList = new G4EmStandardPhysics_option4(); cout << "//// Using G4EmStandardPhysics_option4 Physics List ////" << endl; }
        else if (m_EmList == "Standard")  { emPhysicsList = new G4EmStandardPhysics(); cout << "//// Using G4EmStandardPhysics default EM constructor Physics List ////" << endl; }
        else if (m_EmList == "Livermore") { emPhysicsList = new G4EmLivermorePhysics(); cout << "//// Using G4EmLivermorePhysics Physics List ////" << endl; }
        else if (m_EmList == "Penelope")  { emPhysicsList = new G4EmPenelopePhysics(); cout << "//// Using G4EmPenelopePhysics Physics List ////" << endl; }
        else { std::cout << "\r\033[1;31mERROR: User given physics list " << m_EmList << " is not supported, option are Option4 Livermore Penelope\033[0m" << std::endl; exit(1); }

        {
            value = std::atof(str_value.c_str());
                 if (name == "EmPhysicsList") m_EmList = str_value;
            else if (name == "DefaultCutOff") defaultCutValue = value * mm;
            else if (name == "IonBinaryCascadePhysics") m_IonBinaryCascadePhysics = value;
            else if (name == "NPIonInelasticPhysics") m_NPIonInelasticPhysics = value;
            else if (name == "EmExtraPhysics") m_EmExtraPhysics = value;
            else if (name == "HadronElasticPhysics") m_HadronElasticPhysics = value;
            else if (name == "StoppingPhysics") m_StoppingPhysics = value;
            else if (name == "OpticalPhysics") m_OpticalPhysics = value;
            else if (name == "DriftElectronPhysics") m_DriftElectronPhysics = value;
            else if (name == "HadronPhysicsQGSP_BIC_HP") m_HadronPhysicsQGSP_BIC_HP = value;
            else if (name == "HadronPhysicsQGSP_BERT_HP") m_HadronPhysicsQGSP_BERT_HP = value;
            else if (name == "HadronPhysicsINCLXX") m_HadronPhysicsINCLXX = value;
            else if (name == "HadronPhysicsQGSP_INCLXX_HP") { m_HadronPhysicsQGSP_INCLXX_HP = value; if (value) m_INCLXXPhysics = true; }
            else if (name == "HadronPhysicsQGSP_INCLXX") { m_HadronPhysicsQGSP_INCLXX = value; if (value) m_INCLXXPhysics = true; }
            else if (name == "HadronPhysicsFTFP_INCLXX_HP") { m_HadronPhysicsFTFP_INCLXX_HP = value; if (value) m_INCLXXPhysics = true; }
            else if (name == "HadronPhysicsFTFP_INCLXX") { m_HadronPhysicsFTFP_INCLXX = value; if (value) m_INCLXXPhysics = true; }
            else if (name == "Decay") m_Decay = value;
            else if (name == "IonGasModels") m_IonGasModels = value;hysicsListFile << "Decay 0" << endl;
        }
    */
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

    fReactionFileName = commonPath + "nptool_reaction_dummy.reaction";

    bool firstSample = true;
    for (auto reaction0 : lfCollectedReactions)
    {
        TString nfc_file_name = commonPath+Form("nptool_parameter_%s_%s.mac",keyword.Data(),reaction0.Data());
        LKParameterContainer nptool_parameter(nfc_file_name);
        TIter next(&nptool_parameter);
        LKParameter* parameter;
        while (parameter=(LKParameter*) next()) {
            TString name = parameter -> GetLastName();
            TString value = parameter -> GetValue();
            TString comment = parameter -> GetComment();
            TString lkname = Form("NPTool/%s/%s/%s %s #%s",keyword.Data(),reaction0.Data(),name.Data(),value.Data(),comment.Data());
            fPar -> UpdatePar(value, lkname);
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
        g4man_error << "No nptool reaction file! (parameter NPTool/Reaction/File)" << endl;
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
