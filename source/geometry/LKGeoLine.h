#ifndef LKGEOLINE_HH
#define LKGEOLINE_HH

#include "TVector3.h"
#include "TArrow.h"

#include "LKVector3.h"
#include "LKGeometry.h"

class LKGeoLine : public LKGeometry
{
    public:
        LKGeoLine();
        LKGeoLine(Double_t x1, Double_t y1, Double_t z1, Double_t x2, Double_t y2, Double_t z2);
        LKGeoLine(TVector3 pos1, TVector3 pos2);
        virtual ~LKGeoLine() {}

        virtual TVector3 GetCenter() const;

        virtual void SetLine(Double_t x1, Double_t y1, Double_t z1, Double_t x2, Double_t y2, Double_t z2);
        virtual void SetLine(TVector3 pos1, TVector3 pos2);
        virtual void SetLine(LKGeoLine *line);

        Double_t GetX1() const;
        Double_t GetY1() const;
        Double_t GetZ1() const;

        Double_t GetX2() const;
        Double_t GetY2() const;
        Double_t GetZ2() const;

        TVector3 GetPoint1() const;
        TVector3 GetPoint2() const;

        TVector3 Direction() const; ///< 1 to 2

        Double_t Length(Double_t x, Double_t y, Double_t z) const;
        Double_t Length(TVector3 position) const;
        Double_t Length() const;

        void ClosestPointOnLine(Double_t x, Double_t y, Double_t z, Double_t &x0, Double_t &y0, Double_t &z0) const;
        TVector3 ClosestPointOnLine(TVector3 pos) const;

        Double_t DistanceToLine(Double_t x, Double_t y, Double_t z) const;
        Double_t DistanceToLine(TVector3 pos) const;

        TArrow *DrawArrowXY(Double_t asize = 0.02);
        TArrow *DrawArrowYZ(Double_t asize = 0.02);
        TArrow *DrawArrowZY(Double_t asize = 0.02);
        TArrow *DrawArrowZX(Double_t asize = 0.02);
        TArrow *DrawArrowXZ(Double_t asize = 0.02);

    protected:
        Double_t fX1 = 0;
        Double_t fY1 = 0;
        Double_t fZ1 = 0;

        Double_t fX2 = 0;
        Double_t fY2 = 0;
        Double_t fZ2 = 0;


        ClassDef(LKGeoLine, 0)
};

#endif
