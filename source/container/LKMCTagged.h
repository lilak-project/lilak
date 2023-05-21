#ifndef LKMCTAGGED_HH
#define LKMCTAGGED_HH

#include "LKContainer.h"

class LKMCTagged : public LKContainer
{
    protected:
        Int_t fMCID = -1;
        Double_t fMCPurity = -1; //!

    public:
        LKMCTagged();
        virtual ~LKMCTagged();

        virtual void Clear(Option_t *option = "");
        virtual void Copy (TObject &object) const;

        void SetMCID(Int_t id);
        void SetMCPurity(Double_t purity);

        void SetMCTag(Int_t id, Double_t purity);

        Int_t GetMCID() const { return fMCID; }
        Double_t GetMCPurity() const { return fMCPurity; } /// mm

        virtual void PropagateMC() {};

    ClassDef(LKMCTagged, 1)
};

#endif
