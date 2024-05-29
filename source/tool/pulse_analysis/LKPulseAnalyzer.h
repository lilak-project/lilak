#ifndef LKPULSELANALYZER_HH
#define LKPULSELANALYZER_HH

#include "TObject.h"
#include "LKLogger.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TVirtualPad.h"
#include "TGraph.h"
#include "TObjArray.h"
#include "TFile.h"
#include "TTree.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TParameter.h"

/**
 * @brief LKPulseAnalyzer find channels containing single pulse and create reference pulse from their average.
 * The LKPulseAnalyzer find the reference pulse from the group of channels those are likely to have similar waveform with same width.
 *
 * Steps of creating reference pulse:
 * 1. Collected data through AddChannel(buffer).
 * 2. Check if buffer has value that is lartger than threshold.
 * 3. Check if pulse width at threshold is within the width cut.
 * 4. Check if pulse height is within the height cut.
 * 5. Check if pulse-TB (Time Bucket) is within the TB cut.
 * 6. Check if number of pulses found from the step 2.-5. is 1. => good channel!
 * 7. Align Peak position to TB=0.
 * 8. Normalize pulse height to 1.
 * 9. Accumulate aligned-normalized buffer with other good channels.
 * 10. Find average y-values for each TB from accumulated buffers to create reference pulse.
 * 11. Find error_PD = standard deviation of pedestal region [fRefRange1,fRefRange2) and [fRefRange3,fRefRange4).
 * 12. Find error_TB = standard deviation of y-value for each TB to create reference pulse error.
 * 13. The error for each TB is sqrt(error_PD^2 + error_TB^2)
 *
 * Parameters that must be considered are as follow:
 * @param fTbRange1  : All analysis will be using TB range from fTbRange1 to fTbRange2. Must be set with SetTbRange()
 * @param fTbRange2  : All analysis will be using TB range from fTbRange1 to fTbRange2. Must be set with SetTbRange()
 * @param fYMax      : Maximum dynamic range, maximum y value. Must be set with SetYMax()
 * @param fThreshold : [1st cut] Pulse heigth from pedestal should be larger than this value to proceed next cut, comparing pulse width. Must be set with SetThreshold()
 * @param fPulseWidthAtThresholdMin : [2nd cut] Pulse width at threshold should be between fPulseWidthAtThresholdMin and fPulseWidthAtThresholdMax to proceed to next cut. 
 * @param fPulseWidthAtThresholdMax : [2nd cut] Pulse width at threshold should be between fPulseWidthAtThresholdMin and fPulseWidthAtThresholdMax to proceed to next cut. 
 * @param fPulseHeightMin : [3rd cut] Pulse height must be between fPulseHeightMin and fPulseHeightMax to proceed to next cut. Must be set with SetPulseHeightCuts()
 * @param fPulseHeightMax : [3rd cut] Pulse height must be between fPulseHeightMin and fPulseHeightMax to proceed to next cut. Must be set with SetPulseHeightCuts()
 * @param fPulseTbMin : [4th cut] Pulse time must be between fPulseTbMin and fPulseTbMax to be chosen for collection. Must be set with SetPulseTbCuts
 * @param fPulseTbMax : [4th cut] Pulse time must be between fPulseTbMin and fPulseTbMax to be chosen for collection. Must be set with SetPulseTbCuts
 * @param fInvertChannel : The values in buffer are inverted : new_buffer[i] = fYMax - buffer[i]. Must be set with SetInvertChannel() if buffer has to be inverted.
 * @param fFixPedestal : If fFixPedestal is larger than -10000, pedestal will not be calculated, but will be fixed to fFixPedestal. Must be set with SetFixPedestal() if needed.
 * @param fRefRange1(234) : error_PD = stddev in region TB=[1,2),[3,4) where peak is at TB=100. Must be set with SetPedestalTBRange()
 *
 * If enough number of channels are added, pulse data file can be created using WriteReferencePulse() method.
 * The file name will be "[fPath]/pulseReference_[fName].root" containing:
 * @param pulse          TGraphErrors drawing pulse
 * @param error          TGraph drawing pulse error
 * @param numAnaChannels TParameter<int> number of channels used to draw pulse   
 * @param threshold      TParameter<int> threshold used to collect channels
 * @param yMin           TParameter<int> y-range-cut-1 to collect channels
 * @param yMax           TParameter<int> y-range-cut-2 to collect channels
 * @param xMin           TParameter<int> x-range-cut-1 to collect channels
 * @param xMax           TParameter<int> x-range-cut-2 to collect channels  
 * @param FWHM           TParameter<double> Full Width Half Maximum of the pulse
 * @param ratio          TParameter<double> ratio is used for calculating with of the pulse at y-value = = ratio * [pulse_height]. See below parameters.
 * @param width          TParameter<double> Width of the pulse at y-value = ratio * [pulse_height].
 * @param widthLeading   TParameter<double> Width from leading edge at y-value = ratio * [pulse_height] to peak position.
 * @param widthTrailing  TParameter<double> Width from peak position to trailing edge at y-value = ratio * [pulse_height].
 *
 * pulse data tree can also be written using WriteTree();
 * The file and tree name will be "[fPath]/summary_[fName].root" and "pulse".
 * Tree will contain analyzed parameters for each channel:
 *  @param isCollected : "true" if channel was collected. True means channel is single hit channel and goes into the cut..
 *  @param isSingle : "true" if channel is single hit channel.
 *  @param numPulse : number of pulses in channel
 *  @param pedestal : estimated pedestal
 *  @param tbAtMax : tb position at peak
 *  @param height : height of the pulse
 *  @param width : width of the pulse
 *  @param event
 *  @param cobo
 *  @param asad
 *  @param aget
 *  @param channel
 *
 * ID parameters such as event, cobo, asad, aget, channel will be filled if they were given from AddChannel() method.
 *
 * Users will be able to find cut parameters from histograms drawn from the pulse data tree.
 * @code{.cpp}
 *  {
 *      TTree* tree = get_tree_somehow;
 *      tree -> Draw("width>>hist_pulse_width");
 *      tree -> Draw("yMax>>hist_pulse_height");
 *      tree -> Draw("tbAtMax>>hist_pulse_tbAtMax");
 *  }
 * @endcode
 *
 * Example of using LKPulseAnalyzer:
 *
 * @code{.cpp}
 *  {
 *       auto ana = new LKPulseAnalyzer("MyExperiment");
 *       ana -> SetTbRange(0,350);
 *       ana -> SetThreshold(200);
 *       ana -> SetPulseHeightCuts(500,4000);
 *       ana -> SetPulseWidthCuts(4,20);
 *       ana -> SetPulseTbCuts(100,300);
 *       ana -> SetInvertChannel(false);
 *       ana -> SetFixPedestal(0);
 *       ana -> SetPedestalTBRange(20,70,180,280);
 *
 *       for (...) {
 *          double* buffer = get_data_somehow;
 *          ana -> AddChannel(buffer);
 *       }
 *
 *       ana -> WriteReferencePulse(); // this will create pulseReference_MyExperiment.root
 *       ana -> WriteTree(); // this will create summary_MyExperiment.root
 *  }
 * @endcode
 */
