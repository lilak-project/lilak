#ifndef LKGEOPLANEWITHCENTER_HH
#define LKGEOPLANEWITHCENTER_HH

#include "LKGeoPlane.h"
#include "LKGeoLine.h"

class LKGeoLine;
class LKGeo2DBox;

class LKGeoPlaneWithCenter : public LKGeoPlane
{
    public:
        LKGeoPlaneWithCenter();
        LKGeoPlaneWithCenter(TVector3 pos, TVector3 nnn);
        LKGeoPlaneWithCenter(Double_t x, Double_t y, Double_t z, Double_t xn, Double_t yn, Double_t zn);
        virtual ~LKGeoPlaneWithCenter() {}

        void Print(Option_t *option="") const;

        virtual void SetPlane(TVector3 pos, TVector3 nnn);
        virtual void SetPlane(Double_t x, Double_t y, Double_t z, Double_t xn, Double_t yn, Double_t zn);

        TVector3 GetCenter() const;

        Double_t GetX() const;
        Double_t GetY() const;
        Double_t GetZ() const;

        TVector3 GetVectorU() const;
        TVector3 GetVectorV() const;

        LKGeoLine GetCrossSectionLine(LKGeoPlaneWithCenter plane2) const;

        /*
         * Calculate intersection point between current plane and line.
         * The calculation will yield three possible outcomes.
         * - The line does not intersect the plane: return 0
         * - The line lies in the plane: return 2
         * - The line has intersection with the plane: returnr1 and set intersection
         */
        int Intersection(LKGeoLine line, TVector3 &intersection);

    protected:
        Double_t fX = 0;
        Double_t fY = 0;
        Double_t fZ = 0;

        ClassDef(LKGeoPlaneWithCenter, 1)
};

#endif
