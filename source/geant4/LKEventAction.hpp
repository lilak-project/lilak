#ifndef LKEVENTACTION_HH
#define LKEVENTACTION_HH

#include "LKG4RunManager.hpp"
#include "G4UserEventAction.hh"
#include "G4Event.hh"

class LKEventAction : public G4UserEventAction
{
    public:
        LKEventAction();
        LKEventAction(LKG4RunManager *man);
        virtual ~LKEventAction() {}

        virtual void EndOfEventAction(const G4Event* event);

    private:
        LKG4RunManager *fRunManager = nullptr;
};

#endif
