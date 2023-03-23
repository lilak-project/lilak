#ifndef LKTRACKINGACTION_HH
#define LKTRACKINGACTION_HH

#include "LKParameterContainer.h"
#include "LKG4RunManager.h"
#include "G4UserTrackingAction.hh"
#include "G4Track.hh"
#include "globals.hh"

class LKTrackingAction : public G4UserTrackingAction
{
  public:
    LKTrackingAction();
    LKTrackingAction(LKG4RunManager *man);
    virtual ~LKTrackingAction() {}

    virtual void PreUserTrackingAction(const G4Track* track);

  private:
    LKParameterContainer *fProcessTable;

    LKG4RunManager *fRunManager = nullptr;
};

#endif
