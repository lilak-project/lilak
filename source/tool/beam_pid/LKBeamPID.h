#ifndef LKBEAMPID_HH
#define LKBEAMPID_HH

#include "LKParameterContainer.h"
#include "LKDrawingGroup.h"
#include "LKDrawing.h"
#include "LKPainter.h"
#include "LKBinning.h"
#include "LKLogger.h"

#include "TGraphAsymmErrors.h"
#include "TObjArray.h"
#include "TString.h"
#include "TSystem.h"
#include "TStyle.h"
#include "TGraph.h"
#include "TLatex.h"
#include "TText.h"
#include "TFile.h"
#include "TTree.h"
#include "TH2D.h"
#include "TH1D.h"
#include "TF2.h"
#include "TF1.h"

#include <vector>
#include <fstream>
#include <iomanip>

using namespace std;

class LKBeamPID
{
    public:
        LKBeamPID();
        LKBeamPID(TVirtualPad *pad): LKBeamPID() { UsePad(pad); }

        void InitParameters();
        void InitBeamPIDValues(int numPIDs);

        bool ListFiles(TString path="", TString format="");
        bool SelectFile(int idx=-1);
        void CreateAndFillHistogram(int printb=1);
        void UseCurrentgPad() { UsePad(gPad); }
        void UsePad(TVirtualPad *pad);
        void SelectCenters(vector<vector<double>> points=vector<vector<double>>{});
        void Redraw();
        void ReselectCenters();
        void FitTotal(int mode = 0);
        void CalibrateParFast();
        void CalibratePar() { FitTotal(1); }
        void CalibrateCnt() { FitTotal(2); }
        void CalibrateEta() { FitTotal(3); }
        void MakeSummary();

        void CalibrateEtaMan(int iPID1, int iPID2, TF2* fitTotal = (TF2*)nullptr);
        void DrawDetail();

        LKDrawing* GetFitTestDrawing(int iPID, TH2D *hist, TF2* fit, TF2* fitContanminent=(TF2*)nullptr, bool resetError=false);
        void EvaluateCounts(double parameters[6], double countData[6], int iPID, bool isSelectedEta, double sValue, double binArea, TH2D* hist, TF2* fit, TF2* fitContanminent=(TF2*)nullptr);
        TF2* Fit2DGaussian(TH2D *hist, int idx, double valueX, double valueY, double sigmaX=0, double sigmaY=0, double theta=0);
        TGraph *GetContourGraph(double sValue, double amplit, double valueX, double sigmaX, double valueY, double sigmaY, double thetaR);
        double IntegralInsideGraph(TH2D* hist, TGraph* graph, bool justCount=true);
        double IntegralInsideGraph(TH2D* hist, TGraph* graph, TF2 *f2, bool justCount=true);
        double Integral2DGaussian(double amplitude, double sigmaX, double sigmaY, double contourS=0);
        double Integral2DGaussian(TF2 *f2, double contourS=0);
        void CollectRootFiles(std::vector<TString> &listGenFile, TString dataPath="", TString format="");

        void Help(TString mode="help");
        void AutoBinning();
        void PrintBinning();
        void ResetBinning();
        void SaveBinning();
        void SetSValue(double scale=-1);
        void SetEta(double scale=-1);
        void SetXBinSize(double w, int fill=0);
        void SetYBinSize(double w, int fill=0);
        void SetGausFitRange(double sigDist=-1);
        void SetRunNumber(int run);
        void SaveConfiguration();

        void SetBinningX(int nx, double x1, double x2, int fill=0);
        void SetBinningY(int ny, double y1, double y2, int fill=0);
        void SetRangeX(double x1, double x2, int fill=0);
        void SetRangeY(double y1, double y2, int fill=0);
        void SetBinning(int nx, double x1, double x2, int ny, double y1, double y2);
        void SetBinning(double x1, double x2, double y1, double y2);
        void SetBinNX(double n, int fill=1);
        void SetBinNY(double n, int fill=0);

    protected:
        bool fCalibratedPar = false;
        bool fCalibratedCnt = false;
        bool fCalibratedEta = false;

