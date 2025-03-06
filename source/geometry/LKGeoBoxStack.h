#ifndef LKGEOBOXSTACK_HH
#define LKGEOBOXSTACK_HH

#include "TVector3.h"
#include "TGraph.h"
#include "TMultiGraph.h"
#include "TH2D.h"
#include "TH2Poly.h"

#include "LKVector3.h"
#include "LKGeometry.h"
#include "LKGeoBox.h"

typedef LKVector3::Axis axis_t;

class LKGeoBoxStack : public LKGeometry
{
    public:
        LKGeoBoxStack();
        LKGeoBoxStack(Double_t x,  Double_t y,  Double_t z,
                Double_t dx, Double_t dy, Double_t dz,
                Int_t n,     axis_t as, axis_t af = LKVector3::kZ);

        virtual ~LKGeoBoxStack() {}

        virtual void Print(Option_t *option = "") const;

        virtual TVector3 GetCenter() const;

        void SetBoxStack(Double_t x,  Double_t y,  Double_t z,
                Double_t dx, Double_t dy, Double_t dz,
                Int_t n,     axis_t as, axis_t af = LKVector3::kZ);

        axis_t GetStackAxis() const;
        axis_t GetFaceAxis() const;

        Double_t GetStackAxisCenter() const;
        Double_t GetFaceAxisCenter() const;
        Double_t GetLongAxisCenter() const;

        Double_t GetStackAxisMax() const;
        Double_t GetFaceAxisMax() const;
        Double_t GetLongAxisMax() const;

        Double_t GetStackAxisMin() const;
        Double_t GetFaceAxisMin() const;
        Double_t GetLongAxisMin() const;

        Double_t GetStackAxisDisplacement() const;
        Double_t GetFaceAxisDisplacement() const;
        Double_t GetLongAxisDisplacement() const;

        LKGeoBox GetBox(Int_t idx) const;

        TMultiGraph *GetStackGraph   (axis_t a1 = LKVector3::kNon, axis_t a2 = LKVector3::kNon);
        TH2D        *GetStackHist    (TString name="", TString title="", axis_t a1 = LKVector3::kNon, axis_t a2 = LKVector3::kNon);
        TH2Poly     *GetStackHistPoly(TString name="", TString title="", axis_t a1 = LKVector3::kNon, axis_t a2 = LKVector3::kNon);

        Int_t FindBoxIndex(TVector3 pos) const;
        Int_t FindBoxIndex(Double_t x, Double_t y, Double_t z) const;

    protected:
        Double_t fX;
        Double_t fY;
        Double_t fZ;
        Double_t fdX;
        Double_t fdY;
        Double_t fdZ;
        Int_t fNumStacks;
        axis_t fStackAxis;
        axis_t fFaceAxis;

    ClassDef(LKGeoBoxStack, 1)
};

#endif
