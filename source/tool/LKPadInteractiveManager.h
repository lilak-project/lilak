#ifndef LKPADINTERACTIVEMANAGER_HH
#define LKPADINTERACTIVEMANAGER_HH

#include "TObject.h"
#include "TObjArray.h"
#include "TVirtualPad.h"
#include "LKPadInteractive.h"

class LKPadInteractiveManager : public TObject
{
    public:
        static LKPadInteractiveManager* GetManager(); ///< Get LKPadInteractiveManager static pointer.

        LKPadInteractiveManager();
        virtual ~LKPadInteractiveManager() {};

        void Add(LKPadInteractive* interactive);
        void Add(LKPadInteractive* interactive, TVirtualPad* pad, TString option="");

        LKPadInteractive* GetPadInteractive(int id) { return (LKPadInteractive*) fPadInteractiveArray -> At(id); }

        static void MouseClickEventOnPad();

    private:
        static LKPadInteractiveManager *fInstance;

        TObjArray *fPadInteractiveArray = nullptr;

        int fCountPadInteractives = 0;

    ClassDef(LKPadInteractiveManager, 1)
};

#endif
