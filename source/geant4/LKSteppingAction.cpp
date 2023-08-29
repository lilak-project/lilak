#include "LKSteppingAction.h"

#include "G4Event.hh"
#include "G4RunManager.hh"

LKSteppingAction::LKSteppingAction()
: G4UserSteppingAction()
{
    fRunManager = (LKG4RunManager *) LKG4RunManager::GetRunManager();
}

LKSteppingAction::LKSteppingAction(LKG4RunManager *man)
    : G4UserSteppingAction(), fRunManager(man)
{
}

void LKSteppingAction::UserSteppingAction(const G4Step* step)
{
    G4StepStatus stat = step -> GetPostStepPoint() -> GetStepStatus();
#ifdef LKG4_DEBUG_STEPPINGACTION
    G4String statName = "";
    if (stat==fWorldBoundary        ) statName = "WorldBoundary";
    if (stat==fGeomBoundary         ) statName = "GeomBoundary";
    if (stat==fAtRestDoItProc       ) statName = "AtRestDoItProc";
    if (stat==fAlongStepDoItProc    ) statName = "AlongStepDoItProc";
    if (stat==fPostStepDoItProc     ) statName = "PostStepDoItProc";
    if (stat==fUserDefinedLimit     ) statName = "UserDefinedLimit";
    if (stat==fExclusivelyForcedProc) statName = "ExclusivelyForcedProc";
    if (stat==fUndefined            ) statName = "Undefined";
#endif


    G4ThreeVector pos = step -> GetTrack() -> GetPosition();
    G4ThreeVector mom = step -> GetTrack() -> GetMomentum();
    G4int preNo = step -> GetPreStepPoint() -> GetPhysicalVolume() -> GetCopyNo();

    if (stat == fWorldBoundary) {
        fRunManager -> AddTrackVertex(mom.x(),mom.y(),mom.z(),preNo,pos.x(),pos.y(),pos.z());
#ifdef LKG4_DEBUG_STEPPINGACTION
        g4man_info << statName << " T" << step->GetTrack()->GetTrackID() << "(" << step->GetTrack()->GetParticleDefinition()->GetParticleName() << ")" << " pos(" << pos.x() << ", " << pos.y() << ", " << pos.z() << ") " << endl;
#endif
        return;
    }

    G4int postNo = step -> GetPostStepPoint() -> GetPhysicalVolume() -> GetCopyNo();
    if (preNo != postNo || step -> GetNumberOfSecondariesInCurrentStep() > 0)
        fRunManager -> AddTrackVertex(mom.x(),mom.y(),mom.z(),preNo,pos.x(),pos.y(),pos.z());

    G4double edep = step -> GetTotalEnergyDeposit(); 
    if (edep <= 0) {
#ifdef LKG4_DEBUG_STEPPINGACTION
        g4man_info << statName << " T" << step->GetTrack()->GetTrackID() << "(" << step->GetTrack()->GetParticleDefinition()->GetParticleName() << ") in " << step->GetPreStepPoint()->GetPhysicalVolume()->GetName() << "/" << step->GetPreStepPoint()->GetPhysicalVolume()->GetLogicalVolume()->GetMaterial()->GetName() << " pos(" << pos.x() << ", " << pos.y() << ", " << pos.z() << ") edep = " << edep << endl;
#endif
        return;
    }

    G4double time = step -> GetPreStepPoint() -> GetGlobalTime();
    G4ThreeVector stepPos = .5 * (step -> GetPreStepPoint() -> GetPosition() + step -> GetPostStepPoint() -> GetPosition());
    fRunManager -> AddMCStep(preNo, stepPos.x(), stepPos.y(), stepPos.z(), time, edep);
#ifdef LKG4_DEBUG_STEPPINGACTION
    g4man_info << statName << " T" << step->GetTrack()->GetTrackID() << "(" << step->GetTrack()->GetParticleDefinition()->GetParticleName() << ") in " << step->GetPreStepPoint()->GetPhysicalVolume()->GetName() << "/" << step->GetPreStepPoint()->GetPhysicalVolume()->GetLogicalVolume()->GetMaterial()->GetName() << " i" << preNo << " pos(" << stepPos.x() << ", " << stepPos.y() << ", " << stepPos.z() << ") TE[" << time << ", " << edep << "]" << endl;
#endif
}
