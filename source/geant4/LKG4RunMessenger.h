#ifndef LKG4RUNMESSENGER_HH
#define LKG4RUNMESSENGER_HH

#include "G4UImessenger.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIcmdWithABool.hh"
#include "LKG4RunManager.h"

class LKG4RunMessenger : public G4UImessenger
{
    public:
        LKG4RunMessenger(LKG4RunManager *);
        virtual ~LKG4RunMessenger();

        void SetNewValue(G4UIcommand * command,G4String newValues);

    private:
        LKG4RunManager *fRunManager;

        G4UIcmdWithoutParameter *fBeamOnAll;
        G4UIcmdWithABool *fSuppressMessage;
};

#endif
