#ifndef NTTRACKINGACTION_HH
#define NTTRACKINGACTION_HH

#include "NTParameterContainer.hh"
#include "NTG4RunManager.hh"
#include "G4UserTrackingAction.hh"
#include "G4Track.hh"
#include "globals.hh"

class NTTrackingAction : public G4UserTrackingAction
{
  public:
    NTTrackingAction();
    NTTrackingAction(NTG4RunManager *man);
    virtual ~NTTrackingAction() {}

    virtual void PreUserTrackingAction(const G4Track* track);

  private:
    NTParameterContainer *fProcessTable;

    NTG4RunManager *fRunManager = nullptr;
};

#endif
