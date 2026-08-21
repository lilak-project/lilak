#ifndef LKPULSE_HH
#define LKPULSE_HH

#include "TObject.h"
#include "LKLogger.h"
#include "TGraphErrors.h"
#include "TGraph.h"
#include "TF1.h"

class LKParameterContainer;

/**
 * @brief LKPulse is pulse data class that draw waveform in channel buffer.
 * The class should be initialized with pulse data file created from LKPulseExtractor.
 */
class LKPulse : public TObject
{
    public:
        LKPulse();
        LKPulse(const char *fileName);
        LKPulse(LKParameterContainer *par, TString groupName="LKPulse");
        virtual ~LKPulse() { ; }

        bool Init();
        void Clear(Option_t *option="");
        void Print(Option_t *option="") const;
        void Draw(Option_t *option="");

        double EvalTb(double tb, double tb0=0, double amplitude=1);
        double ErrorTb(double tb, double tb0=0, double amplitude=1);
        double Error0Tb(double tb, double tb0=0, double amplitude=1);

        double Eval(double tb, double tb0=0, double amplitude=1);
        double Error(double tb, double tb0=0, double amplitude=1);
        double Error0(double tb, double tb0=0, double amplitude=1);

        TGraphErrors *GetPulseGraph(double tb0, double amplitude, double pedestal=0);
        TGraphErrors* FillPulseGraph(TGraphErrors* graphPulse, double tb0, double amplitude, double pedestal=0);

        void SetUsePulseFunction(bool value=true);
        void SetPulseFunctionParameters(double baseline, double peak, double t0, double alpha, double tau);
        void SetPulseFunctionRange(double tbMin, double tbMax);
        bool LoadParameterFile(const char *fileName, TString groupName="LKPulse");
        bool UpdatePulseFunctionParameters(LKParameterContainer *par, TString groupName="LKPulse");
        void FixPulseFunctionAlpha(bool value=true) { fFixPulseFunctionAlpha = value; }
        void FixPulseFunctionTau(bool value=true) { fFixPulseFunctionTau = value; }
        TF1* GetPulseFunction(TString name="pulse_function", bool subtractBaseline=true);
        bool FitPulseFunction(TGraphErrors *graph, Option_t *option="RQ0");
        bool UsesPulseFunction() const { return fUsePulseFunction; }
        double GetPulseFunctionBaseline() const { return fPulseFunctionBaseline; }
        double GetPulseFunctionPeak() const { return fPulseFunctionPeak; }
        double GetPulseFunctionT0() const { return fPulseFunctionT0; }
        double GetPulseFunctionAlpha() const { return fPulseFunctionAlpha; }
        double GetPulseFunctionTau() const { return fPulseFunctionTau; }
        double GetPulseFunctionTbMin() const { return fPulseFunctionTbMin; }
        double GetPulseFunctionTbMax() const { return fPulseFunctionTbMax; }

        int GetNDF() const { return fNumPoints; }
        int GetNumAnalyzedChannels() const  { return fNumAnalyzedChannels; }
        int GetThresholdC() const  { return fThreshold; }
        int GetHeightMin() const  { return fHeightMin; }
        int GetHeightMax() const  { return fHeightMax; }
        int GetTbMin() const  { return fTbMin; }
        int GetTbMax() const  { return fTbMax; }
        bool GetInverted() const  { return (fInversion==-1); }
        double GetFWHM() const  { return fFWHM; }
        double GetFloorRatio() const  { return fFloorRatio; }
        double GetWidth() const  { return fRefWidth; }
        double GetLeadingWidth() const  { return fWidthLeading; }
        double GetTrailingWidth() const  { return fWidthTrailing; }
        double GetPulseRefTbMin() const { return fPulseRefTbMin; }
        double GetPulseRefTbMax() const { return fPulseRefTbMax; }
        double GetBackGroundLevel() const  { return fBackGroundLevel; }
        double GetBackGroundError() const  { return fBackGroundError; }
        double GetFluctuationLevel() const  { return fFluctuationLevel; }

        void SetInverted() { fInversion = -1; }

        bool IsGood() const { return fPulseIsGood; }

        static double PulseFunction(double *x, double *p);
        static double PulseFunctionBaselineSubtracted(double *x, double *p);

    private:
        void UpdatePulseFunctionProperties();
        double EvalPulseFunction(double x, double amplitude=1) const;
        double ErrorPulseFunction(double x, double amplitude=1) const;

        bool fPulseIsGood = false;
        bool fUsePulseFunction = false; ///< If true, evaluate the analytic gamma-like pulse instead of the reference graph.

        int fNumPoints = 0;
        TGraphErrors* fGraphPulse = nullptr;
        TGraph*       fGraphError = nullptr;
        TGraph*       fGraphError0 = nullptr;

        int          fNumAnalyzedChannels = 0;
        int          fInversion = 1;
        int          fThreshold = 0;
        int          fHeightMin = 0;
        int          fHeightMax = 0;
        int          fTbMin = 0;
        int          fTbMax = 0;
        double       fFWHM = 0;
        double       fFloorRatio = 0.05;
        double       fRefWidth = 0;
        double       fWidthLeading = 0;
        double       fWidthTrailing = 0;
        int          fPulseRefTbMin = 0;
        int          fPulseRefTbMax = 0;
        double       fBackGroundLevel = 0;
        double       fBackGroundError = 0;
        double       fFluctuationLevel = 0;

        bool         fFixPulseFunctionAlpha = false; ///< Fix alpha in FitPulseFunction().
        bool         fFixPulseFunctionTau = false; ///< Fix tau in FitPulseFunction().
        double       fPulseFunctionBaseline = 0.000247812; ///< Normalized baseline offset from pulse-reference fit.
        double       fPulseFunctionPeak = 1.00117; ///< Normalized pulse height of the fitted function.
        double       fPulseFunctionT0 = -3.35106; ///< (tb) Function start time relative to hit time.
        double       fPulseFunctionAlpha = 3.4317; ///< Shape exponent of the gamma-like fitted pulse.
        double       fPulseFunctionTau = 4.54546; ///< (tb) Decay time scale of the gamma-like fitted pulse.
        double       fPulseFunctionTbMin = -20.; ///< (tb) Lower valid range relative to hit time.
        double       fPulseFunctionTbMax = 81.; ///< (tb) Upper valid range relative to hit time.
        double       fPulseFunctionErrorScale = 0.05; ///< Relative error used by analytic pulse fitting.

    ClassDef(LKPulse,2);
};

#endif
