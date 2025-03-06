#ifndef LKGEOLINE_HH
#define LKGEOLINE_HH

#include "TVector3.h"
#include "TArrow.h"

#include "LKVector3.h"
#include "LKGeometry.h"
#include "LKGeoPlaneWithCenter.h"
#include "TGraph2D.h"

class LKGeoBox;
class LKGeoPlaneWithCenter;

/**
 * (x-x1)/v_x = (y-y1)/v_y = (z-z1)/v_z = t
 */
class LKGeoLine : public LKGeometry
{
    public:
        LKGeoLine();
        LKGeoLine(Double_t x1, Double_t y1, Double_t z1, Double_t x2, Double_t y2, Double_t z2);
        LKGeoLine(TVector3 pos1, TVector3 pos2);
        virtual ~LKGeoLine() {}

        void Print(Option_t *option="") const;

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

        Double_t GetT() const;

        TVector3 GetPoint1() const;
        TVector3 GetPoint2() const;

        TVector3 GetPointAtX(double x) const;
        TVector3 GetPointAtY(double y) const;
        TVector3 GetPointAtZ(double z) const;
        TVector3 GetPointAtT(double t) const;

        TVector3 Direction() const; ///< 1 to 2

        Double_t Length(Double_t x, Double_t y, Double_t z) const;
        Double_t Length(TVector3 position) const;
        Double_t Length() const;

        void ClosestPointOnLine(Double_t x, Double_t y, Double_t z, Double_t &x0, Double_t &y0, Double_t &z0) const;
        TVector3 ClosestPointOnLine(TVector3 pos) const;

        TVector3 GetCrossingPoint(LKGeoPlaneWithCenter plane2) const;

        //bool LimitXMin(double xMin);
        //bool LimitYMin(double yMin);
        //bool LimitZMin(double zMin);
        //bool LimitXMax(double xMax);
        //bool LimitYMax(double yMax);
        //bool LimitZMax(double zMax);

        Double_t DistanceToLine(Double_t x, Double_t y, Double_t z) const;
        Double_t DistanceToLine(TVector3 pos) const;

        bool SetRange(LKGeoBox* box);

        virtual TGraph* GetGraph(TVector3 offset=TVector3(0,0,0));

        TArrow *GetArrowXY(Double_t asize = 0.02);
        TArrow *GetArrowYZ(Double_t asize = 0.02);
        TArrow *GetArrowZY(Double_t asize = 0.02);
        TArrow *GetArrowZX(Double_t asize = 0.02);
        TArrow *GetArrowXZ(Double_t asize = 0.02);

        TGraph2D* GetGraphXYZ();
        TGraph2D* GetGraphZXY();

    protected:
        Double_t fX1 = 0;
        Double_t fY1 = 0;
        Double_t fZ1 = 0;

        Double_t fX2 = 0;
        Double_t fY2 = 0;
        Double_t fZ2 = 0;


        ClassDef(LKGeoLine, 1)
};

#endif
