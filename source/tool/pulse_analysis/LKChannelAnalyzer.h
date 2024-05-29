#ifndef LKCHANNELANALYZER_HH
#define LKCHANNELANALYZER_HH

//#define DEBUG_CHANA_FINDPED
//#define DEBUG_CHANA_FINDPEAK 500
//#define DEBUG_CHANA_ANALYZE
//#define DEBUG_CHANA_FITPULSE
//#define DEBUG_CHANA_FITAMPLITUDE
//#define DEBUG_CHANA_ANALYZE_NHIT 1

#include <vector>
#include <cfloat>
using namespace std;

#include "TGraph.h"
#include "TGraphErrors.h"
#include "TParameter.h"
#include "TH1D.h"
#include "TClonesArray.h"
#include "TMultiGraph.h"
#include "TH2D.h"

#include "LKLogger.h"
#include "LKPulse.h"
#include "LKPulseFitParameter.h"
#include "LKPadInteractive.h"
#include "LKPadInteractiveManager.h"

//#define NUMBER_OF_PEDESTAL_TEST_REGIONS 6
//#define NUMBER_OF_PEDESTAL_TEST_REGIONS 7
//#define NUMBER_OF_PEDESTAL_TEST_REGIONS_M1 NUMBER_OF_PEDESTAL_TEST_REGIONS-1

class LKTbIterationParameters
{
    public:
        LKTbIterationParameters() {}

        double fC1 = DBL_MAX;
        double fC2 = DBL_MAX;
        double fT1 = 0;
        double fT2 = 0;
        double fScaleTbStep0 = 1;
        double fScaleTbStep = 1;

        void SetScaleTbStep(double value) {
            fScaleTbStep0 = value;
            fScaleTbStep = value;
        }

        double Slope() const {
            double slope = - (fC1 - fC2) / (fT1 - fT2);
            return slope;
        }

        double TbStep() const {
            double tbStep = fScaleTbStep * Slope();
            return tbStep;
        }

        double NextTb(double tb) const {
            double step = TbStep();
                 if (step> 1) step =  1;
            else if (step<-1) step = -1;
            double tbNext = tb + step;
            return tbNext;
        }

        bool Add(double c0, double t0) {
            if (c0<fC1) {
                fC2 = fC1;
                fT2 = fT1;
                fC1 = c0;
                fT1 = t0;
                fScaleTbStep = fScaleTbStep0;
                return true;
            }
            else if (c0<fC2) {
                fC2 = c0;
                fT2 = t0;
                fScaleTbStep = fScaleTbStep0;
                return true;
            }
            else {
                fScaleTbStep = 0.5 * fScaleTbStep;
                return false;
            }
        }
};

