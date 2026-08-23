#include "LKComW1Channel.h"

ClassImp(LKComW1Channel);

LKComW1Channel::LKComW1Channel()
{
    Clear();
}

void LKComW1Channel::Clear(Option_t *option)
{
    LKComChannel::Clear(option);
    fSide = false;
    fStrip = -1;
}

void LKComW1Channel::Set(short detID, bool side, int strip, ULong64_t time, short energy, short energyShort, double ecal)
{
    LKComChannel::Set(detID,time,energy,energyShort,ecal);
    fSide = side;
    fStrip = strip;
}
