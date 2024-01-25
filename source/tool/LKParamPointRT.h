#ifndef LKHOUGHPOINTRT_HH
#define LKHOUGHPOINTRT_HH

#include "TClonesArray.h"
#include "TObject.h"
#include "TGraph.h"

//#include "LKContainer.h"
#include "LKLogger.h"
#include "LKGeoLine.h"
#include "LKImagePoint.h"

/**
 * ### Indexing iParamCorner
 * - 0 : center of parameter bin
 * - 1 : left  bottom corner of parameter bin
 * - 2 : left  top    corner of parameter bin
 * - 3 : right bottom corner of parameter bin
 * - 4 : right top    corner of parameter bin
 * - 9rrtt : rr and tt range between 0 - 10. The parameter point will be (r, t) where r = r1 + 0.1*rr*(r2-r1) and t = t1 + 0.1*tt*(t2-t1).
 */
class LKParamPointRT : public TObject
//class LKParamPointRT : public LKContainer
{
    public:
        LKParamPointRT();
        LKParamPointRT(double xc, double yc, double r1, double t1, double r2, double t2, double w=0);
        virtual ~LKParamPointRT() { ; }

        void Clear(Option_t *option="");
        void Print(Option_t *option="") const;
        void Copy(TObject &object) const;

        double GetR0() const  { return fRadius0; }
        double GetR1() const  { return fRadius1; }
        double GetR2() const  { return fRadius2; }
        double GetT0() const  { return fTheta0; }
        double GetT1() const  { return fTheta1; }
        double GetT2() const  { return fTheta2; }
        double GetWeight() const  { return fWeight; }
        TVector3 GetCorner(int iParamCorner) const;

        void SetPoint(double xc, double yc, double r1, double t1, double r2, double t2, double w) { SetTransformCenter(xc,yc); SetRadius(r1,r2); SetTheta(t1,t2); fWeight = w; }
        void SetTransformCenter(double xc, double yc) { fXTransformCenter = xc; fYTransformCenter = yc; }
        void SetRadius(double r1, double r2) { fRadius0 = .5*(r1+r2); fRadius1 = r1; fRadius2 = r2; }
        void SetTheta(double t1, double t2) { fTheta0 = .5*(t1+t2); fTheta1 = t1; fTheta2 = t2; }
        void SetWeight(double weight) { fWeight = weight; }

        double GetCenterR() const;
        double GetCenterT() const;
        bool IsInside(double r, double t);
        LKGeoLine GetGeoLineInImageSpace(int iParamCorner, double x1, double x2, double y1, double y2);
        TGraph* GetLineInImageSpace(int iParamCorner, double x1, double x2, double y1, double y2);
        TGraph* GetRadialLineInImageSpace(int iParamCorner, double angleSize);
        TGraph* GetRibbonInImageSpace(double x1, double x2, double y1, double y2);
        TGraph* GetBandInImageSpace(double x1, double x2, double y1, double y2);
        TGraph* GetRangeGraphInParamSpace(bool drawYX=true);

        double DistanceToPoint(TVector3 point);
        double DistanceToPoint(int iParamCorner, TVector3 point);

        double DistanceToImagePoint(LKImagePoint* imagePoint);
        double DistanceToImagePoint(int iParamCorner, LKImagePoint* imagePoint);
        double CorrelateBoxLine(LKImagePoint* imagePoint);
        double CorrelateBoxRibbon(LKImagePoint* imagePoint);
        double CorrelateBoxBand(LKImagePoint* imagePoint);

        TVector3 GetPOCA(int iParamCorner);
        double EvalX(int iParamCorner, double y) const;
        double EvalY(int iParamCorner, double x) const;
        double EvalX(double y) const { return EvalX(0,y); }
        double EvalY(double x) const { return EvalY(0,x); }
        /// Evaluate two end points of straight line (x1,y1) and (x2,y2) in image space from parameter bin corner of index=iParamCorner (see Indexing iParamCorner).
        /// The position of two end points are evaluated within given x/y-ranges (x1, x2) and (y1, y2).
        void GetImagePoints(int iParamCorner, double &x1, double &x2, double &y1, double &y2);
        LKGeoLine GetGeoLine(int iParamCorner, double x1, double x2, double y1, double y2);

        double       fXTransformCenter;
        double       fYTransformCenter;
        double       fRadius0;
        double       fRadius1;
        double       fRadius2;
        double       fTheta0;
        double       fTheta1;
        double       fTheta2;
        double       fWeight;

    ClassDef(LKParamPointRT,1);
};

#endif
