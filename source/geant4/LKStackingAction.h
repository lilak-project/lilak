#ifndef LKSTACKINGACTION_HH
#define LKSTACKINGACTION_HH

#include "G4ClassificationOfNewTrack.hh"
#include "LKG4RunManager.h"

class LKStackingAction : public G4UserStackingAction
{
    public:
        LKStackingAction();
        LKStackingAction(LKG4RunManager *man);
        virtual ~LKStackingAction() {}

    protected:
        G4StackManager * stackManager;

    public:
        G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track*);
        void NewStage();
        void PrepareNewEvent();

    private:
        LKG4RunManager *fRunManager = nullptr;
};

#endif
