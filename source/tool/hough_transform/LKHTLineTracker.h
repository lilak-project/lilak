#ifndef LKHOUGHTRANSFORMTRACKER_HH
#define LKHOUGHTRANSFORMTRACKER_HH

#include <vector>
using namespace std;

#include "LKLogger.h"
#include "LKGeoLine.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TPad.h"

#include "LKImagePoint.h"
#include "LKParamPointRT.h"
#include "LKLinearTrack.h"
#include "LKODRFitter.h"
#include "LKHit.h"
#include "LKVector3.h"
#include "LKPadInteractive.h"

class LKHTWeightingFunction
{
    public:
        LKHTWeightingFunction() {
            //e_info << "Initializing default LKHTWeightingFunction" << endl;
        }
        ~LKHTWeightingFunction() {}
        virtual double EvalFromPoints(LKImagePoint* imagePoint, LKParamPointRT* paramPoint) {
            auto distance = paramPoint -> DistanceToImagePoint(0, imagePoint);
            auto error = imagePoint -> GetError();
            return EvalFromDistance(distance,error,imagePoint->fWeightHT);
        }
        virtual double EvalFromDistance(double distance, double error, double pointWeight) { return 1; }
};
class LKHoughWFConst : public LKHTWeightingFunction {
    public:
        LKHoughWFConst() {
            e_info << "Initializing LKHoughWFConst" << endl;
            e_info << "  - Hough transform weight is always 1" << endl;
        }
        ~LKHoughWFConst() {}
        double EvalFromPoints(LKImagePoint* imagePoint, LKParamPointRT* paramPoint) { return 1; }
        double EvalFromDistance(double distance, double error, double pointWeight) { return 1; }
};
class LKHoughWFLinear : public LKHTWeightingFunction {
    public:
        LKHoughWFLinear() {
            e_info << "Initializing LKHoughWFLinear" << endl;
            e_info << "  - Hough transform weight is 1 - (dist/error/2)" << endl;
        }
        ~LKHoughWFLinear() {}
        double EvalFromDistance(double distance, double error, double pointWeight) {
            double weight = (1 - distance/error/2);
            if (weight<0)
                return 0;
            return weight;
        }
};
class LKHoughWFInverse : public LKHTWeightingFunction {
    public:
        LKHoughWFInverse() {
            e_info << "Initializing LKHoughWFInverse" << endl;
            e_info << "  - Hough transform weight is error/(dist+error)" << endl;
        }
        ~LKHoughWFInverse() {}
        double EvalFromDistance(double distance, double error, double pointWeight) {
            double weight = (error)/(distance+error);
            return weight;
        }
};
class LKHoughWFGivenWeight : public LKHTWeightingFunction {
    public:
        LKHoughWFGivenWeight() {
            e_info << "Initializing LKHoughWFGivenWeight" << endl;
            e_info << "  - Hough transform weight is same as given fit weight" << endl;
            e_info << "  - For LKHit input, weighting is 1./[position-error] (LKHit::WeightPositionError)" << endl;
            e_info << "  - !!! Please check if hit position error is set !!!" << endl;
        }
        ~LKHoughWFGivenWeight() {}
        virtual double EvalFromPoints(LKImagePoint* imagePoint, LKParamPointRT*) {
            return imagePoint->fWeightHT;
        }
};

//#include "LKHTWeightingFunction.h"

