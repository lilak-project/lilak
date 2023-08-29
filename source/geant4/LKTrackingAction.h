#ifndef LKTRACKINGACTION_HH
#define LKTRACKINGACTION_HH

//#define LKG4_DEBUG_TRACKINGACTION

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
#ifdef LKG4_DEBUG_TRACKINGACTION
        virtual void PostUserTrackingAction(const G4Track* track);
#endif

    private:
        LKParameterContainer *fProcessTable;

        LKG4RunManager *fRunManager = nullptr;
};

#endif
