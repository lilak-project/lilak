#ifndef LKSIMPLEANALYSISMETHODS_HH
#define LKSIMPLEANALYSISMETHODS_HH

#include "TObject.h"
#include "TH1D.h"

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
