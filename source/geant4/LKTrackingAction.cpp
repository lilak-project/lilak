#include "G4Event.hh"
#include "globals.hh"
#include "G4VProcess.hh"
#include "G4RunManager.hh"
#include "G4ThreeVector.hh"
#include "G4ProcessTable.hh"

#include "LKTrackingAction.h"

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

    G4int processID = -1;
    if (fProcessTable -> CheckPar(processName))
        processID = fProcessTable -> GetParInt(processName);

#ifdef LKG4_DEBUG_TRACKINGACTION
    g4man_info << "Start of T" << track->GetTrackID() << "(" << track->GetParticleDefinition()->GetParticleName() << "), mom(" << momentum.x() << ", " << momentum.y() << ", " << momentum.z() << "), V" << volumeID << ", pos(" << position.x() << ", " << position.y() << ", " << position.z() << ") " << processName << endl;
#endif

    fRunManager -> AddMCTrack(track->GetTrackID(), track->GetParentID(), track->GetDefinition()->GetPDGEncoding(), volumeID, processID,
                              position.x(), position.y(), position.z(), momentum.x(), momentum.y(), momentum.z(), track->GetKineticEnergy());
}

#ifdef LKG4_DEBUG_TRACKINGACTION
void LKTrackingAction::PostUserTrackingAction(const G4Track* track)
{
    G4ThreeVector momentum = track -> GetMomentum();
    G4ThreeVector position = track -> GetPosition();
    G4int volumeID = track -> GetVolume() -> GetCopyNo();

    const G4VProcess *process = track -> GetCreatorProcess();
    G4String processName = "Primary";
    if (process != nullptr)
        processName = process -> GetProcessName();

    g4man_info << "End of T" << track->GetTrackID() << "(" << track->GetParticleDefinition()->GetParticleName() << "), mom(" << momentum.x() << ", " << momentum.y() << ", " << momentum.z() << "), V" << volumeID << ", pos(" << position.x() << ", " << position.y() << ", " << position.z() << ") " << processName << endl;
}
#endif
