#include "LKPadInteractive.h"
#include "LKPadInteractiveManager.h"

ClassImp(LKPadInteractive);

void LKPadInteractive::AddInteractivePad(TVirtualPad* pad, TString option)
{
    LKPadInteractiveManager::GetManager() -> Add(this,pad,option);
}
