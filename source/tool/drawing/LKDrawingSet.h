#ifndef LKDRAWINGSET_HH
#define LKDRAWINGSET_HH

#include "TObjArray.h"
#include "LKDrawing.h"
#include "LKDrawingGroup.h"

class LKDrawingSet : public TObjArray
{
    public:
        LKDrawingSet();
        ~LKDrawingSet() {}

        virtual void Draw(Option_t *option="");
        virtual void Print(Option_t *option="") const;

        void AddDrawing(LKDrawing* drawing);
        void AddGroup(LKDrawingGroup* group);
        void AddSet(LKDrawingSet* set);
        LKDrawingGroup* CreateGroup(TString name);

        TH1* FindHist(TString name);
        LKDrawing* FindDrawing(TString name, TString option="");

        virtual Int_t Write(const char *name = nullptr, Int_t option=TObject::kSingleKey, Int_t bufsize = 0) const;


    ClassDef(LKDrawingSet, 1)
};

#endif