/**
    @brief LKHTLineTracker is tool for finding and fitting a straight line track from a provided list of hits.

    ## Abriviations for this document
    @param HT Hough Transform
    @param IMS Image Space
    @param IMP Image Point
    @param IMB Image point Box : box representing the bin-range or error of image point. 
    @param PMS Parameter Space
    @param PMP Parameter Point
    @param PML Parameter point presented as a Line
    @param PMB Parameter point presented as a Band
    @param PML Radial Line comming out from the transform center
    @param TC Transform Center : A origin point of Hough transform
    @param WF Weighting Function

    ## Definitions
    The Hough transform (HT) introduces parameter space (PMS) to find the appropriate parameters for the given model,
    which, in this case, is a straight line. The space describing the real dimensions (x, y) is referred to as image space (IMS),
    while the space describing the parameters of a model, (a straight line for this case,) (radius and theta) is referred to as PMS.
    Each space is defined as classes LKImagePoint and LKParamPointRT.

    We define a line; Parameter presented as a Line (PML), which is a straight line defined by two parameters selected from parameter space.
    The first parameter, "radius," represents the length of the radial line (PML) extending from the transform center.
    At the end of the PML, a PML is drawn perpendicular to it.
    The angle between the x-axis and the PML defines the second parameter, "theta".
    Thus, the angle between the x-axis and the PML is theta + 90 degrees.
    Note that the unit of angles used within this method is degrees.

    @param Transform-Center (TC) The transform center is the position in IMS where the HT will be performed.
           The choice of the transform center will affect the results. It must be set using SetTransformCenter().
    @param Image-space-range The number of bins, minimum and maximum ranges for x and y, must be set. It must be set using SetImageSpaceRange().
    @param Param-space-range The number of bins for radius and theta parameters should be set.
           The range of the radius is automatically calculated using the transform center and IMS range.
           The binning does not need to be finer than 200x200 (in cases the case of TPCs with "average" spatial resolution for example),
           if the HT is employed only for the purpose of collecting hits and not for fitting tracks.
           It must be set using SetParamSpaceBins(). In case user wants to set the full range of radius and theta, it can be set using SetParamSpaceRange().
           For line fitting, the FitTrackWithParamPoint() method can be used.
    @param Correlator Choose from the following options. More descriptions are given below paragraph.
           1. Point-Band correlator : SetCorrelatePointBand()
           2. Box-Line correlator : SetCorrelateBoxLine()
           3. Box-Ribbon correlator : SetCorrelateBoxRibbon()
           4. Box-Band correlator : SetCorrelateBoxBand()

    ## Correlators
    While the PML represents the line defined from the central value of the selected bin in PMS,
    a band; Parameter presented as a Band (PMB), represents the range that includes all parameter values within the selected bin range in PMS.
    It needs to be noted that each image point (IMP) also has its error represented by the bin in IMS.
    This definition will help understanding the correlator description below:

    There are four correlation methods to choose from:
    1. Point-Band correlator : This correlator checks if the PMB include the center point of the image point box (IMB).
                               This method doos not care of the point error and take care of parameter point (PMP) error.
                               The speed of correlator is the fastest out of all.
    2. Box-Line correlator : This correlator checks if the PML passes through the IMB.
                             By selecting this correlator, the PMP will be filled up only when the line precisely intersects the IMB.
                             This method take care of the point error and do not take care of PMP error.
                             This method is slower than Point-Band correlator.
                             It is not recommanded to use this method unless one understands what this actually does.
    3. Box-Band correlator : This correlator verifies whether there is an overlap between the PMB and the IMB.
                             The correlation condition is less strict compared to the Point-Band and Box-Line correlators,
                             but it is more reasonable because both the PMP and the PMP are represented as bin boxes
                             rather than single points.
                             This method provides a general explanation for 
                             and is suitable for detectors with average spatial resolution.
                             However this method is slower than Point-Band and Box-Line correlators.
    4. Box-RBand correlator : This correlator is similar to Box-Band correlator.
                              However, r-band is defined only using the center theta value.
                              This means that r-band do not overlap with the other neighboring parameter bins with same theta range.
                              This correlator is default option.

    ## Weighting function
    Weighting function is a function that gives weight value to fill up the PMS bin by correlating with IMB.
    Defualt weighting function is LKHoughWFInverse. The weight in LKHoughWFInverse is defined by:
    weight = [IMP weight] * [IMP error] / ([distance from IMP to PML] + [IMP error])
    Other weighting functions can be found in LKHTLineTracker.h.
    It is also possible to create an user defined weighting function by inheriting LKHTLineTracker class.

    ## Usage of HT
    The author of this method does not recommend the use of the HT method as a track fitter.
    The HT tool is efficient when it comes to grouping, But its performance as a fitter is limited by various factors.
    Instead, The author recommand to find the group of hits,
    then use FitTrackWithParamPoint() method which employ the least chi-squared method to fit straight line.

    If one intends to employ HT for the fitter, it's needed to use the smallest possible PMS binning to meet the user's desired resolution.
    If the spatial resolution is not impressive, the Box-Band correlator should be used. However, these adjustments will increase computation time.
    Even if good track parameters are identified, there's no assurance that the fit result will be superior to the least chi-squared method.
    In case of situation where only a single track exist in the event,
    small number of bins for PMS can be used along with RetransformFromLastParamPoint() method.
    RetransformFromLastParamPoint() method find the maximum PMS-bin,
    and recalculate HT parameters to range PMS within the selected bin.
    After, the Transform() method will be called which is equalivant to dividing PMS in to n^2 x m^2 bins,
    where nxm is the binning chosen by the user.

    ## Example macro

    @code{.cpp}
    {
         auto tracker = new LKHTLineTracker();
         tracker -> SetTransformCenter(0,0);
         tracker -> SetImageSpaceRange(120, -150, 150, 120, 0, 500);
         tracker -> SetParamSpaceBins(numBinsR, numBinsT);
         for (...) {
             tracker -> AddImagePoint(x, xerror, y, yerror, weight);
         }
         tracker -> SetCorrelateBoxBand();
         tracker -> Transform();
         auto paramPoint = tracker -> FindNextMaximumParamPoint();
         track = tracker -> FitTrackWithParamPoint(paramPoint);
    }
    @endcode

    @todo performance & parameters guide
 */

