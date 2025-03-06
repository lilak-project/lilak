#ifndef LKGEOBOX_HH
#define LKGEOBOX_HH

#include "TVector3.h"
#include "TGraph.h"

#include "LKVector3.h"
#include "LKGeoRotated.h"
#include "LKGeoLine.h"
#include "LKGeo2DBox.h"

typedef LKVector3::Axis axis_t;

class LKGeoBox : public LKGeoRotated
{
    public:
        LKGeoBox();
        LKGeoBox(Double_t xc, Double_t yc, Double_t zc, Double_t dx, Double_t dy, Double_t dz);
        LKGeoBox(TVector3 center, TVector3 length);
        virtual ~LKGeoBox() {}

        virtual void Print(Option_t *option = "") const;
        virtual void Clear(Option_t *option = "");
        virtual void Copy(LKGeoBox *box) const;

        virtual void SetBox(Double_t xc, Double_t yc, Double_t zc, Double_t dx, Double_t dy, Double_t dz);
        virtual void SetBox(TVector3 center, TVector3 length);

        TVector3 GetCenter() const;
        Double_t GetdX() const;
        Double_t GetdY() const;
        Double_t GetdZ() const;

        TVector3 GetCorner(Int_t idx) const;
        TVector3 GetCorner(Int_t xpm, Int_t ypm, Int_t zpm) const; ///< pm should be 1(high) or -1(low)

        LKGeoLine GetEdge(Int_t idx) const;
        LKGeoLine GetEdge(Int_t idxCorner1, Int_t idxCorner2) const;
        LKGeoLine GetEdge(Int_t xpm, Int_t ypm, Int_t zpm) const; ///< pm of edge axis is 0 while the other two should be 1(high) or -1(low)

        LKGeo2DBox GetFace(axis_t xaxis, axis_t yaxis) const;
        //LKGeo2DBox GetFace(Int_t axis) const;

        LKGeoPlaneWithCenter GetPlane(axis_t xaxis, axis_t yaxis) const;
        /// +X  = 0
        /// -X  = 1
        /// +Y  = 2
        /// -Y  = 3
        /// +Z  = 4
        /// -Z  = 5
        LKGeoPlaneWithCenter GetPlane(Int_t axis) const;

        bool TestPointInsidePlane(int iPlane, TVector3 point) const;
        bool GetCrossingPoints(LKGeoLine line, TVector3 &point1,TVector3 &point2) const;

        TGraph *Get2DBoxGraph(axis_t axis1 = LKVector3::kX, axis_t axis2 = LKVector3::kY);

        bool IsInside(TVector3 pos) const;
        bool IsInside(Double_t x, Double_t y, Double_t z) const;

    protected:
        Double_t fX;
        Double_t fY;
        Double_t fZ;
        Double_t fdX;
        Double_t fdY;
        Double_t fdZ;

    ClassDef(LKGeoBox, 1)
};

#endif
