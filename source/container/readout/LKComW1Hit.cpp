#include "LKComW1Hit.h"

ClassImp(LKComW1Hit);

LKComW1Hit::LKComW1Hit()
{
    Clear();
}

void LKComW1Hit::Clear(Option_t *option)
{
    LKComHit::Clear(option);
    fX = -1;
    fY = -1;
}
