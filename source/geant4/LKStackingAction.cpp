#include "LKLogger.h"
#include "LKStackingAction.h"

LKStackingAction::LKStackingAction()
: G4UserStackingAction()
{
    fRunManager = (LKG4RunManager *) LKG4RunManager::GetRunManager();
}

LKStackingAction::LKStackingAction(LKG4RunManager *man)
: G4UserStackingAction(), fRunManager(man)
{
}

G4ClassificationOfNewTrack LKStackingAction::ClassifyNewTrack(const G4Track* track)
{
    return fUrgent;
}

void LKStackingAction::NewStage()
{
}

void LKStackingAction::PrepareNewEvent()
{
}