class LKPulseAnalyzer : public TObject
{
    public:
        //LKPulseAnalyzer(const char* name, const char *path=".");
        LKPulseAnalyzer(const char* name, const char *path=".");

        LKPulseAnalyzer() : LKPulseAnalyzer(0) { ; }
        virtual ~LKPulseAnalyzer() { ; }

        bool Init();
        void Clear(Option_t *option="");
        void Print(Option_t *option="") const;

        double* GetAverageData() { return fAverageData; }
        double* GetChannelData() { return fChannelData; }
        TCanvas* GetCvsAverage() const  { return fCvsAverage; }

        TH1D* GetHistMean() const  { return fHistMean; }
        TH1D* GetHistAverage() const  { return fHistAverage; }
        TH1D* GetHistPedestal() const  { return fHistPedestal; }
        TH2D* GetHistAccumulate() const  { return fHistAccumulate; }
        TH1D* GetHistYFluctuation() const  { return fHistYFluctuation; }
        TH1D* GetHistPedestalResidual() const  { return fHistPedestalResidual; }

        TTree* GetTree() { return fTree; }

        void SetInvertChannel(bool value) { fInvertChannel = value; }
        void SetTbRange(int range1, int range2) { fTbRange1 = range1; fTbRange2 = range2; }
        //void SetTbMax(int value) { fTbRange2 = value; }
        //void SetTbStart(int value) { fTbRange1 = value; }
        //void SetChannelMax(int value) { fYMax = value; }
        void SetYMax(int value) { fYMax = value; }
        void SetThreshold(int value) { fThreshold = value; }
        void SetPulseHeightCuts(int min, int max) {
            fPulseHeightMin = min;
            fPulseHeightMax = max;
        }
        void SetPulseTbCuts(int min, int max) {
            fPulseTbMin = min;
            fPulseTbMax = max;
        }
        void SetPulseWidthCuts(int min, int max) {
            fPulseWidthAtThresholdMin = min;
            fPulseWidthAtThresholdMax = max;
        }
        void SetCvsGroup(int w, int h, int x, int y) {
            fWGroup = w;
            fHGroup = h;
            fXGroup = x;
            fYGroup = y;
        }
        void SetCvsAverage(int w, int h) {
            fWAverage = w;
            fHAverage = h;
        }
        void SetFixPedestal(double value) { fFixPedestal = value; }
        void SetFloorRatio(double value) { fFloorRatio = value; }
        void SetPedestalTBRange(int range1, int range2, int range3, int range4) {
            fRefRange1 = range1;
            fRefRange2 = range2;
            fRefRange3 = range3;
            fRefRange4 = range4;
        }

