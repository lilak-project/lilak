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
    G4int statID = -1;
         if (stat==fWorldBoundary        ) statID = 0;
    else if (stat==fGeomBoundary         ) statID = 1;
    else if (stat==fAtRestDoItProc       ) statID = 2;
    else if (stat==fAlongStepDoItProc    ) statID = 3;
    else if (stat==fPostStepDoItProc     ) statID = 4;
    else if (stat==fUserDefinedLimit     ) statID = 5;
    else if (stat==fExclusivelyForcedProc) statID = 6;
    else if (stat==fUndefined            ) statID = 7;
    //else if (GetNumberOfSecondariesInCurrentStep>0) statID = 8;
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


    G4Track* track = step -> GetTrack();
    G4ThreeVector pos = track -> GetPosition();
    G4ThreeVector mom = track -> GetMomentum();
    G4int preNo = step -> GetPreStepPoint() -> GetPhysicalVolume() -> GetCopyNo();

    if (stat==fWorldBoundary)
    {
        fRunManager -> AddTrackVertex(preNo, 1/*workld boundary*/, pos.x(), pos.y(), pos.z(), mom.x(),mom.y(),mom.z(), track->GetKineticEnergy());
#ifdef LKG4_DEBUG_STEPPINGACTION
        g4man_info << statName << " T" << track->GetTrackID() << "(" << track->GetParticleDefinition()->GetParticleName() << ")" << " pos(" << pos.x() << ", " << pos.y() << ", " << pos.z() << ") " << endl;
#endif
        return;
    }

    if (stat==fGeomBoundary)
        fRunManager -> AddTrackVertex(preNo, 2/*geometry boundary*/, pos.x(), pos.y(), pos.z(), mom.x(),mom.y(),mom.z(), track->GetKineticEnergy());

    if (step->GetNumberOfSecondariesInCurrentStep()>0)
        fRunManager -> AddTrackVertex(preNo, 3/*secondary created*/, pos.x(), pos.y(), pos.z(), mom.x(),mom.y(),mom.z(), track->GetKineticEnergy());

    G4double edep = step -> GetTotalEnergyDeposit(); 
    /*
    if (edep <= 0) {
#ifdef LKG4_DEBUG_STEPPINGACTION
        g4man_info << statName << " T" << track->GetTrackID() << "(" << track->GetParticleDefinition()->GetParticleName() << ") in " << step->GetPreStepPoint()->GetPhysicalVolume()->GetName() << "/" << step->GetPreStepPoint()->GetPhysicalVolume()->GetLogicalVolume()->GetMaterial()->GetName() << " pos(" << pos.x() << ", " << pos.y() << ", " << pos.z() << ") edep = " << edep << endl;
#endif
        return;
    }
    */

    G4double time = step -> GetPreStepPoint() -> GetGlobalTime();
    G4ThreeVector stepPos = .5 * (step -> GetPreStepPoint() -> GetPosition() + step -> GetPostStepPoint() -> GetPosition());
    fRunManager -> AddMCStep(preNo, stepPos.x(), stepPos.y(), stepPos.z(), time, edep);
#ifdef LKG4_DEBUG_STEPPINGACTION
    g4man_info << statName << " T" << track->GetTrackID() << "(" << track->GetParticleDefinition()->GetParticleName() << ") in " << step->GetPreStepPoint()->GetPhysicalVolume()->GetName() << "/" << step->GetPreStepPoint()->GetPhysicalVolume()->GetLogicalVolume()->GetMaterial()->GetName() << " i" << preNo << " pos(" << stepPos.x() << ", " << stepPos.y() << ", " << stepPos.z() << ") TE[" << time << ", " << edep << "]" << endl;
#endif
}
