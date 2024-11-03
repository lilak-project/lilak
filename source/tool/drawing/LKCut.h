#ifndef LKCUT_HH
#define LKCUT_HH

#include "TCut.h"
#include "TCutG.h"
#include "TNamed.h"
#include "TObjArray.h"

#include <vector>
using namespace std;

class LKCut : public TNamed
{
    public:
        LKCut(const char* name="cut", const char* title="");
        virtual ~LKCut() {}

        virtual void Draw(Option_t *option="");
        virtual void Draw(double x1, double x2);
        virtual void Print(Option_t *option="") const;

        bool IsActive();
        TObject* At(Int_t idx) const;

        void Add(TCut cut, bool apply=true);
        void Add(TCut* cut, bool apply=true);
        void Add(TCutG* cut, bool apply=true);
        void Add(TString cut, bool apply=true) { Add(TCut(cut),apply); }
        void Add(TObject* cut, bool apply=true);
        void Add(LKCut *cuts);

        virtual int IsInsideAnd(Double_t x, Double_t y) const;
        virtual int IsInsideOr(Double_t x, Double_t y) const;
        virtual int IsInside(Double_t x, Double_t y) const { return IsInsideOr(x, y); }
        virtual int IsInside(int i, Double_t x, Double_t y) const;

        TObjArray* GetCutArray() { return fCutArray; }
        vector<int> *GetTypeArray() { return &fTypeArray; }
        vector<bool> *GetActiveArray() { return &fActiveArray; }

        int GetNumCuts() const { return fCutArray->GetEntries(); }

    private:
        //TString fVarX;
        //TString fVarY;

        TObjArray* fCutArray = nullptr;
        vector<int> fTypeArray;
        vector<bool> fActiveArray;

    ClassDef(LKCut, 1)
};

#endif