        TCanvas* GetGroupCanvas() { return fCvsGroup; }
        int IsCollected() const { return fIsCollected; }
        int GetNumGoodChannels() const { return fCountGoodChannels; }
        int GetNumHistChannels() const { return fCountHistChannel; }
        int GetNumChannelPad() const { return fCountChannelPad; }
        int GetNumCvsGroup() const { return fCountCvsGroup; }
        double GetPedestalPry() const { return fPedestalPry; }
        //int GetChannelMax() const { return fYMax; }
        int GetYMax() const { return fYMax; }
        int GetFirstPulseTb() const { return fFirstPulseTb; }
        int GetMaxValue() const { return fMaxValue; }
        int GetTbAtMaxValue() const { return fTbAtMaxValue; }

        double GetBackGroundLevel() const  { return fBackGroundLevel; }
        double GetBackGroundError() const  { return fBackGroundError; }
        double GetFluctuationLevel() const  { return fFluctuationLevel*fYMax; }

        void AddChannel(double *data) { int buffer[512]; for (auto tb=fTbRange1; tb<fTbRange2; ++tb) buffer[tb] = (int)data[tb]; AddChannel(buffer,-1); }
        void AddChannel(int *data) { AddChannel(data,-1); }
        void AddChannel(int *data, int event, int cobo, int asad, int aget, int channel);
        void AddChannel(int *data, int channelID);

        void DumpChannel(Option_t *option="");
        TFile* WriteTree();

        bool DrawChannel(TVirtualPad* pad=(TVirtualPad*)nullptr);
        void MakeAccumulatePY();
        void MakeHistAverage();
        TCanvas* DrawAverage(TVirtualPad* pad=(TVirtualPad*)nullptr);
        TCanvas* DrawAccumulate(TVirtualPad* pad=(TVirtualPad*)nullptr);
        TCanvas* DrawWidth(TVirtualPad* pad=(TVirtualPad*)nullptr);
        TCanvas* DrawHeight(TVirtualPad* pad=(TVirtualPad*)nullptr);
        TCanvas* DrawPulseTb(TVirtualPad* pad=(TVirtualPad*)nullptr);
        TCanvas* DrawPedestal(TVirtualPad* pad=(TVirtualPad*)nullptr);
        TCanvas* DrawResidual(TVirtualPad* pad=(TVirtualPad*)nullptr);
        TCanvas* DrawReference(TVirtualPad *pad=(TVirtualPad*)nullptr);
        TCanvas* DrawHeightWidth(TVirtualPad* pad=(TVirtualPad*)nullptr);

