#ifndef LKSETSICHANNELTASK_HH
#define LKSETSICHANNELTASK_HH

#include "TH1.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TVirtualPad.h"
#include "TClonesArray.h"

#include "LKTask.h"
#include "LKSiliconArray.h"

class TFormula;

class LKSetSiChannelTask : public LKTask
{
    public:
        /// Saturated-hit marker, not an estimate of the physical energy.
        static constexpr double kSaturatedEnergy = 10000.;
        LKSetSiChannelTask();
        virtual ~LKSetSiChannelTask();

        bool Init();
        void Exec(Option_t*);

    private:
        int fPedestalTbFirst = 0;
        int fPedestalTbLast = 250;
        double fBipolarMinPercent = 50;
        double fBipolarMaxPercent = 150;
        int fBipolarWindow = 30;
        bool fPulserAnalysis = false;
        TFormula *fSaturationEnergyFormula = nullptr; //! Compiled once at Init
        bool fSaturationNeedsSlope = false;
        int fSaturationSlopeWindow = 6; ///< TB samples immediately before the first saturated TB.
        int fHitTbStart = 0;
        int fHitTbEnd = 512;
        LKSiliconArray* fSiliconArray = nullptr;
        LKChannelAnalyzer* fChannelAnalyzer = nullptr;
        LKChannelAnalyzer* fChannelAnalyzer2 = nullptr;

    private:
        TClonesArray *fRawDataArray = nullptr;
        TClonesArray *fSiChannelArray = nullptr;
        TClonesArray *fFitDataArray = nullptr;

    ClassDef(LKSetSiChannelTask, 6)
};

#endif
