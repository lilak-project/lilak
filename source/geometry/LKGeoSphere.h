#ifndef LKGEOSPHERE_HH
#define LKGEOSPHERE_HH

#include "TVector3.h"
#include "TGraph.h"

#include "LKVector3.h"
#include "LKGeometry.h"
#include "LKGeoCircle.h"

class LKGeoSphere : public LKGeometry
{
    public:
        LKGeoSphere();
        LKGeoSphere(Double_t x, Double_t y, Double_t z, Double_t r);
        LKGeoSphere(TVector3 pos, Double_t r);
        virtual ~LKGeoSphere() {}

        void SetSphere(Double_t x, Double_t y, Double_t z, Double_t r);
        void SetSphere(TVector3 pos, Double_t r);

        Double_t GetX() const;
        Double_t GetY() const;
        Double_t GetZ() const;
        Double_t GetR() const;

        virtual TVector3 GetCenter() const;
        Double_t GetRadius() const;

        TGraph *GetCircleXY(Int_t n = 100, Double_t theta1 = 0, Double_t theta2 = 0);
        TGraph *GetCircleYZ(Int_t n = 100, Double_t theta1 = 0, Double_t theta2 = 0);
        TGraph *GetCircleZX(Int_t n = 100, Double_t theta1 = 0, Double_t theta2 = 0);

        TVector3 StereographicProjection(Double_t x, Double_t y);

    protected:
        Double_t fX = 0;
        Double_t fY = 0;
        Double_t fZ = 0;
        Double_t fR = 0;

        LKGeoCircle *circle = nullptr;

    ClassDef(LKGeoSphere, 1)
};

#endif