class LKHTLineTracker : public LKPadInteractive
{
    public:
        LKHTLineTracker();
        virtual ~LKHTLineTracker() { ; }

        bool Init();
        void Print(Option_t *option="") const;

        void Reset(); ///< Reset parameter space range and parameter data
        void Clear(Option_t *option=""); /// Reset parameter space range and parameter data. Clear track-array, hit-array, and point-array.

        void ClearPoints();

        TVector3 GetTransformCenter() const  { return fTransformCenter; }
        int GetNumBinsImageSpace(int ixy) const  { return fNumBinsImageSpace[ixy]; }
        int GetNumBinsParamSpace(int ixy) const  { return fNumBinsParamSpace[ixy]; }
        double GetRangeImageSpace(int ixy, int i) const  { return fRangeImageSpace[ixy][i]; }
        double GetRangeParamSpace(int ixy, int i) const  { return fRangeParamSpace[ixy][i]; }
        double** GetParamData() const  { return fParamData; }
        int GetNumImagePoints() const  { return fNumImagePoints; }
        int GetNumParamPoints() const  { return fNumBinsParamSpace[0]*fNumBinsParamSpace[1]; }
        double GetMaxWeightingDistance(double distance) const { return fMaxWeightingDistance; }
        TObjArray* GetHitArray() { return fHitArray; }
        TObjArray* GetSelectedHitArray() { return fSelectedHitArray; }

        /**
         * Add 2-dimensional histogram to set image space range and add data for each bin.
         * Users do not need to call SetImageSpaceRange when using this method.
         */
        void AddHistogram(TH2* hist);

        void SetTransformCenter(double x, double y) { fTransformCenter = TVector3(x,y,0); }
        void SetImageSpaceRange(int nx, double x1, double x2, int ny, double y1, double y2);
        void SetParamSpaceBins(int nr, int nt);
        void SetParamSpaceRange(int nr, double r2, double r1, int nt, double t1, double t2);
        void AddImagePoint(double x, double xError, double y, double yError, double weightHT=1, double weightFit=-1);
        void AddImagePointBox(double x1, double y1, double x2, double y2, double weightHT=1, double weightFit=-1);
        void SetImageData(double** imageData);
        void SetWeightCutTrackFit(double value) { fWeightCutTrackFit = value; }

        void AddHit(LKHit* hit, LKVector3::Axis a1, LKVector3::Axis a2, double weightHT=1);

