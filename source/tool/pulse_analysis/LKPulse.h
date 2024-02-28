#ifndef LKPULSE_HH
#define LKPULSE_HH

#include "TObject.h"
#include "LKLogger.h"
#include "TGraphErrors.h"
#include "TGraph.h"

/**
 * @brief LKPulse is pulse data class that draw waveform in channel buffer.
 * The class should be initialized with pulse data file created from LKPulseAnalyzer.
 */
class LKPulse : public TObject
{
    public:
        LKPulse();
        LKPulse(const char *fileName);
        virtual ~LKPulse() { ; }

        bool Init();
        void Clear(Option_t *option="");
        void Print(Option_t *option="") const;

        double EvalTb(double tb, double tb0=0, double amplitude=1);
        double ErrorTb(double tb, double tb0=0, double amplitude=1);
        double Error0Tb(double tb, double tb0=0, double amplitude=1);

        double Eval(double tb, double tb0=0, double amplitude=1);
        double Error(double tb, double tb0=0, double amplitude=1);
        double Error0(double tb, double tb0=0, double amplitude=1);

        TGraphErrors *GetPulseGraph(double tb0, double amplitude, double pedestal=0);
        TGraphErrors* FillPulseGraph(TGraphErrors* graphPulse, double tb0, double amplitude, double pedestal=0);

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

    private:
        int fNumPoints = 0;
        TGraphErrors* fGraphPulse = nullptr;
        TGraph*       fGraphError = nullptr;
        TGraph*       fGraphError0 = nullptr;

        int          fNumAnalyzedChannels;
        int          fInversion = 1;
        int          fThreshold;
        int          fHeightMin;
        int          fHeightMax;
        int          fTbMin;
        int          fTbMax;
        double       fFWHM;
        double       fFloorRatio;
        double       fRefWidth;
        double       fWidthLeading;
        double       fWidthTrailing;
        int          fPulseRefTbMin;
        int          fPulseRefTbMax;
        double       fBackGroundLevel;
        double       fBackGroundError;
        double       fFluctuationLevel;



    ClassDef(LKPulse,1);
};

#endif