        TObjArray *GetHistArray() { return fHistArray; }

        double FullWidthRatioMaximum(TH1D *hist, double ratioFromMax, double numSplitBin, double &x0, double &x1, double &error);
        double FullWidthRatioMaximum(TH1D *hist, double ratioFromMax, double numSplitBin=4) {
            double dummy;
            return FullWidthRatioMaximum(hist, ratioFromMax, numSplitBin, dummy, dummy, dummy);
        }

        TGraphErrors* GetReferencePulse(int tbOffsetFromHead=0, int tbOffsetFromTail=0);
        TFile* WriteReferencePulse(int tbOffsetFromHead=0, int tbOffsetFromTail=0);

        void SetCvs(TCanvas *cvs);
        void SetHist(TH1 *hist);

    private:
        TString fName = "";
        TString fPath = ".";

        // data
        TFile*       fFile = nullptr;
        TTree*       fTree = nullptr;

        // single channel cuts
        int          fTbRange1 = 0;     ///< All analysis will be using TB range from fTbRange1 to fTbRange2. Must be set with SetTbRange()
        int          fTbRange2 = 512;   ///< All analysis will be using TB range from fTbRange1 to fTbRange2. Must be set with SetTbRange()
        int          fYMax = 4096;      ///< Maximum dynamic range, maximum y value. Must be set with SetYMax()
        int          fThreshold = 1000; ///< [1st cut] Pulse heigth from pedestal should be larger than this value to proceed next cut, comparing pulse width. Must be set with SetThreshold()
        int          fPulseWidthAtThresholdMin = 4;  ///< [2nd cut] Pulse width at threshold should be between fPulseWidthAtThresholdMin and fPulseWidthAtThresholdMax to proceed to next cut. 
        int          fPulseWidthAtThresholdMax = 30; ///< [2nd cut] Pulse width at threshold should be between fPulseWidthAtThresholdMin and fPulseWidthAtThresholdMax to proceed to next cut. 
        int          fPulseHeightMin = 1500; ///< [3rd cut] Pulse height must be between fPulseHeightMin and fPulseHeightMax to proceed to next cut. Must be set with SetPulseHeightCuts()
        int          fPulseHeightMax = 2500; ///< [3rd cut] Pulse height must be between fPulseHeightMin and fPulseHeightMax to proceed to next cut. Must be set with SetPulseHeightCuts()
        int          fPulseTbMin = 100; ///< [4th cut] Pulse time must be between fPulseTbMin and fPulseTbMax to be chosen for collection. Must be set with SetPulseTbCuts
        int          fPulseTbMax = 200; ///< [4th cut] Pulse time must be between fPulseTbMin and fPulseTbMax to be chosen for collection. Must be set with SetPulseTbCuts

        // single channel options
        bool         fInvertChannel = false; ///< The values in buffer are inverted : new_buffer[i] = fYMax - buffer[i]. Must be set with SetInvertChannel() if buffer has to be inverted.
        double       fFixPedestal = -99999; ///< If fFixPedestal is larger than -10000, pedestal will not be calculated, but will be fixed to fFixPedestal. Must be set with SetFixPedestal() if needed.

        //for (auto iPY=20;  iPY<70;  ++iPY) idxPD.push_back(iPY);
        //for (auto iPY=180; iPY<280; ++iPY) idxPD.push_back(iPY);
        int          fRefRange1 = 20;  ///< error_PD = stddev in region TB=[1,2),[3,4) where peak is at TB=100. Must be set with SetPedestalTBRange()
        int          fRefRange2 = 70;  ///< error_PD = stddev in region TB=[1,2),[3,4) where peak is at TB=100. Must be set with SetPedestalTBRange()
        int          fRefRange3 = 180; ///< error_PD = stddev in region TB=[1,2),[3,4) where peak is at TB=100. Must be set with SetPedestalTBRange()
        int          fRefRange4 = 280; ///< error_PD = stddev in region TB=[1,2),[3,4) where peak is at TB=100. Must be set with SetPedestalTBRange()