/**
 * @brief LKChannelAnalyzer find and fit pulse signal from the given channel buffer using input pulse data.
 * The main usage of LKChannelAnalyzer is to run Analyze() method.
 * Analyze() method is combination of FindPeak(), FitPulse(), TestPulse(), FitAmplitude()
 * which are, in principle, can be used independently as well. Read method descriptions for detail.
 *
 * LKChannelAnalyzer should be initialized with pulse data file created from LKPulseAnalyzer.
 * See LKPulseAnalyzer for pulse data file.
 * Some parameters are set from pulse data file. Look for keyword "Automatically set".
 * Some parameters must be set by user. Look for keyword "Must be set".
 *
 * Critical parameters affecting the fitting speed and fit resolution are
 *
 * @param fNDFFit : Number of points to use. Related to fitting speed. Automatically set from pulse: fNDFFit = fWidthLeading + fFWHM/4. Can be set with Set SetNDFPulse()
 * @param fIterMax : Maximum iteration cut. Must be set with SetIterMax()
 * @param fTbStepCut : Break from loop if fit-TB-step < fTbStepCut. Related to fitting speed and resolution. Can be set with SetTbStepCut().
 * @param fScaleTbStep : (~0.2) Scale factor for choosing fit-TB-step for next TB candidate. Related to fitting speed and iteration #. Must be set with SetScaleTbStep()
 *
 * The value of fit-TB-step is chosen by fScaleTbStep times inner parameter "slope".
 *
 * @param slope inner parameter defined as @f$(\chi^2_{i-1}/NDF_{i-1}-\chi^2_{i}/NDF_{i}) / (t_i-t_{i-1})@f$, where 'i' corresponds to the iteration count and t is the fitting TB-position. This parameter indicates how rapidly the chi-square value changes per unit TB. The program is designed to take larger TB-steps when the chi-square difference is big and smaller TB-steps when the chi-square difference is small. One can adjust the rate of change by modifying the variable fScaleTbStep. The value of slope can be access by using DEBUG_CHANA_FITPULSE macro in LKChannelanalyzer.h
 *
 * These parameters depend on the experimental settings. Users should test the parameter dependence on the performance of LKChannelAnalyzer.
 * For debugging the fitting process, you can utilize the following C++ macros.
 * To activate them, macros (in LKChannelAnalyzer.h) should be comment-in, and lilak should be re-compiled.
 *
 * @param DEBUG_CHANA_FINDPEAK
 * @param DEBUG_CHANA_ANALYZE
 * @param DEBUG_CHANA_FITPULSE From this macro, users have access to following graphs, to check how inner parameters change : dGraph_it_tb, dGraph_it_tbStep, dGraph_it_chi2, dGraph_it_slope, dGraph_tb_chi2, dGraph_tb_slope, dGraph_it_slopeInv, dGraph_tb_slopeInv.
 *
 *
 * Example of using LKChannelAnalyzer:
 *
 * @code{.cpp}
 *  {
 *      auto ana = LKChannelAnalyzer()
 *      ana -> SetPulse("pulse_data_created_from_LKPulseAnalyzer.root");
 *      ana -> SetTbMax(512);
 *      ana -> SetTbStart(0);
 *      ana -> SetDynamicRange(4096);
 *      ana -> SetThresholdOneStep(2);
 *      ana -> SetThreshold(100);
 *      ana -> SetIterMax(15);
 *      ana -> SetTbStepCut(0.01);
 *      ana -> SetScaleTbStep(0.2);
 *
 *      for (...)
 *      {
 *          double* buffer = get_data_somehow;
 *          ana -> Analyze(buffer);
 *
 *          auto numHits = ana -> GetNumHits();
 *          for (auto iHit=0; iHit<numHits; ++iHit) {
 *              auto tbHit = ana -> GetTbHit(iHit);
 *              auto amplitude = ana -> GetAmplitude(iHit);
 *              MyAnalysis();
 *          }
 *      }
 *  }
 * @endcode
 */
class LKChannelAnalyzer : public LKPadInteractive
{
    public:
        LKChannelAnalyzer();
        virtual ~LKChannelAnalyzer() { ; }

        bool Init();
        void Clear(Option_t *option="");
        void Print(Option_t *option="") const;
        void Draw(Option_t *option="");

        /// Set pulse function and related parameter from pulse data file created from LKPulseAnalyzer
        void SetSigAtMaximum() { fAnalyzerMode = kSigAtMaximumMode; }
        void SetSigAtThreshold() { fAnalyzerMode = kSigAtThresholdMode; }
        void SetPulse(const char* fileName);
        LKPulse* GetPulse() const  { return fPulse; }
        TString GetPulseFileName() const { return fPulseFileName; }

        double Eval  (double tb, double tb0=0, double amplitude=1) { return fPulse -> Eval  (tb, tb0, amplitude); }
        double Error (double tb, double tb0=0, double amplitude=1) { return fPulse -> Error (tb, tb0, amplitude); }
        double Error0(double tb, double tb0=0, double amplitude=1) { return fPulse -> Error0(tb, tb0, amplitude); }

