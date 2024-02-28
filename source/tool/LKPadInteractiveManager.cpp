#include "LKPadInteractiveManager.h"
#include "LKLogger.h"
#include "TCanvas.h"
#include "TH1.h"
#include "TH2.h"

using namespace std;

ClassImp(LKPadInteractiveManager)

LKPadInteractiveManager* LKPadInteractiveManager::fInstance = nullptr;

LKPadInteractiveManager* LKPadInteractiveManager::GetManager()
{
    if (fInstance != nullptr)
        return fInstance;
    return new LKPadInteractiveManager();
}

LKPadInteractiveManager::LKPadInteractiveManager()
{
    fInstance = this;
    fPadInteractiveArray = new TObjArray();
}

void LKPadInteractiveManager::Add(LKPadInteractive* interactive)
{
    auto interactiveFound = (LKPadInteractive*) fPadInteractiveArray -> FindObject(interactive);
    if (interactiveFound==nullptr) {
        interactive -> SetPadInteractiveID(fCountPadInteractives);
        ++fCountPadInteractives;
        fPadInteractiveArray -> Add(interactive); 
    }
}

void LKPadInteractiveManager::Add(LKPadInteractive* interactive, TVirtualPad* pad, TString option)
{
    auto interactiveFound = (LKPadInteractive*) fPadInteractiveArray -> FindObject(interactive);
    if (interactiveFound==nullptr) {
        interactive -> SetPadInteractiveID(fCountPadInteractives);
        ++fCountPadInteractives;
        fPadInteractiveArray -> Add(interactive); 
    }
    auto interactiveID = interactive -> GetPadInteractiveID();
    if (pad -> GetUniqueID() != interactiveID) {
        pad -> AddExec("MouseClickEventOnPad", "LKPadInteractiveManager::MouseClickEventOnPad()");
        pad -> SetUniqueID(interactiveID); 
    }
}

void LKPadInteractiveManager::MouseClickEventOnPad()
{
    if (gPad==nullptr)
        return;

    TObject* select = gPad -> GetCanvas() -> GetClickSelected();
    if (select == nullptr) {
        gPad -> GetCanvas() -> SetClickSelected(nullptr);
        return;
    }

    bool isNotH2 = !(select -> InheritsFrom(TH2::Class()));
    if (isNotH2) {
        gPad -> GetCanvas() -> SetClickSelected(nullptr);
        return;
    }

    int interactiveID = gPad -> GetUniqueID();

    int xEvent = gPad -> GetEventX();
    int yEvent = gPad -> GetEventY();
    int xAbs = gPad -> AbsPixeltoX(xEvent);
    int yAbs = gPad -> AbsPixeltoY(yEvent);
    double xOnClick = gPad -> PadtoX(xAbs);
    double yOnClick = gPad -> PadtoY(yAbs);

    gPad -> GetCanvas() -> SetClickSelected(nullptr);
    LKPadInteractiveManager::GetManager() -> GetPadInteractive(interactiveID) -> ExecMouseClickEventOnPad(gPad, xOnClick, yOnClick);
}
