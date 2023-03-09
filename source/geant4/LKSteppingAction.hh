#ifndef NTSTEPPINGACTION_HH
#define NTSTEPPINGACTION_HH

#include "NTG4RunManager.hh"
#include "G4UserSteppingAction.hh"
#include "G4Step.hh"
#include "globals.hh"

class NTSteppingAction : public G4UserSteppingAction
{
  public:
    NTSteppingAction();
    NTSteppingAction(NTG4RunManager *man);
    virtual ~NTSteppingAction() {}

    virtual void UserSteppingAction(const G4Step*);

  private:
    NTG4RunManager *fRunManager = nullptr;
};

#endif