        TString GetCorrelatorName() const {
            TString correlatorName;
                 if (fCorrelateType==kCorrelatePointBand) correlatorName = "Point-Band";
            else if (fCorrelateType==kCorrelateBoxLine  ) correlatorName = "Box-Line";
            else if (fCorrelateType==kCorrelateBoxRibbon) correlatorName = "Box-Ribbon";
            else if (fCorrelateType==kCorrelateBoxBand  ) correlatorName = "Box-Band";
            return correlatorName;
        }

        TString GetProcessName() const {
            TString processName;
                 if (fProcess==kNon         ) processName = "Non";
            else if (fProcess==kAddPoints   ) processName = "AddPoints";
            else if (fProcess==kTransform   ) processName = "Transform";
            else if (fProcess==kSelectPoints) processName = "SelectPoints";
            else if (fProcess==kFitTrack    ) processName = "FitTrack";
            else if (fProcess==kClear       ) processName = "Clear";
            else if (fProcess==kClearPoints ) processName = "ClearPoints";
            return processName;
        }

        bool IsCorrelatePointBand()   { if (fCorrelateType==kCorrelatePointBand)   return true; return false; }
        bool IsCorrelateBoxLine()   { if (fCorrelateType==kCorrelateBoxLine)   return true; return false; }
        bool IsCorrelateBoxRibbon()   { if (fCorrelateType==kCorrelateBoxRibbon)   return true; return false; }
        bool IsCorrelateBoxBand()  { if (fCorrelateType==kCorrelateBoxBand)  return true; return false; }

        void SetCorrelatePointBand();
        void SetCorrelateBoxLine();
        void SetCorrelateBoxRibbon();
        void SetCorrelateBoxBand();
        void SetMaxWeightingDistance(double distance) { fMaxWeightingDistance = distance; }
        void SetCutNumTrackHits(double value) { fCutNumTrackHits = value; }

        void SetWFConst()       { fWeightingFunction = new LKHoughWFConst(); }
        void SetWFLinear()      { fWeightingFunction = new LKHoughWFLinear(); }
        void SetWFInverse()     { fWeightingFunction = new LKHoughWFInverse(); }
        void SetWFGivenWeight() { fWeightingFunction = new LKHoughWFGivenWeight(); }

        void SetWeightingFunction(LKHTWeightingFunction* wf) { fWeightingFunction = wf; }

        LKImagePoint* GetImagePoint(int i);
        LKImagePoint* PopImagePoint(int i);
        LKParamPointRT* GetParamPoint(int i);
        LKParamPointRT* GetParamPoint(int ir, int it);

        void Transform();
        void DeTransformSelectedPoints();
        LKParamPointRT* FindNextMaximumParamPoint();
        LKParamPointRT* GetCurrentMaximumParamPoint() { return fParamPoint; }

        void RemoveSelectedPoints();
        void SelectPoints(LKParamPointRT* paramPoint, double weightCut=-1);
        LKLinearTrack* FitTrackWithParamPoint(LKParamPointRT* paramPoint, double weightCut=-1); /// Used hits(points) will be removed from the hit array
        LKLinearTrack* FitTrack3DWithParamPoint(LKParamPointRT* paramPoint, double weightCut=-1); /// Used hits(points) will be removed from the hit array

        LKLinearTrack* FitTrack(LKParamPointRT* paramPoint, double weightCut=-1) { return FitTrackWithParamPoint(paramPoint, weightCut); }
        LKLinearTrack* FitTrack3D(LKParamPointRT* paramPoint, double weightCut=-1) { return FitTrack3DWithParamPoint(paramPoint, weightCut); }

        void CleanLastParamPoint(double rWidth=-1, double tWidth=-1);
        LKParamPointRT* ReinitializeFromLastParamPoint();
        void RetransformFromLastParamPoint();

        TGraphErrors *GetDataGraphImageSpace();
        TGraphErrors *GetSelectedDataGraph(LKParamPointRT* paramPoint);
        TH2D* GetHistImageSpace(TString name="", TString title="");
        TH2D* GetHistParamSpace(TString name="", TString title="");
        void DrawAllParamLines(int i=-1, bool drawRadialLine=true);
        void DrawAllParamBands();

