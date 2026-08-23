#include "LKComChannel.h"

ClassImp(LKComChannel);

LKComChannel::LKComChannel()
{
    Clear();
}

void LKComChannel::Clear(Option_t*)
{
    fDetectorID = -1;
    fTime = 0;
    fEnergy = -1;
    fEnergyShort = -1;
    fECal = -1;
}

void LKComChannel::Set(short detID, ULong64_t time, short energy, short energyShort, double ecal)
{
    fDetectorID = detID;
    fTime = time;
    fEnergy = energy;
    fEnergyShort = energyShort;
    fECal = ecal;
}
