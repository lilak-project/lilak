#ifndef LKPRIMARYGENERATORACTION_HH
#define LKPRIMARYGENERATORACTION_HH

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "G4Event.hh"
#include "globals.hh"
#include "LKMCEventGenerator.hh"

class LKPrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
  public:
    LKPrimaryGeneratorAction();
    LKPrimaryGeneratorAction(const char *fileName);
    virtual ~LKPrimaryGeneratorAction();

    // method from the base class
    virtual void GeneratePrimaries(G4Event*);         
  
    // method to access particle gun
    const G4ParticleGun* GetParticleGun() const { return fParticleGun; }

    void SetEventGenerator(const char *fileName);
  
  private:
    G4ParticleGun* fParticleGun;
    LKMCEventGenerator *fEventGenerator;
    bool fReadMomentumOrEnergy;
};

#endif