        //void Draw(TVirtualPad* padImage, TVirtualPad* padParam, LKParamPointRT* paramPoint=(LKParamPointRT*) nullptr);
        void Draw(TVirtualPad* padImage, TVirtualPad* padParam, LKParamPointRT* paramPoint, TString option);
        void Draw(TVirtualPad* padImage, TVirtualPad* padParam, LKParamPointRT* paramPoint) { Draw(padImage, padParam, paramPoint, ""); }
        void Draw(TVirtualPad* padImage, TVirtualPad* padParam, TString option) { Draw(padImage, padParam, (LKParamPointRT*)nullptr, option); }
        void Draw(TVirtualPad* padImage, TVirtualPad* padParam) { Draw(padImage, padParam, (LKParamPointRT*)nullptr, ""); }
        void Draw(TString option="");

        //TGraph* GetGraphPathToMaxWeight() { return fGraphPathToMaxWeight; }

    private:
        TVector3     fTransformCenter;
        bool         fInitializedImageData = false;
        bool         fInitializedParamData = false;
        int          fNumBinsImageSpace[2] = {10,10};
        int          fNumBinsParamSpace[2] = {10,10};
        double       fBinSizeImageSpace[2] = {0};
        double       fBinSizeParamSpace[2] = {0};
        double       fBinSizeMaxImageSpace = 0;
        double       fBinSizeMaxParamSpace = 0;
        double       fRangeImageSpace[2][2] = {{0}};
        double       fRangeParamSpace[2][2] = {{0}};
        double       fRangeParamSpaceInit[2][2] = {{0}};
        double**     fParamData;
        int          fIdxSelectedR = -1;
        int          fIdxSelectedT = -1;
        double       fMaxWeightingDistance = 0;

        int             fNumHits = 0;
        TObjArray*      fHitArray = nullptr;
        LKVector3::Axis fA1 = LKVector3::kX;
        LKVector3::Axis fA2 = LKVector3::kY;

        TObjArray*      fSelectedHitArray = nullptr;
        vector<int>     fSelectedImagePointIdxs;

        int             fNumImagePoints = 0;
        TClonesArray*   fImagePointArray = nullptr;
        LKImagePoint*   fImagePoint = nullptr;
        LKParamPointRT* fParamPoint = nullptr;

        bool fClickedPadImage = true;
        LKParamPointRT* fClickSelectedParamPoint = nullptr;
        TGraph*         fGraphClickFitted = nullptr;

        int           fNumLinearTracks = 0;
        TClonesArray* fTrackArray = nullptr;

        int          fCutNumTrackHits = 3;
        LKODRFitter* fLineFitter = nullptr;

        const int    kCorrelatePointBand = 0;
        const int    kCorrelateBoxLine = 1;
        const int    kCorrelateBoxRibbon = 2;
        const int    kCorrelateBoxBand = 3;
        int          fCorrelateType = kCorrelateBoxBand;

        LKHTWeightingFunction* fWeightingFunction = nullptr;
        double       fWeightCutTrackFit = 0.2;

        TGraphErrors* fGraphImageData = nullptr;
        TGraphErrors* fGraphSelectedImageData = nullptr;
        //TGraph* fGraphPathToMaxWeight = nullptr;

        TPad *fPadImage = nullptr;
        TPad *fPadParam = nullptr;
        TH2D *fHistImage = nullptr;
        TH2D *fHistParam = nullptr;

        const int kNon = 0;
        const int kAddPoints = 1;
        const int kTransform = 2;
        const int kSelectPoints = 3;
        const int kFitTrack = 4;
        const int kClear = 5;
        const int kClearPoints = 6;
        const int kRemovePoints = 7;
        const int kDeTransformPoints = 8;
        int fProcess = kNon;

        TGraph* fGraphBand = nullptr;

    public:
        virtual void ExecMouseClickEventOnPad(TVirtualPad *pad, double xOnClick, double yOnClick);

    private:
        TGraph* fGraphClicked = nullptr;

    ClassDef(LKHTLineTracker,1);
};

#endif
