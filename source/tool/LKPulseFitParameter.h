#ifndef LKPULSEFITPARAMETER_HH
#define LKPULSEFITPARAMETER_HH

#include "TObject.h"

class LKPulseFitParameter : public TObject
{
    public:
        LKPulseFitParameter() { Clear(); }
        LKPulseFitParameter(double tbHit, double amplitude, double chi2NDF, int ndf)
            : fTbHit(tbHit), fAmplitude(amplitude), fChi2NDF(chi2NDF), fNDF(ndf) {}

        virtual ~LKPulseFitParameter() { ; }

        void Clear(Option_t *option="") {
            fTbHit = -1;
            fAmplitude = -1;
            fChi2NDF = -1;
            fNDF = -1;
        }

        double fTbHit = -1;
        double fAmplitude = 0;
        double fChi2NDF = 0;
        int    fNDF = 0;

    ClassDef(LKPulseFitParameter,1);
};

#endif