        // single channel parameters
        bool         fIsSinglePulseChannel = false;
        bool         fIsCollected = false;
        int          fMaxValue = 0;
        int          fTbAtMaxValue = 0;
        int          fFirstPulseTb = -1;
        int          fPreValue = 0;
        int          fCurValue = 0;
        int          fCountTbWhileAbove = 0;
        int          fCountPulse = 0;
        int          fCountWidePulse = 0;
        int          fChannelID = 0;
        bool         fValueIsAboveThreshold = false;
        int          fCountPedestalPry = 0;
        double       fPedestalPry = 0;
        double       fPedestal = 0;
        double       fWidth = 0;
        double       fChannelData[512];
        int          fEventID = -1;
        short        fCobo = -1;
        short        fAsad = -1;
        short        fAget = -1;
        int          fChannel = -1;

        // single channel draw
        bool         fRunAccumulatePY = false;
        int          fCountHistChannel = 0;
        int          fCountChannelPad = 0;
        int          fCountCvsGroup = 0;
        TCanvas*     fCvsGroup = nullptr;
        int          fWGroup = 1000;
        int          fHGroup = 800;
        int          fXGroup = 3;
        int          fYGroup = 2;

        // average
        int          fCountGoodChannels = 0;
        double       fAverageData[512];
        double       fTbAtRefFloor1 = -1;
        double       fTbAtRefFloor2 = -1;

        double       fFWHM = 0;
        double       fRefWidth = 0;
        double       fWidthLeading = 0;
        double       fWidthTrailing = 0;
        double       fFloorRatio = 0.05;

        double       fBackGroundLevel = 0;
        double       fBackGroundError = 0;
        double       fFluctuationLevel = 0;

        int          fPulseRefTbMin = 0;
        int          fPulseRefTbMax = 0;

        // background
        int          fCountGoodBackgrounds = 0;
        int          fCountBGBin[512] = {0};
        double       fBackground[512] = {0};


    public:
        // average draw
        TObjArray*   fHistArray = nullptr;
        TCanvas*     fCvsMean = nullptr;
        TH1D*        fHistMean = nullptr;
        TCanvas*     fCvsAverage = nullptr;
        TH1D*        fHistAverage = nullptr;
        TCanvas*     fCvsReference = nullptr;
        TH2D*        fHistReference = nullptr;
        TCanvas*     fCvsAccumulate = nullptr;
        TH2D*        fHistAccumulate = nullptr;
        int          fWAverage = 600;
        int          fHAverage = 500;

        TGraph*       fGraphAverage = nullptr;
        TGraphErrors* fGraphReferenceM100 = nullptr;
        TGraphErrors* fGraphReference = nullptr;
        TGraph*       fGraphReferenceError = nullptr;
        TGraph*       fGraphReferenceRawError = nullptr;

        // extra
        TCanvas*     fCvsWidth = nullptr;
        TH1D*        fHistWidth = nullptr;
        TCanvas*     fCvsHeight = nullptr;
        TH1D*        fHistHeight = nullptr;
        TCanvas*     fCvsPulseTb = nullptr;
        TH1D*        fHistPulseTb = nullptr;
        TCanvas*     fCvsResidual = nullptr;
        TH2D*        fHistResidual = nullptr;
        TCanvas*     fCvsPedestal = nullptr;
        TH1D*        fHistPedestal = nullptr;
        TH1D*        fHistReusedData = nullptr;
        TH1D*        fHistPedestalPry = nullptr;
        TCanvas*     fCvsHeightWidth = nullptr;
        TH2D*        fHistHeightWidth = nullptr;
        TH1D*        fHistYFluctuation = nullptr;
        TH1D*        fHistPedestalResidual= nullptr;

    ClassDef(LKPulseAnalyzer,1);
};

#endif