        int GetNumHits() const  { return fNumHits; }
        LKPulseFitParameter GetFitParameter(int i) const  { return fFitParameterArray[i]; }
        double GetTbHit(int i) const      { return fFitParameterArray[i].fTbHit; }
        double GetAmplitude(int i) const  { return fFitParameterArray[i].fAmplitude; }
        double GetChi2NDF(int i) const    { return fFitParameterArray[i].fChi2NDF; }
        double GetNDF(int i) const        { return fFitParameterArray[i].fNDF; }
        double GetWidth(int i) const      { return fFitParameterArray[i].fWidth; }

        void Analyze(TH1D* hist);
        void Analyze(int* data);
        void Analyze(double* data);
        void AnalyzePulseFitting();
        void AnalyzeSigAtMaximum();
        void AnalyzeSigAtThreshold();

        /**
         * Find pedestal and subtract it from the buffer.
         * Pedestal is estimated through following process
         * 1. Divide tb-region by NUMBER_OF_PEDESTAL_TEST_REGIONS(6)
         * 2. Calculate average-charge for each region.
         * 3. Find two regions which has least value of average-charge difference.
         * 4. Make charge-average-cut which is ~20% of charge-average of selected two regions.
         * 5. For all regions, select which has average-charge that come in the average-charge-cut.
         * 6. Pedestal is average-charge of select regions.
         */
        double FindAndSubtractPedestal(double *buffer);
        /**
         * Find first peak from adc time-bucket starting from input tbCurrent
         * tbCurrent and tbStart becomes time-bucket of the peak and starting point
         */
        bool FindPeak(double *buffer, int &tbPointer, int &tbStartOfPulse);
        /**
         * Perform least square fitting with the the pulse around tbStart ~ tbPeak.
         * This process is iteration based process using method LSFitPuse()
         */
        bool FitPulse(double *buffer, int tbStartOfPulse, int tbPeak, double &tbHit, double &amplitude, double &squareSum, int &ndf, bool &isSaturated);
        /**
         * Compare pulse with the previous pulse
         * If the pulse is not distinguishable from the previous pulse, return false.
         * If the pulse is distinguishable, subtract pulse distribution from adc, and return true
         */
        bool TestPulse(double *buffer, double tbHitPre, double amplitudePre, double tbHit, double amplitude);
        /**
         * Perform least square fitting with the fixed parameter tbStart and ndf.
         * Amplitude is calculated from weighted least chi-square fitting
         * with waveform f(x) and weight w(x) = 1/(stddev(x)^2).
         * Amplitude = (Sum_i y_i * w(x_i) * f(x_i)) / (Sum_i w(x_i) f^2(x_i)
         */
        void FitAmplitude(double *buffer, double tbStartOfPulse, int &ndf, double &amplitude, double &chi2NDF);

        int GetTbMax() const  { return fTbMax; }
        int GetTbStart() const  { return fTbStart; }
        int GetTbStartCut() const  { return fTbStartCut; }
        int GetThreshold() const  { return fThreshold; }
        int GetThresholdOneTbStep() const  { return fThresholdOneStep; }
        int GetNumTbAcendingCut() const  { return fNumTbAcendingCut; }
        int GetPedestal() const  { return fPedestal; }
        int GetDynamicRange() const  { return fDynamicRange; }
        int GetNDF() const  { return fNDFFit; }
        int GetIterMax() const  { return fIterMax; }
        double GetScaleTbStep() const { return fScaleTbStep; }
        double GetTbStepCut() const { return fTbStepCut; }

        void SetTbRange(int tbStart, int tbMax) { fTbStart = tbStart; fTbMax = tbMax; }
        void SetTbMax(int tbMax) { fTbMax = tbMax; }
        void SetTbStart(int tbStart) { fTbStart = tbStart; }
        void SetTbStartCut(int tbStartCut) { fTbStartCut = tbStartCut; }
        void SetThreshold(int threshold) { fThreshold = threshold; }
        void SetThresholdOneStep(int oneStepThreshold) { fThresholdOneStep = oneStepThreshold; }
        void SetNumTbAcendingCut(int numAcendingCut) { fNumTbAcendingCut = numAcendingCut; }
        void SetDynamicRange(int dynamicRange) { fDynamicRange = dynamicRange; }
        void SetNDFPulse(int nDFPulse) { fNDFFit = nDFPulse; }
        void SetIterMax(int iterMax) { fIterMax = iterMax; }
        void SetScaleTbStep(double scaleTbStep) { fScaleTbStep = scaleTbStep; }
        void SetTbStepCut(double value) { fTbStepCut = value; }
        void SetDataIsInverted(bool value=true) { fDataIsInverted = value; }

