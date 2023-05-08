#ifndef LKSAMPLEPOINT
#define LKSAMPLEPOINT

#include "Rtypes.h"
#include "TString.h"
#include <string>
#include <sstream>
using namespace std;

class LKSamplePoint
{
    public:
        LKSamplePoint();
        LKSamplePoint(LKSamplePoint &sample);
        LKSamplePoint(Double_t v, Double_t rms = 0, Double_t w = 1);
        virtual ~LKSamplePoint();

        void Print();
        void Init(Double_t v, Double_t rms = 0, Double_t w = 1);
        void Init(string line);
        void Update(Double_t v, Double_t w = 1);

        TString GetSummary();

        Double_t fValue;      ///< Value of the sample point
        Double_t fRMS;        ///< RMS of the value
        Double_t fWeightSum;  ///< Sum of weights from the updates

        ClassDef(LKSamplePoint, 1);
};

#endif
