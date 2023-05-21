#ifndef LKGEOMETRY_HH
#define LKGEOMETRY_HH

#include "LKContainer.h"

class LKGeometry
{
    protected:
        Double_t fRMS = -1;

    public:
        LKGeometry() {}
        virtual ~LKGeometry() {}

        void SetRMS(Double_t val) { fRMS = val; }
        Double_t GetRMS() const { return fRMS; }

        ClassDef(LKGeometry, 1)
};

#endif