        double* GetBuffer() { return fBuffer; }

        TGraph*       FillPedestalGraph    (TGraph*       graph, double tb1=0, double tb2=512);
        TGraphErrors* FillPulseGraph       (TGraphErrors* graph, double tb0, double amplitude, double pedestal=0);
        TGraphErrors* FillGraphPulseFitting(TGraphErrors* graph, double tb0, double amplitude, double pedestal=0);
        TGraphErrors* FillGraphSigAtMaximum(TGraphErrors* graph, double tb0, double amplitude, double pedestal=0);
        TGraphErrors* FillGraphSigAtThreshold(TGraphErrors* graph, double tb0, double amplitude, double pedestal=0);

        TGraph*       GetPedestalGraph    (double tb1=0, double tb2=512);
        TGraphErrors* GetPulseGraph       (double tb0, double amplitude, double pedestal=0);
        TGraphErrors* GetGraphPulseFitting(double tb0, double amplitude, double pedestal=0);
        TGraphErrors* GetGraphSigAtMaximum(double tb0, double amplitude, double pedestal=0);
        TGraphErrors* GetGraphSigAtThreshold(double tb0, double amplitude, double pedestal=0);

        void ClearGraphArray() { if (fGraphArray!=nullptr) fGraphArray -> Clear("C"); fNumGraphs = 0; }

    public:
        virtual void ExecMouseClickEventOnPad(TVirtualPad *pad, double xOnClick, double yOnClick);
        void SetPad(TVirtualPad *pad, TH2D* hist=nullptr);

    public:
        TVirtualPad* fPadFitPanel = nullptr;
        TH2D* fHistFitPanel = nullptr;

    private:
        const int    kPulseFittingMode = 1;
        const int    kSigAtMaximumMode = 2;
        const int    kSigAtThresholdMode = 3;
        int          fAnalyzerMode = kSigAtMaximumMode;

        LKPulse*     fPulse = nullptr; ///< Pulse pointer
        double       fBufferOrigin[512]; ///< Copied buffer from data
        double       fBuffer[512]; ///< Copied buffer from data
        int          fNumHits = 0; ///< Number of hits found after Analyze()
        vector<LKPulseFitParameter> fFitParameterArray; ///< Vector holding fit TB
        bool         fDataIsInverted = false;

        // pedestal
        int          fNumTbSample = 50; ///< Least number of time-bins in pedestal sample regions
        int          fNumPedestalSamples = 0;
        int          fNumPedestalSamplesM1 = 0;
        int          fNumTbSampleLast = 0;
        bool         fUsedSample[20];
        double       fPedestalSample[20] = {0.};
        double       fStddevSample[20] = {0.};
        double       fPedestal = 0; ///< pedestal level of current channel
        double       fPedestalErrorRefSampleCut = 10;
        const double fCVCut = 0.2; ///< stdDev/mean cut for collecting samples used for calculating pedestal.

        // tb
        int          fTbMax = 512; ///< Maximum TB in buffer. Must be set with SetTbMax()
        int          fTbStart = 1; ///< Starting TB-position for analysis. Must be set with SetTbStart()
        int          fTbStartCut = -1; ///< Pulse TB-position cannot be larger than fTbStartCut. Automatically set from pulse: fTbStartCut = fTbMax - fNDFFit. Can be set with SetTbStartCut()
        int          fNumTbAcendingCut = 5; ///< Peak will be recognize if number-of-TBs-acending >= fNumTbAcendingCut. Automatically set from pulse: fNumTbAcendingCut = int(fWidthLeading*2/3).Can be set with SetNumTbAcendingCut()

