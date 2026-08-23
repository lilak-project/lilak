#ifndef LKCHANNELSIMULATOR_HH
#define LKCHANNELSIMULATOR_HH

#include "TObject.h"
#include "LKLogger.h"
#include "LKPulse.h"
#include "TH1.h"
#include "TF1.h"

class LKParameterContainer;

/**
 * @brief Simulate and fill buffer with given pulse data and user input parameters
 *
 * Example of using LKChannelSimulator:
 *
 * @code{.cpp}
 *  {
 *      gRandom -> SetSeed(time(0));
 *
 *      const int chMax = 4096;
 *      const int tbMax = 350;
 *
 *      auto sim = new LKChannelSimulator();
 *      sim -> SetYMax(chMax);
 *      sim -> SetTbMax(tbMax);
 *      sim -> SetNumSmoothing(2);
 *      sim -> SetSmoothingLength(2);
 *      sim -> SetPedestalFluctuationLength(3);
 *      sim -> SetPedestalFluctuationLevel(20);
 *      sim -> SetPulseErrorScale(0.05);
 *      sim -> SetBackGroundLevel(100);
 *
 *      int buffer[tbMax] = {0};
 *
 *      int numSimulations = 100;
 *      for (auto iSim=0; iSim<numSimulations; ++iSim)
 *      {
 *          memset(buffer, 0, sizeof(buffer));
 *
 *          sim -> AddFluctuatingPedestal(buffer);
 *
 *          auto tbHit = GetTbSomehow();
 *          auto amplitude = GetAmplitudeSomehow();
 *          sim -> AddHit(buffer,tbHit,amplitude);
 *
 *          AnalyzeBuffer(buffer);
 *      }
 *  }
 * @endcode
 */
class LKChannelSimulator : public TObject
{
    public:
        LKChannelSimulator();
        virtual ~LKChannelSimulator() { ; }

        void Print(Option_t *option="") const;

        void Init() { if (fBuffer==nullptr) fBuffer = new int[fTbMax]; }
        void Reset() { Init(); memset(fBuffer, 0, sizeof(int)*fTbMax); }

        void SetPulse(const char* fileName);
        void SetPulse(LKParameterContainer* par, TString groupName="LKPulse");
        void SetPulse(LKPulse* pulse);
        LKPulse* GetPulse() const { return fPulse; }
        void SetUsePulseFunction(bool value);
        void SetPulseFunctionParameters(double baseline, double peak, double t0, double alpha, double tau);
        void SetPulseFunctionRange(double tbMin, double tbMax);
        void UpdatePulseFunctionParameters(LKParameterContainer* par, TString groupName="LKChannelSimulator");
        TF1* GetPulseFunction(TString name="pulse_function", bool subtractBaseline=true);

        void SetYMax(int yMax) { fYMax = yMax; }
        void SetTbMax(int yMax) { fTbMax = yMax; }
        void SetNumSmoothing(int num) { fNumSmoothing = num; }
        void SetSmoothingLength(int length) { fSmoothingLength = length; }
        void SetPedestalFluctuationLevel(double level) { fPedestalFluctuationLevel = level; }
        void SetPedestalFluctuationLength(int length, double error=0.1) { fPedestalFluctuationLength = length; fPedestalFluctuationLengthError = error; }
        void SetBackGroundLevel(double mean, double sigma=-1, double min=0, double max=0);
        void SetPulseErrorScale(double scale) { fPulseErrorScale = scale; }
        void SetCutBelow0(bool value) { fCutBelow0 = value; }

        double GetPedestalFluctuationLevel() { return fPedestalFluctuationLevel; }

        void AddPedestal();
        void AddFluctuatingPedestal();
        void AddHit(double tb0, double amplitude);

        int* GetBuffer() { return fBuffer; }
        void FillHist(TH1* hist);
        TH1D* GetHist(TString name);

    protected:
        void Smoothing(int n, int smoothLevel, int numSmoothing);
        double SampleBackGroundLevel();

    private:
        LKPulse*     fPulse = nullptr;

        int          fYMax = 4096; ///< Max value of channel y. Must be set with SetYMax().
        int          fTbMax = 512; ///< Max value of TB. Must be set with SetTbMax().
        int          fNumSmoothing = 2; ///< Number of iteration for Smoothing. Can be set with SetNumSmoothing().
        int          fSmoothingLength = 4; ///< Number of bins to be used for smoothing one bin. Can be set with SetSmoothingLength().
        double       fFloorRatio = 0.05;
        double       fBackGroundLevel = 400; ///< (ADC) Fixed pedestal or mean pedestal if fBackGroundLevelSigma is non-negative.
        double       fBackGroundLevelSigma = -1; ///< (ADC) If non-negative, sample pedestal from Gaussian(mean,sigma).
        double       fBackGroundLevelMin = 0; ///< (ADC) Lower accepted sampled pedestal, used only when min<max.
        double       fBackGroundLevelMax = 0; ///< (ADC) Upper accepted sampled pedestal, used only when min<max.
        double       fPedestalFluctuationLevel = 1; ///< (ADC) Absolute pedestal fluctuation sigma. Will be set from pulse data file.
        int          fPedestalFluctuationLength = 4; ///< Used for SetFluctuatingPedestal(). The width of bakcground fluctuatation will be around this value. Can be set with SetPedestalFluctuationLength().
        double       fPedestalFluctuationLengthError = 0;
        double       fPulseErrorScale = 0.05; ///<
        bool         fCutBelow0 = true;

        int*         fBuffer = nullptr;

    ClassDef(LKChannelSimulator,2);
};

#endif
