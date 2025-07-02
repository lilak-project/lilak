#ifndef LKNPTOOLRUNMANAGER_HH
#define LKNPTOOLRUNMANAGER_HH

#include "LKG4RunManager.h"

class LKNPToolRunManager : public LKG4RunManager
{
    public:
        LKNPToolRunManager();
        virtual ~LKNPToolRunManager();

        virtual void AddParameterContainer(TString fname);

    public:
        virtual void CheckNPToolPhysicsListFile();
        virtual void CheckNPToolReactionFile();

        virtual void InitPreAddActions();
        virtual void InitPostAddActions();
        virtual bool InitGeneratorFile();

        virtual void SetUserAction(G4VUserPrimaryGeneratorAction *userAction);
        virtual void SetUserAction(G4UserRunAction      *userAction) { G4RunManager::SetUserAction(userAction); }
        virtual void SetUserAction(G4UserEventAction    *userAction) { G4RunManager::SetUserAction(userAction); }
        virtual void SetUserAction(G4UserStackingAction *userAction) { G4RunManager::SetUserAction(userAction); }
        virtual void SetUserAction(G4UserTrackingAction *userAction) { G4RunManager::SetUserAction(userAction); }
        virtual void SetUserAction(G4UserSteppingAction *userAction) { G4RunManager::SetUserAction(userAction); }

        virtual void SetUserInitialization(G4VUserPhysicsList               *userInit);
        virtual void SetUserInitialization(G4UserWorkerInitialization       *userInit) { G4RunManager::SetUserInitialization(userInit); }
        virtual void SetUserInitialization(G4VUserDetectorConstruction      *userInit) { G4RunManager::SetUserInitialization(userInit); }
        virtual void SetUserInitialization(G4VUserActionInitialization      *userInit) { G4RunManager::SetUserInitialization(userInit); }
        virtual void SetUserInitialization(G4UserWorkerThreadInitialization *userInit) { G4RunManager::SetUserInitialization(userInit); }

    protected:
        bool fNPToolMode = false;
        TString fReactionFileName;
};

#endif