        // y
        int          fDynamicRangeOriginal = 4096; ///< Dynamic range of buffer. Used for recognizing saturation. Must be set with SetDynamicRange()
        int          fDynamicRange = 4096; ///< Dynamic range subtracted by pedestal of buffer.
        int          fThreshold = 50; ///< Threshold for recognizing pulse peak. Must be set with SetThreshold()
        int          fThresholdOneStep = 2; ///< Threshold for counting number-of-TBs-acending (y-current > y-previous). Must be set with SetThresholdOneStep()

        // peak finding
        int          fTbStepIfFoundHit = 10; ///< [Peak finding] TB-step after hit was found. Automatically set from pulse: fTbStepIfFoundHit = fNDFFit.
        int          fTbStepIfSaturated = 15; ///< [Peak finding] TB-step after saturation was found. Automatically set from pulse: fTbStepIfSaturated = int(fWidth*1.2);
        int          fTbSeparationWidth = 10; ///< [Peak finding] Estimation of # of TB that can separate two different pulse. Automatically set from pulse: fTbSeparationWidth = fNDFFit.
        //int          fNumTbsCorrection = 50; ///< [Peak finding] Number of TBs to subtract from the found pulse. Automatically set from pulse: fNumTbsCorrection = int(numPulsePoints);

        // fitting
        int          fNDFFit = 0; ///< [Pulse-fitting] Number of points to use. Related to fitting speed. Automatically set from pulse: fNDFFit = fWidthLeading + fFWHM/4. Can be set with Set SetNDFPulse()
        int          fIterMax = 15; ///< [Pulse-fitting] Maximum iteration cut. Must be set with SetIterMax()
        double       fTbStepCut = 0.01; ///< [Pulse-fitting] Break from loop if fit-TB-step < fTbStepCut. Related to fitting speed and resolution. Can be set with SetTbStepCut().
        double       fScaleTbStep = 0.5; ///< [Pulse-fitting] (~0.2) Scale factor for choosing fit-TB-step for next TB candidate. Related to fitting speed and iteration #. Must be set with SetScaleTbStep()

        double       fFWHM; ///< [Pulse] Full Width Half Maximum of reference pulse. Automatically set from pulse.
        double       fFloorRatio; ///< [Pulse] Ratio regard to the full pulse height where pulse width was measured. Automatically set from pulse.
        double       fWidth; ///< [Pulse] Width of the pulse. Automatically set from pulse.
        double       fWidthLeading; ///< [Pulse] Width measured from leading edge to TB-at-peak. Automatically set from pulse.
        double       fWidthTrailing; ///< [Pulse] Width measured from TB-at-peak to trailing edge. Automatically set from pulse.
        double       fPulseRefTbMin;
        double       fPulseRefTbMax;
        int          fNDFPulse;

        TString      fPulseFileName;

        TClonesArray* fGraphArray = nullptr;
        Int_t         fNumGraphs = 0;

#ifdef DEBUG_CHANA_FINDPEAK
    public:
        int fNumGraphFP = 0;
        TGraph* dGraphA = nullptr; // ascending
        TMultiGraph* dMGraphFP = nullptr; // find peak multi-graph
        TClonesArray* dGraphFPArray = nullptr; // find peak graph array
#endif

#ifdef DEBUG_CHANA_FITPULSE
    public:
        TGraph* dGraph_it_tb = nullptr;
        TGraph* dGraph_it_tbStep = nullptr;
        TGraph* dGraph_it_chi2 = nullptr;
        TGraph* dGraph_it_slope = nullptr;
        TGraph* dGraph_tb_chi2 = nullptr;
        TGraph* dGraph_tb_slope = nullptr;
        TGraph* dGraph_it_slopeInv = nullptr;
        TGraph* dGraph_tb_slopeInv = nullptr;
#endif

        TH1D* fHistBuffer = nullptr;

    ClassDef(LKChannelAnalyzer,1);
};

#endif
