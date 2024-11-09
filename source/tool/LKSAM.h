#ifndef LKSIMPLEANALYSISMETHODS_HH
#define LKSIMPLEANALYSISMETHODS_HH

#include "TObject.h"
#include "TH1D.h"
#include "LKDrawing.h"

/// LILAK Simple Analysis Methods
class LKSAM : public TObject
{
    public:
        static LKSAM* GetSAM(); ///< Get LKSAM static pointer.

        LKSAM();
        virtual ~LKSAM() {};

        double FWHM(double *buffer, int length);
        double FWHM(double *buffer, int length, int    iPeak, double amplitude, double pedestal, double &xMin, double &xMax, double &half);
        double FWHM(TH1D* hist,                 double xPeak, double amplitude, double pedestal, double &iMin, double &iMax, double &half);

        double EvalPedestal(double *buffer, int length, int method=0);
        /// cvCut: stdDev/mean cut for collecting samples used for calculating pedestal.
        double EvalPedestalSamplingMethod(double *buffer, int length,  int sampleLength=50, double cvCut=0.2, bool subtractPedestal=false);

        double GetSmoothLevel(TH1* hist, double thresholdRatio=0.05);

        /// Make polar histogram
        /// @param hist : Input histogram running from 0 to 360
        /// @param rMin : Radius of min-axis that represent 0 value of histogram
        /// @param rMax : Radius of max-axis that represent maximum value of histogram
        /// @param numGrids : Number of grids between inner and outer axis
        /// @param numAxisDiv : Number of axis division creating n-polygon. if value is >60, it will use circle to draw the axis
        /// @param drawInfo : Draw histogram stats-like information on the i-th corner with the given value i(=0,1,2,3). Use -1 to ignore
        /// @param usePolygonAxis : False to use circle axis
        /// @param binContentOffRatio : Draw content of each bin in between outer and inner axis using this ratio. Use 0 to ignore
        /// @param labelOffRatio : Draw label of polar angle (if exist) in between inner and outer axis using this ratio. Use 0 to ignore
        /// @param binLabelOffRatio : Draw angle-label of numAxisDiv point with offset equal to dr * binLabelOffRatio. Use 0 to ignore
        /// @param TGraph* graphAtt : Attribute reference graph for drawing histogram bar. Use nullptr to use default (gray-color) attribute.
        LKDrawing* MakeTH1Polar(
                TH1D* hist,
                double rMin=5,
                double rMax=10,
                int numGrids=0,
                int numAxisDiv=12,
                int drawInfo=1,
                bool usePolygonAxis=true,
                double binContentOffRatio=0,
                double labelOffRatio=0.1,
                double binLabelOffRatio=0.1,
                TGraph* graphAtt=nullptr
                );

    private:
        void InitBufferDouble(int length);

    private:
        double *fBufferDouble = nullptr;
        int fBufferDoubleSize = 0;      // Variable to keep track of the current buffer size


    private:
        static LKSAM *fInstance;

    ClassDef(LKSAM, 1)
};

#endif