    private:
        int fStage = 0;
        LKDrawingGroup *fTop = nullptr, *fGroupFit = nullptr, *fGroupPID = nullptr;
        LKDrawing *fDraw2D = nullptr;
        TObjArray *fHistDataArray = nullptr, *fHistFitGArray = nullptr, *fHistBackArray = nullptr, *fHistCalcArray = nullptr, *fHistTestArray = nullptr, *fHistErrrArray = nullptr;
        vector<double> fValueOfS, fErrorAtS;
        TObjArray* fPIDFitArray = nullptr;
        TObjArray* fContourGraphArray = nullptr;
        vector<TString> fListGenFile;
        TFile* fDataFile = nullptr;
        TTree* fDataTree = nullptr;
        TH2D *fHistPID = nullptr;
        bool fRunCollected = false, fInitialized = false;
        int fCurrentRunNumber=0, fCurrentType = 1;
        TGraph* fFinalContourGraph = nullptr;
        TGraph* fGraphTTOutTTIn = nullptr;
        const TString fFormulaRotated2DGaussian = "[0]*exp(-0.5*(pow(((x-[1])*cos([5])+(y-[2])*sin([5]))/[3],2)+pow((-(x-[1])*sin([5])+(y-[2])*cos([5]))/[4],2)))";
        TString fCurrentFileName;
        TF1* fFitCountDiff = nullptr;
        TF1* fFitCountDiff2 = nullptr;
        TF1* fFitCountDiff3 = nullptr;
        double fOverallError = 0;
        TF2* fFitTotal = nullptr;

        LKBinning fBnn1, fBnn0;
        TString fSetXName, fSetYName;
        TString fXName, fYName;
        TString fDefaultPath = "./";
        TString fDefaultFormat = "gen.root";
        double fDefaultFitSigmaX = 2.5;
        double fDefaultFitSigmaY = 1.5;
        double fDefaultFitTheta = 21;
        double fAmpRatioRange = 0.2;
        double fPosRatioRangeInSig = 0.1;
        double fSigmaRatioRange = 0.2;
        double fThetaRange = 0.1*TMath::Pi();
        double fFitRangeInSigma = 1;
        double fFixSigmaX = -1;
        double fFixSigmaY = -1;
        double fFixThetaR = -1;
        int fNumContours = 20;
        double fSelectedEta = 0.2;
        vector<double> fCompareEtaList;
        vector<double> fDrawEtaList = {0.9,0.5};
        int fFrameIndex = 0;
        int fContourColor = kRed;
        int fPIDIndexTextColor1 = kBlack;
        int fPIDIndexTextColor2 = kGreen;
        int fLegendFillStyle = 3001;

        vector<vector<double>> fBeamPIDList;
        vector<vector<double>> fFittingList;
        vector<vector<double>> fCompareList;

        // fBeamPIDList[iPID][0] // 0 total
        // fBeamPIDList[iPID][1] // 1 count svalue
        // fBeamPIDList[iPID][2] // 2 count data
        // fBeamPIDList[iPID][3] // 3 count fit
        // fBeamPIDList[iPID][4] // 4 contamination count
        // fBeamPIDList[iPID][5] // 5 contamination error
        // fBeamPIDList[iPID][6] // 6 corrected count
        // fBeamPIDList[iPID][7] // 7 purity (contour_count/total)
        // fBeamPIDList[iPID][8] // 8 purity (bg_subtracted_count/total)
        // fBeamPIDList[iPID][9] // 9 overall error
        // fFittingList[iPID][0] // 0 amplitude
        // fFittingList[iPID][1] // 1 valueX
        // fFittingList[iPID][2] // 2 valueY
        // fFittingList[iPID][3] // 3 sigmaX
        // fFittingList[iPID][4] // 4 sigmaY
        // fFittingList[iPID][5] // 5 theta
        // fCompareList[iPID][0] // 1 compare-2 svalue
        // fCompareList[iPID][1] // 1 compare-1 count
        // fCompareList[iPID][2] // 2 compare-1 systematic
        // fCompareList[iPID][3] // 3 compare-2 svalue
        // fCompareList[iPID][4] // 4 compare-2 count
        // fCompareList[iPID][5] // 5 compare-2 systematic

    ClassDef(LKBeamPID, 1)
};

#define e_title std::cout << "\033[0;35m" << "[RUN-" << fCurrentRunNumber << "] ============= "  << "\033[0m"

#endif
