#ifndef LKTPCHIT_HH
#define LKTPCHIT_HH

#include "LKHit.hpp"
#include "LKContainer.hpp"

#ifdef ACTIVATE_EVE
#include "TEveElement.h"
#endif
#include "TVector3.h"
#include "TMath.h"
#include "TF1.h"

#include <vector>
using namespace std;

class LKTpcHit : public LKHit
{
    protected:
        Int_t fPadID   = -1;
        Int_t fSection = -999;
        Int_t fRow     = -999;
        Int_t fLayer   = -999;
        Double_t fTb   = -1;

    public :
        LKTpcHit() { Clear(); };
        virtual ~LKTpcHit() {};

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "at") const;
        virtual void Copy (TObject &object) const;

        virtual void PrintTpcHit() const;
        void CopyFrom(LKTpcHit const *hit);

        void AddHit(LKTpcHit *hit); // @todo

        virtual TF1 *GetPulseFunction(Option_t *option = "");

        void SetPadID(Int_t id);
        void SetSection(Int_t section);
        void SetRow(Int_t row);
        void SetLayer(Int_t layer);
        void SetTb(Double_t tb);

        Int_t GetPadID() const;
        Int_t GetSection() const;
        Int_t GetRow() const;
        Int_t GetLayer() const;
        Double_t GetTb() const;

        void SetSortByLayer(bool sortEarlierIfSmaller);

#ifdef ACTIVATE_EVE
        virtual TEveElement *CreateEveElement();
#endif

        ClassDef(LKTpcHit, 1)
};

class LKHitSortSectionRow {
    public:
        LKHitSortSectionRow();
        bool operator() (LKTpcHit* h1, LKTpcHit* h2) {
            if (h1 -> GetSection() == h2 -> GetSection())
                return h1 -> GetRow() < h2 -> GetRow();
            return h1 -> GetSection() < h2 -> GetSection();
        }
};

class LKHitSortSectionLayer {
    public:
        LKHitSortSectionLayer();
        bool operator() (LKTpcHit* h1, LKTpcHit* h2) {
            if (h1 -> GetSection() == h2 -> GetSection())
                return h1 -> GetLayer() < h2 -> GetLayer();
            return h1 -> GetSection() < h2 -> GetSection();
        }
};

#endif
