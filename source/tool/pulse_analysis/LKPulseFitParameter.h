#ifndef LKPULSEFITPARAMETER_HH
#define LKPULSEFITPARAMETER_HH

#include "TObject.h"

class LKPulseFitParameter : public TObject
{
    public:
        LKPulseFitParameter() { Clear(); }
        LKPulseFitParameter(double tbHit, double width, double integral, double amplitude, int ndf, double chi2NDF)
            : fTbHit(tbHit), fWidth(width), fIntegral(integral), fAmplitude(amplitude), fNDF(ndf), fChi2NDF(chi2NDF) {}

        virtual ~LKPulseFitParameter() { ; }

        void Clear(Option_t *option="") {
            fTbHit = -1;
            fWidth = -1;
            fIntegral = -1;
            fAmplitude = -1;
            fNDF = -1;
            fChi2NDF = -1;
        }

        double fTbHit = -1;
        double fWidth = -1;
        double fIntegral = -1;
        double fAmplitude = -1;
        int    fNDF = -1;
        double fChi2NDF = -1;

    ClassDef(LKPulseFitParameter,1);
};

#endif
