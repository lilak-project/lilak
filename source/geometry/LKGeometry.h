#ifndef LKGEOMETRY_HH
#define LKGEOMETRY_HH

#include "LKContainer.h"
#include "TGraph.h"
#include "TVector3.h"

class LKGeometry
{
    protected:
        Double_t fRMS = -1;

    public:
        LKGeometry() {}
        virtual ~LKGeometry() {}

        void SetRMS(Double_t val) { fRMS = val; }
        Double_t GetRMS() const { return fRMS; }

        TGraph *GetGraph(TVector3 offset) { return (TGraph*) nullptr; }
        TGraph *GetGraph(TVector3 offset, TString option);

        ClassDef(LKGeometry, 1)
};

#endif
