#ifndef LKCHANNEL_HH
#define LKCHANNEL_HH

#include "LKContainer.hpp"

#include "TObject.h"
#include "TObjArray.h"
#include "TVector3.h"

class LKChannel : public LKContainer
{
    public:
        LKChannel();
        virtual ~LKChannel() {}

        virtual void Clear(Option_t *option = "");
        //virtual void Print(Option_t *option = "") const;
        virtual void Copy(TObject &obj) const;

        void  SetID(Int_t id);
        Int_t GetID() const;

    protected:
        Int_t fID = -1;

        ClassDef(LKChannel, 1)
};

#endif
