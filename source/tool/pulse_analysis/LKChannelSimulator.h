#ifndef LKCHANNELSIMULATOR_HH
#define LKCHANNELSIMULATOR_HH

#include "TObject.h"
#include "LKLogger.h"
#include "LKPulse.h"
#include "TH1.h"

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
 *      sim -> SetPedestalFluctuationScale(0.2);
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

        void CreateBuffer() { fBuffer = new int[fTbMax]; }

        void SetPulse(const char* fileName);
        void SetYMax(int yMax) { fYMax = yMax; }
        void SetTbMax(int yMax) { fTbMax = yMax; CreateBuffer(); }
        void SetNumSmoothing(int num) { fNumSmoothing = num; }
        void SetSmoothingLength(int length) { fSmoothingLength = length; }
        void SetPedestalFluctuationScale(double scale) { fPedestalFluctuationScale = scale; }
        void SetPedestalFluctuationLength(int length) { fPedestalFluctuationLength = length; }
        void SetBackGroundLevel(double value) { fBackGroundLevel = value; }
        void SetPulseErrorScale(double scale) { fPulseErrorScale = scale; }
        void SetCutBelow0(bool value) { fCutBelow0 = value; }

        double GetPedestalFluctuationLevel() { return (fPedestalFluctuationScale * fPedestalFluctuationLevel); }

        void AddPedestal(int* buffer);
        void AddFluctuatingPedestal(int* buffer);
        void AddHit(int* buffer, double tb0, double amplitude);

        //void ResetBuffer() { memset(fBuffer, 0, sizeof(fBuffer)); }
        void ResetBuffer() { memset(fBuffer, 0, fTbMax); }
        void AddPedestal() { AddPedestal(fBuffer); }
        void AddFluctuatingPedestal() { AddFluctuatingPedestal(fBuffer); }
        void AddHit(double tb0, double amplitude) { AddHit(fBuffer, tb0, amplitude); }

        int* GetBuffer() { return fBuffer; }
        void FillHist(TH1* hist);

    protected:
        void Smoothing(int* buffer, int n, int smoothLevel, int numSmoothing);

    private:
        LKPulse*     fPulse = nullptr;

        int          fYMax = 4096; ///< Max value of channel y. Must be set with SetYMax().
        int          fTbMax = 512; ///< Max value of TB. Must be set with SetTbMax().
        int          fNumSmoothing = 2; ///< Number of iteration for Smoothing. Can be set with SetNumSmoothing().
        int          fSmoothingLength = 4; ///< Number of bins to be used for smoothing one bin. Can be set with SetSmoothingLength().
        double       fFloorRatio = 0.05;
        double       fBackGroundLevel = 400; ///< Pedestal. Will be set from pulse data file. Can be set with SetBackGroundLevel().
        double       fPedestalFluctuationLevel = 1; ///< Background pedestal will fluctuate around this value. Will be set from pulse data file.
        double       fPedestalFluctuationScale = 1; ///< scale=0 will draw flat background distribution where scale=1 will draw background with standard error. Can be set with SetPedestalFluctuationScale().
        int          fPedestalFluctuationLength = 4; ///< Used for SetFluctuatingPedestal(). The width of bakcground fluctuatation will be around this value. Can be set with SetPedestalFluctuationLength().
        double       fPulseErrorScale = 0.05; ///<
        bool         fCutBelow0 = true;

        int* fBuffer;

    ClassDef(LKChannelSimulator,1);
};

#endif
