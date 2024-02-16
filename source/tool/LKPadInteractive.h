#ifndef LKPADINTERACTIVE_HH
#define LKPADINTERACTIVE_HH

#include "TObject.h"
#include "TVirtualPad.h"

class LKPadInteractive : public TObject
{
    public:
        LKPadInteractive() {};
        virtual ~LKPadInteractive() {};

        void SetPadInteractiveID(int id) { fPadInteractiveID = id; }
        int GetPadInteractiveID() const { return fPadInteractiveID; }

        virtual void ExecMouseClickEventOnPad(TVirtualPad *pad, double xOnClick, double yOnClick) = 0;

    protected:
        int fPadInteractiveID = -1;

    ClassDef(LKPadInteractive, 1)
};

#endif
