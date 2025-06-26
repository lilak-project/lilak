#ifndef LKMCTAG_HH
#define LKMCTAG_HH

#include <vector>

#include "LKContainer.h"

class LKMCTag : public LKContainer
{
    protected:
        std::vector<Int_t> fIndex;  // Index in LKChannel
        std::vector<Int_t> fMCID;   // Truth track ID
        std::vector<Double_t> fWeight; // Truth track weight within same index

    public:
        LKMCTag();
        virtual ~LKMCTag();

        virtual void Clear(Option_t *option = "");
        virtual void Copy (TObject &object) const;

        void AddMCTag(Int_t mcId, Int_t index=-1);
        void AddMCWeightTag(Int_t mcId, Double_t weight, Int_t index=-1);

        Int_t GetMCNum(int index=-1);
        Int_t GetMCID(int mcIdx=0, int index=-1);
        Double_t GetMCPurity(int mcIdx=0, int index=-1);

    ClassDef(LKMCTag, 1)
};

#endif