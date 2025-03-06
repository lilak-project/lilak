#ifndef LKGEOPOLYGON_HH
#define LKGEOPOLYGON_HH

#include "TVector3.h"
#include "TGraph.h"
#include "LKGeometry.h"

class LKGeoPolygon : public LKGeometry
{
    public:
        LKGeoPolygon();
        LKGeoPolygon(double x, double y, double r, double n, double theta0=90);
        virtual ~LKGeoPolygon() {}

        //virtual void Print(Option_t *option = "") const;
        void SetPolygon(double x, double y, double r, double n, double theta0=90);

        /// Set the radius based on the inscribed cirble instead of the circumscribe circle.
        void SetRMin(double r);

        double GetX() const { return fX; }
        double GetY() const { return fY; }
        double GetR() const { return fR; }
        double GetRadius() const { return fR; }
        double GetN() const { return fN; }
        double GetNVertex() const { return fN; }
        double GetT() { return fT; }
        double GetTheta0() { return fT; }

        virtual TGraph *GetGraph(TVector3 offset=TVector3(0,0,0));
        TVector3 GetCenter() { return TVector3(fX,fY,0); }
        TVector3 GetPoint(int i);

    protected:
        double fX = 0;
        double fY = 0;
        double fR = 100;
        double fN = 8;
        double fT = 90;

    ClassDef(LKGeoPolygon, 1)
};

#endif
