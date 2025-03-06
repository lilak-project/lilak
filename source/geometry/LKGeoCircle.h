#ifndef LKGEOCIRCLE_HH
#define LKGEOCIRCLE_HH

#include "TVector3.h"
#include "TGraph.h"

#include "LKVector3.h"
#include "LKGeometry.h"

class LKGeoCircle : public LKGeometry
{
    public:
        LKGeoCircle();
        LKGeoCircle(Double_t x, Double_t y, Double_t r);
        virtual ~LKGeoCircle() {}

        virtual void Print(Option_t *option = "") const;

        virtual TVector3 GetCenter() const;

        void SetCircle(Double_t x, Double_t y, Double_t r);

        TVector3 GetRandomPoint();

        Double_t GetX() const;
        Double_t GetY() const;
        Double_t GetZ() const; //
        Double_t GetR() const;
        Double_t GetRadius() const;

        TGraph *GetGraph(TVector3 offset=TVector3(0,0,0), Int_t n = 100, Double_t theta1 = 0, Double_t theta2 = 0);
        TGraph *GetGraph(Int_t n, Double_t theta1, Double_t theta2) { return GetGraph(TVector3(0,0,0),n,theta1,theta2); }
        virtual TGraph *GetGraph(TVector3 offset) { return GetGraph(offset,100,0,0); }

        TVector3 ClosestPointToCircle(Double_t x, Double_t y);
        TVector3 PointAtPhi(Double_t phi);

        Double_t Phi(Double_t x, Double_t y);

    protected:
        Double_t fX = 0;
        Double_t fY = 0;
        Double_t fZ = 0; // if exist
        Double_t fR = 0;

    ClassDef(LKGeoCircle, 1)
};

#endif
