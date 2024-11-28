#ifndef LKCUT_HH
#define LKCUT_HH

#include "TCut.h"
#include "TCutG.h"
#include "TNamed.h"
#include "TObjArray.h"

#include <vector>
using namespace std;

class LKParameterContainer;

class LKCut : public TNamed
{
    public:
        LKCut(const char* name="cut", const char* title="");
        virtual ~LKCut() {}

        virtual void Draw(Option_t *option="");
        virtual void Draw(double x1, double x2, double y1, double y2);
        virtual void Print(Option_t *option="") const;

        bool IsActive();
        TObject* At(Int_t idx) const;

        TString MakeTag() { return Form("c%d",GetNumCuts()); }

        void Add(LKCut *cuts);

        void Add(TString tag, TCut cut, bool apply=true);
        void Add(TString tag, TCut* cut, bool apply=true);
        void Add(TString tag, TCutG* cut, bool apply=true);
        void Add(TString tag, TString cut, bool apply=true);
        void Add(TString tag, TObject* cut, bool apply=true);

        void Add(TCut cut, bool apply=true) { Add("",cut,apply); }
        void Add(TCut* cut, bool apply=true) { Add("",cut,apply); }
        void Add(TCutG* cut, bool apply=true) { Add("",cut,apply); }
        void Add(TString cut, bool apply=true) { Add("",cut,apply); }
        void Add(TObject* cut, bool apply=true) { Add("",cut,apply); }

        void AddPar(LKParameterContainer* par);

        int IsInsideAnd(Double_t x, Double_t y) const;
        int IsInsideOr(Double_t x, Double_t y) const;
        int IsInsideSingle(int i, Double_t x, Double_t y) const;
        int IsInside(Double_t x, Double_t y) const;

        TString GetCutString(int i, TString varX, TString varY);
        TString MakeCutStringDelim(TString varX, TString varY, TString delim);
        TString MakeCutStringOr(TString varX, TString varY) { return MakeCutStringDelim(varX, varY, "||"); }
        TString MakeCutStringAnd(TString varX, TString varY) { return MakeCutStringDelim(varX, varY, "&&"); }
        TString MakeCutString(TString varX, TString varY, TString expression="");

        TObjArray* GetCutArray() { return fCutArray; }
        vector<int> *GetTypeArray() { return &fTypeArray; }
        vector<bool> *GetActiveArray() { return &fActiveArray; }

        int GetNumCuts() const { return fCutArray->GetEntries(); }
        TString GetExpression() const { return fExpression; }

    private:
        TObjArray* fCutArray = nullptr;
        vector<TString> fTagArray;
        vector<int> fTypeArray;
        vector<bool> fActiveArray;
        TString fExpression;

    private:
        const int kTypeTCut = 1; //! TCut type
        const int kTypeCutG = 2; //! TCutG type

    ClassDef(LKCut, 1)
};

#endif
