#ifndef LKWPOINT_HH
#define LKWPOINT_HH

#include "TVector3.h"
#include "LKContainer.hpp"

/// position data with weight

class LKWPoint : public LKContainer
{
    public:
        LKWPoint();
        LKWPoint(Double_t x, Double_t y, Double_t z, Double_t w = 1);

        virtual void Print(Option_t *option = "") const;
        virtual void Clear(Option_t *option = "");
        virtual void Copy (TObject &object) const;

        void Set(Double_t x, Double_t y, Double_t z, Double_t w = 1);
        void SetW(Double_t w);
        void SetPosition(Double_t x, Double_t y, Double_t z);

        TVector3 GetPosition() const { return  TVector3(fX,fY,fZ); }

        Double_t x() const { return fX; }
        Double_t y() const { return fY; }
        Double_t z() const { return fZ; }
        Double_t w() const { return fW; }

        Double_t X() const { return fX; }
        Double_t Y() const { return fY; }
        Double_t Z() const { return fZ; }
        Double_t W() const { return fW; }

    protected:
        Double_t fX;
        Double_t fY;
        Double_t fZ;
        Double_t fW;

    ClassDef(LKWPoint, 0)
};

#endif
