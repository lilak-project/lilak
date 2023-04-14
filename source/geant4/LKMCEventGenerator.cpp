#include "LKMCEventGenerator.hpp"
#include "LKG4RunManager.hpp"
#include "LKMCTrack.hpp"
#include "TSystem.h"

LKMCEventGenerator::LKMCEventGenerator()
{
}

LKMCEventGenerator::LKMCEventGenerator(TString fileName)
{
    fInputFile.open(fileName.Data());

    TString me;
    fInputFile >> me;
    me.ToLower();

    if (me == "p") fReadMomentumOrEnergy = true;
    else if (me == "e") fReadMomentumOrEnergy = false;

    fInputFile >> fNumEvents;

    g4gen_info << fileName << " containing " << fNumEvents << " events, initialized with " << me << endl;
}

LKMCEventGenerator::~LKMCEventGenerator()
{
    if(fInputFile.is_open()) fInputFile.close();
}

bool LKMCEventGenerator::ReadNextEvent(Double_t &vx, Double_t &vy, Double_t &vz)
{
    Int_t eventID;
    if (!(fInputFile >> eventID >> fNumTracks >> vx >> vy >> vz))
        return false;

    fCurrentTrackID = 0;
    return true;
}

bool LKMCEventGenerator::ReadNextTrack(Int_t &pdg, Double_t &px, Double_t &py, Double_t &pz)
{
    if (fCurrentTrackID >= fNumTracks)
        return false;

    fInputFile >> pdg >> px >> py >> pz;
    fCurrentTrackID++;

    return true;
}
