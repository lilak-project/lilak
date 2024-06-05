#ifndef MISCELANEOUS_HH
#define MISCELANEOUS_HH

#include "TObject.h"
#include "LKLogger.h"
#include <iostream>
using namespace std;

class LKMisc : public TObject
{
    public:
        static double FWHM(double *buffer, int length);
        static double FWHM(double *buffer, int length, int xPeak, double amplitude, double pedestal, double &xMin, double &xMax);

        static double EvalPedestal(double *buffer, int length, int method=0);
        /// cvCut: stdDev/mean cut for collecting samples used for calculating pedestal.
        static double EvalPedestalSamplingMethod(double *buffer, int length,  int sampleLength=50, double cvCut=0.2, bool subtractPedestal=false);
    

    public:
        LKMisc();
        virtual ~LKMisc() {}
        static LKMisc* GetMisc();

    private:
        static LKMisc* fInstance;

    ClassDef(LKMisc,1);
};

#endif
