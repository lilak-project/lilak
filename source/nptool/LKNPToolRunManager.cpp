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
    fPar -> SetCollectParameters(true);
    auto Reaction = fPar -> InitPar("Beam", "NPTool/Reaction/Type ?? # Beam, Isotropic, AlphaDecay");
    bool reactionParameterExist = fPar -> CheckPar("NPTool/Reaction/Type");
    vector<TString> reactions;
    if (reactionParameterExist==false) {
        reactions.push_back("Beam");
        reactions.push_back("AlphaDecay");
        reactions.push_back("Isotropic");
    }
    reactions.push_back(Reaction);

    fReactionFileName = TString(LILAK_PATH)+"/common/nptool_reaction_dummy.reaction";

    for (auto reaction0 : reactions)
    {
        if (reaction0=="Beam")
        {
            auto Particle          = fPar -> InitPar("1H",    "NPTool/Reaction/Beam/Particle           ?? # particle name (1H, 4He, gamma, ...)");
            auto ExcitationEnergy  = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/ExcitationEnergy   ?? MeV", 0);
            auto ZEmission         = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/ZEmission          ?? mm" , 0);
            auto ZProfile          = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/ZProfile           ?? mm" , 0);
            auto Energy            = fPar -> InitPar(10.,     "NPTool/Reaction/Beam/Energy             ?? MeV", 0);
            auto SigmaEnergy       = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/SigmaEnergy        ?? MeV", 0);
            auto SigmaThetaX       = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/SigmaThetaX        ?? deg", 0);
            auto SigmaPhiY         = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/SigmaPhiY          ?? deg", 0);
            auto SigmaX            = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/SigmaX             ?? mm" , 0);
            auto SigmaY            = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/SigmaY             ?? mm" , 0);
            auto MeanThetaX        = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/MeanThetaX         ?? deg", 0);
            auto MeanPhiY          = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/MeanPhiY           ?? deg", 0);
            auto MeanX             = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/MeanX              ?? mm" , 0);
            auto MeanY             = fPar -> InitPar(0.,      "NPTool/Reaction/Beam/MeanY              ?? mm" , 0);
            auto EnergyProfilePath = fPar -> InitPar("",      "NPTool/Reaction/Beam/EnergyProfilePath  ?? # ");
            auto XThetaXProfilePath= fPar -> InitPar("",      "NPTool/Reaction/Beam/XThetaXProfilePath ?? # ");
            auto YPhiYProfilePath  = fPar -> InitPar("",      "NPTool/Reaction/Beam/YPhiYProfilePath   ?? # ");
            ofstream reaction_file(fReactionFileName);
            reaction_file << "Beam" << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/Particle")          -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/ExcitationEnergy")  -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/ZEmission")         -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/ZProfile")          -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/Energy")            -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/SigmaEnergy")       -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/SigmaThetaX")       -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/SigmaPhiY")         -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/SigmaX")            -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/SigmaY")            -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/MeanThetaX")        -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/MeanPhiY")          -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/MeanX")             -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/MeanY")             -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/EnergyProfilePath") -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/XThetaXProfilePath")-> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Beam/YPhiYProfilePath")  -> GetLine("m:n:1") << endl;
        }

        else if (reaction0=="AlphaDecay")
        {
            auto EnergyLow         = fPar -> InitPar(9.9,        "NPTool/Reaction/AlphaDecay/EnergyLow         ?? MeV", 0);
            auto EnergyHigh        = fPar -> InitPar(10.0,       "NPTool/Reaction/AlphaDecay/EnergyHigh        ?? MeV", 0);
            auto HalfOpenAngleMin  = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/HalfOpenAngleMin  ?? deg", 0);
            auto HalfOpenAngleMax  = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/HalfOpenAngleMax  ?? deg", 0);
            auto x0                = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/x0                ?? mm" , 0);
            auto y0                = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/y0                ?? mm" , 0);
            auto z0                = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/z0                ?? mm" , 0);
            auto SigmaX            = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/SigmaX            ?? mm" , 0);
            auto SigmaY            = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/SigmaY            ?? mm" , 0);
            auto SigmaZ            = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/SigmaZ            ?? mm" , 0);
            auto ExcitationEnergy  = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/ExcitationEnergy  ?? MeV", 0);
            auto ActivityBq        = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/ActivityBq        ?? Bq" , 0);
            auto TimeWindow        = fPar -> InitPar(0.,         "NPTool/Reaction/AlphaDecay/TimeWindow        ?? s"  , 0);
            auto direction         = fPar -> InitPar(TVector3(0,0,1), "NPTool/Reaction/AlphaDecay/Direction         ??");
            auto SourceProfile     = fPar -> InitPar("",         "NPTool/Reaction/AlphaDecay/SourceProfile     ??");
            ofstream reaction_file(fReactionFileName);
            reaction_file << "AlphaDecay" << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/EnergyLow")        -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/EnergyHigh")       -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/HalfOpenAngleMin") -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/HalfOpenAngleMax") -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/x0")               -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/y0")               -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/z0")               -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/SigmaX")           -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/SigmaY")           -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/SigmaZ")           -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/ExcitationEnergy") -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/ActivityBq")       -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/TimeWindow")       -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/Direction")        -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/AlphaDecay/SourceProfile")    -> GetLine("m:n:1") << endl;
        }

        else if (reaction0=="Isotropic")
        {
            auto Particle               = fPar -> InitPar("4He",      "NPTool/Reaction/Isotropic/Particle               ?? # particle name (1H, 4He, gamma, ...)");
            auto EnergyDistribution     = fPar -> InitPar("flat",     "NPTool/Reaction/Isotropic/EnergyDistribution     ?? # flat, FromHisto, Watt");
            auto EnergyLow              = fPar -> InitPar(9.9,        "NPTool/Reaction/Isotropic/EnergyLow              ?? MeV", 0);
            auto EnergyHigh             = fPar -> InitPar(10.0,       "NPTool/Reaction/Isotropic/EnergyHigh             ?? MeV", 0);
            auto HalfOpenAngleMin       = fPar -> InitPar(0.,         "NPTool/Reaction/Isotropic/HalfOpenAngleMin       ?? deg", 0);
            auto HalfOpenAngleMax       = fPar -> InitPar(0.,         "NPTool/Reaction/Isotropic/HalfOpenAngleMax       ?? deg", 0);
            auto x0                     = fPar -> InitPar(0.,         "NPTool/Reaction/Isotropic/x0                     ?? mm" , 0);
            auto y0                     = fPar -> InitPar(0.,         "NPTool/Reaction/Isotropic/y0                     ?? mm" , 0);
            auto z0                     = fPar -> InitPar(0.,         "NPTool/Reaction/Isotropic/z0                     ?? mm" , 0);
            auto ExcitationEnergy       = fPar -> InitPar(0.,         "NPTool/Reaction/Isotropic/ExcitationEnergy       ?? MeV", 0);
            auto SigmaX                 = fPar -> InitPar(0.,         "NPTool/Reaction/Isotropic/SigmaX                 ?? mm" , 0);
            auto SigmaY                 = fPar -> InitPar(0.,         "NPTool/Reaction/Isotropic/SigmaY                 ?? mm" , 0);
            auto SigmaZ                 = fPar -> InitPar(0.,         "NPTool/Reaction/Isotropic/SigmaZ                 ?? mm" , 0);
            auto Multiplicity           = fPar -> InitPar(Int_t(1),          TString("NPTool/Reaction/Isotropic/Multiplicity           ??"));
            auto Direction              = fPar -> InitPar("",         "NPTool/Reaction/Isotropic/Direction              ??");
            auto EnergyDistributionHist = fPar -> InitPar("",         "NPTool/Reaction/Isotropic/EnergyDistributionHist ??");
            auto SourceProfile          = fPar -> InitPar("",         "NPTool/Reaction/Isotropic/SourceProfile          ??");
            ofstream reaction_file(fReactionFileName);
            reaction_file << "Isotropic" << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/Particle")               -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/EnergyDistribution")     -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/EnergyLow")              -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/EnergyHigh")             -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/HalfOpenAngleMin")       -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/HalfOpenAngleMax")       -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/x0")                     -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/y0")                     -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/z0")                     -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/ExcitationEnergy")       -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/SigmaX")                 -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/SigmaY")                 -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/SigmaZ")                 -> GetLine("m:n:1") << endl;
            reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/Multiplicity")           -> GetLine("m:n:1") << endl;
            if ((fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/Direction") -> GetValue()).IsNull()==false)
                reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/Direction")              -> GetLine("m:n:1") << endl;
            if ((fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/EnergyDistributionHist") -> GetValue()).IsNull()==false)
                reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/EnergyDistributionHist") -> GetLine("m:n:1") << endl;
            if ((fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/SourceProfile") -> GetValue()).IsNull()==false)
                reaction_file << fPar -> GetCollectedParameterContainer() -> FindPar("NPTool/Reaction/Isotropic/SourceProfile")          -> GetLine("m:n:1") << endl;
        }
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
