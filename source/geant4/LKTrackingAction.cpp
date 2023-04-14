#include "G4Event.hh"
#include "globals.hh"
#include "G4VProcess.hh"
#include "G4RunManager.hh"
#include "G4ThreeVector.hh"

#include "LKTrackingAction.hpp"

LKTrackingAction::LKTrackingAction()
: G4UserTrackingAction()
{
    fRunManager = (LKG4RunManager *) LKG4RunManager::GetRunManager();
    fProcessTable = fRunManager -> GetProcessTable();
}

LKTrackingAction::LKTrackingAction(LKG4RunManager *man)
: G4UserTrackingAction(), fRunManager(man)
{
    fProcessTable = fRunManager -> GetProcessTable();
}

void LKTrackingAction::PreUserTrackingAction(const G4Track* track)
{
    G4ThreeVector momentum = track -> GetMomentum();
    G4ThreeVector position = track -> GetPosition();
    G4int volumeID = track -> GetVolume() -> GetCopyNo();

    const G4VProcess *process = track -> GetCreatorProcess();
    G4String processName = "Primary";
    if (process != nullptr)
        processName = process -> GetProcessName();
    G4int processID = fProcessTable -> GetParInt(processName);

    fRunManager -> AddMCTrack(track -> GetTrackID(), track -> GetParentID(), track -> GetDefinition() -> GetPDGEncoding(), momentum.x(), momentum.y(), momentum.z(), volumeID, position.x(), position.y(), position.z(), processID);
}
