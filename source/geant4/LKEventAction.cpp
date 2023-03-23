#include "LKEventAction.hpp"

#include "G4Event.hh"
#include "G4RunManager.hh"

LKEventAction::LKEventAction()
: G4UserEventAction()
{
  fRunManager = (LKG4RunManager *) LKG4RunManager::GetRunManager();
}

LKEventAction::LKEventAction(LKG4RunManager *man)
: G4UserEventAction(), fRunManager(man)
{
}
void LKEventAction::EndOfEventAction(const G4Event*)
{
  fRunManager -> NextEvent();
}
