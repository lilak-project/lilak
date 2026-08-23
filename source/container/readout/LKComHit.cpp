#include "LKComHit.h"

ClassImp(LKComHit);

LKComHit::LKComHit()
{
    Clear();
}

void LKComHit::Clear(Option_t*)
{
    fDetectorID = -1;
    fChannelMult = 0;
    fTime = 0;
    fEnergy = -1;
    fECal = -1;
}
