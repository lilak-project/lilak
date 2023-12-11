#ifndef LKEVENTHEADER_HH
#define LKEVENTHEADER_HH

#include "LKContainer.h"

class LKEventHeader : public LKContainer
{
    public:
        LKEventHeader();
        virtual ~LKEventHeader() { ; }

        virtual void Clear(Option_t *option="");
        virtual void Print(Option_t *option="") const;
        virtual void Copy(TObject &object) const;

        Int_t  GetEventNumber() const  { return fEventNumber; }
        Bool_t IsGoodEvent()    const  { return fIsGoodEvent; }

        void SetEventNumber(Int_t eventNumber) { fEventNumber = eventNumber; }
        void SetIsGoodEvent(Bool_t isGoodEvent) { fIsGoodEvent = isGoodEvent; }

    protected:
        Int_t   fEventNumber = -1;
        Bool_t  fIsGoodEvent = false;

    ClassDef(LKEventHeader,1);
};

#endif
