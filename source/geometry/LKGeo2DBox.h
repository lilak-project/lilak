#ifndef LKGEO2DBOX_HH
#define LKGEO2DBOX_HH

#include "TVector3.h"
#include "TGraph.h"

#include "LKGeoRotated.h"
#include "LKGeoLine.h"

class LKGeo2DBox : public LKGeoRotated
{
    public:
        LKGeo2DBox();
        LKGeo2DBox(Double_t x1, Double_t x2, Double_t y1, Double_t y2);
        LKGeo2DBox(Double_t xc, Double_t yc, Double_t dx, Double_t dy, Double_t rotation);
        virtual ~LKGeo2DBox() {}

        virtual void Print(Option_t *option = "") const;

        virtual TVector3 GetCenter() const;

        void Set2DBox(Double_t x1, Double_t x2, Double_t y1, Double_t y2);
        void Set2DBox(Double_t xc, Double_t yc, Double_t dx, Double_t dy, Double_t rotation);

        Double_t GetXCenter() const;
        Double_t GetdX() const;
        Double_t GetX(Int_t idx) const;

        Double_t GetYCenter() const;
        Double_t GetdY() const;
        Double_t GetY(Int_t idx) const;

        TVector3 GetCorner(Int_t idx) const;
        TVector3 GetCorner(Int_t xpm, Int_t ypm) const;

        LKGeoLine GetEdge(Int_t idx) const;
        LKGeoLine GetEdge(Int_t xpm, Int_t ypm) const; ///< pm of edge axis is 0 while the other should be 1(high) or -1(low)

        void Translate(Double_t x, Double_t y);
        void Rotate(Double_t deg, Double_t x, Double_t y);

        virtual TGraph *GetGraph(TVector3 center=TVector3(0,0,0));

        //bool IsInside(Double_t x, Double_t y);

    protected:
        Double_t fX[4];
        Double_t fY[4];

    ClassDef(LKGeo2DBox, 1)
};

#endif
