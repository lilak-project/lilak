#include "LKG4RunMessenger.hh"

LKG4RunMessenger::LKG4RunMessenger(LKG4RunManager *runManager)
: fRunManager(runManager)
{
  fBeamOnAll = new G4UIcmdWithoutParameter("/run/beamOnAll",this);
  fSuppressMessage = new G4UIcmdWithABool("/run/suppressPP",this);
}

LKG4RunMessenger::~LKG4RunMessenger()
{
}

void LKG4RunMessenger::SetNewValue(G4UIcommand *command, G4String newValue)
{
  if (command==fBeamOnAll)
    fRunManager -> BeamOnAll();
  else if (command==fSuppressMessage)
    fRunManager -> SetSuppressInitMessage(fSuppressMessage->GetNewBoolValue(newValue));

}
