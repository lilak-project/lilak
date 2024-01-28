#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4SystemOfUnits.hh"
#include <G4strstreambuf.hh>

#include "LKPrimaryGeneratorAction.h"
#include "LKG4RunManager.h"

LKPrimaryGeneratorAction::LKPrimaryGeneratorAction()
{
    fParticleGun = new G4ParticleGun();
}

LKPrimaryGeneratorAction::LKPrimaryGeneratorAction(const char *fileName)
{
    fParticleGun = new G4ParticleGun();
    fEventGenerator = new LKMCEventGenerator(fileName);
    fReadMomentumOrEnergy = fEventGenerator -> ReadMomentumOrEnergy();
}

LKPrimaryGeneratorAction::~LKPrimaryGeneratorAction()
{
    delete fParticleGun;
}

void LKPrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{
    G4int pdg;
    G4double vx, vy, vz, px, py, pz;

    fEventGenerator -> ReadNextEvent(vx, vy, vz);

    fParticleGun -> SetParticlePosition(G4ThreeVector(vx,vy,vz));

    if (fPrintExampleEvent)
        g4man_info << "Printing just for first event v(" << vx <<","<< vy <<","<< vz <<")"<< endl;

    while (fEventGenerator -> ReadNextTrack(pdg, px, py, pz))
    {
        G4ParticleDefinition* particle = G4ParticleTable::GetParticleTable() -> FindParticle(pdg);
        fParticleGun -> SetParticleDefinition(particle);

        G4ThreeVector momentum(px,py,pz);
        fParticleGun -> SetParticleMomentumDirection(momentum.unit());

        G4strstreambuf* oldBuffer = dynamic_cast<G4strstreambuf*>(G4cout.rdbuf(0));
        // Removing print outs in between here ------------->
        if (fReadMomentumOrEnergy) fParticleGun -> SetParticleMomentum(momentum.mag()*MeV);
        else                       fParticleGun -> SetParticleEnergy(momentum.mag()*MeV);
        // <------------- to here
        G4cout.rdbuf(oldBuffer);

        if (fPrintExampleEvent)
            g4man_info << "== " << pdg << "(" << px <<","<< py <<","<< pz <<")"<< endl;

        fParticleGun -> GeneratePrimaryVertex(anEvent);
    }

    fPrintExampleEvent = false;
}

bool LKPrimaryGeneratorAction::SetEventGenerator(const char *fileName)
{
    fEventGenerator = new LKMCEventGenerator(fileName);
    fReadMomentumOrEnergy = fEventGenerator -> ReadMomentumOrEnergy();
    auto numEvents = fEventGenerator -> GetNumEvents();
    ((LKG4RunManager *) LKG4RunManager::GetRunManager()) -> SetNumEvents(numEvents);
    if (numEvents==0)
        return false;
    return true;
}
