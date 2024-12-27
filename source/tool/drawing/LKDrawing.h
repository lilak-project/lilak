#ifndef LKDRAWING_HH
#define LKDRAWING_HH

#include "TPaveStats.h"
#include "TPaveText.h"
#include "TObjArray.h"
#include "TString.h"
#include "TLegend.h"
#include "TNamed.h"
#include "TGraph.h"
#include "LKCut.h"
#include "TTree.h"
#include "TPad.h"
#include "TF1.h"
#include "TH1.h"
#include "TH2.h"
#include <vector>
using namespace std;

#include "LKMisc.h"

class LKDrawing : public TObjArray
{
    public:
        LKDrawing(TString name="");
        LKDrawing(TObject* obj, TObject* obj1=nullptr, TObject* obj2=nullptr, TObject* obj3=nullptr);
        ~LKDrawing() {}

        virtual const char* GetName() const;
        /// option
        /// - "v" : Draw with LKDataViewer
        /// - "raw" : Skip almost all functions of LKDrawing.
        /// - raw : Skip almost all functions of LKDrawing.
        virtual void Draw(Option_t *option="");
        virtual void Print(Option_t *option="") const;
        virtual Int_t Write(const char *name = nullptr, Int_t option=TObject::kSingleKey, Int_t bufsize = 0) const;
        virtual void Clear(Option_t *option="");
        void Init();
        void CopyTo(LKDrawing* drawing, bool clearFirst=true);
        virtual Double_t GetHistEntries() const;

        ////////////////////////////////////////////////////////////////////////////////////
        void Fill(TTree* tree);

        ////////////////////////////////////////////////////////////////////////////////////
        virtual void Add(TObject *obj) { Add(obj,"",""); }
        /// drawOption will be used to draw the object when Draw method is called. Some specaial cases are:
        /// - "stat0" : if object is histogram, SetStat(0) will be called to the histogram
        /// - "drawx" : object will be added to the list but will not be drawn.
        /// - "[A]>[B]" : object will be drawn with [A] option at it's order and will be drawn
        /// once more with [B] option after all the objects in the list are drawn. (only for histograms and graphs)
        ///
        /// title will be used when creating legend using SetCreateLegend method. Some specaial cases are:
        /// - "legendx" : This object will not be added to the legend when SetCreateLegend is called.
        /// - "." : Same as legendx
        void Add(TObject *dataObj, TString drawOption, TString title="");
        void AddLegendLine(TString title);
        void SetFitObjects(TObject *data, TF1 *fit);
        void AddDrawing(LKDrawing *drawing);
        void SetTitle(int i, TString title) { fTitleArray[i] = title; }
        void SetOption(int i, TString option) { fDrawOptionArray[i] = option; }
        void SetCanvas(TVirtualPad* cvs) { fCvs = (TPad*) cvs; }
        void SetCanvas(TPad* cvs) { fCvs = (TPad*) cvs; }
        void SetCanvas(TCanvas* cvs) { fCvs = (TPad*) cvs; }

        ////////////////////////////////////////////////////////////////////////////////////
        int GetNumDrawings() const { return GetEntries(); }
        TString GetTitle(int i) const { return fTitleArray.at(i); }
        TString GetOption(int i) const { return fDrawOptionArray.at(i); }
        TPad* GetCanvas() { return fCvs; }
        TH1* GetMainHist() { return fMainHist; }
        LKCut* GetCut() { return fCuts; }
        TObject* FindObjectStartWith(const char* name) const;

        ////////////////////////////////////////////////////////////////////////////////////
        void SetHistColor(TH2* hist, int color, int max);
        void GetPadCorner(TPad *cvs, int iCorner, double &x_corner, double &y_corner, double &x_unit, double &y_unit);
        void GetPadCornerBoxDimension(TPad *cvs, int iCorner, double dx, double dy, double &x1, double &y1, double &x2, double &y2);
        TPaveStats* MakeStats(TPad *cvs);
        bool MakeStatsBox(TPad *cvs, int iCorner=0, int fillStyle=-1);
        void MakeLegendBelowStats(TPad* cvs, TLegend *legend);
        void MakeLegendCorner(TPad* cvs, TLegend *legend, int iCorner=0);
        void MakePaveTextCorner(TPad* cvs, TPaveText *pvtt, int iCorner=0);
        void SetMainHist(TPad *pad, TH1* hist);

        TH2D* MakeGraphFrame();
        TH2D* MakeGraphFrame(TGraph* graph, TString mxyTitle="");
        void AddGraphFrame(TGraph* graph, TString mxyTitle="", TString drawOption="", TString title="") { Add(MakeGraphFrame(graph,mxyTitle),drawOption,title); }

        ////////////////////////////////////////////////////////////////////////////////////
        TString GetGlobalOption() const { return fGlobalOption; }
        void SetGlobalOption(TString option) { fGlobalOption = option; }
        bool CheckOption(TString option) { return LKMisc::CheckOption(fGlobalOption,option); }
        int FindOptionInt(TString option, int value) { return LKMisc::FindOptionInt(fGlobalOption,option,value); }
        double FindOptionDouble(TString option, double value) { return LKMisc::FindOptionDouble(fGlobalOption,option,value); }
        TString FindOptionString(TString option, TString value) { return LKMisc::FindOptionString(fGlobalOption,option,value); }

        ////////////////////////////////////////////////////////////////////////////////////
        void RemoveOption(TString option);
        /**
         * - log[x,y,z]
         * - grid[x,y]
         * - legend_below_stats : place legend just below statistics box
         * - histcc : enable color comparisons of 2d-histograms by setting histogram contents to have different values
         * - merge_pvtt_stats : merge TPaveText with stats box
         * - create_frame : create frame if no frame histogram exist
         */
        void AddOption(TString option) { LKMisc::AddOption(fGlobalOption,option); }
        /**
         * - [x,y][1,2] : SetRangeUser x,y
         * - [l,r,b,t]_margin : canvas margin
         * - pave_dx : (0.280) statistics box dx
         * - pave_line_dy : (0.050) statistics box dy for each line
         * - font : (132) default font
         * - [m,x,y,z]_[title/label]_[size/font/offset] : text attributes (m for top main title)
         * - pad_color : pad color (TODO)
         * - stats_corner : place statistics box at the corner of frame (0:TR,1:TL,2:BL,3:BR)
         * - pave_corner : place pave text at the corner of frame (0:TR,1:TL,2:BL,3:BR)
         * - pave_attribute : if 0, use default pave text attibute (white bg, black text with 132)
         * - legend_corner : place legend box at top corner of histogram frame (0:TR,1:TL,2:BL,3:BR)
         * - opt_stat : ksiourmen (default is 1111)
         *      k = 1;  kurtosis printed
         *      k = 2;  kurtosis and kurtosis error printed
         *      s = 1;  skewness printed
         *      s = 2;  skewness and skewness error printed
         *      i = 1;  integral of bins printed
         *      o = 1;  number of overflows printed
         *      u = 1;  number of underflows printed
         *      r = 1;  rms printed
         *      r = 2;  rms and rms error printed
         *      m = 1;  mean value printed
         *      m = 2;  mean and mean error values printed
         *      e = 1;  number of entries printed
         *      n = 1;  name of histogram is printed
         * - opt_fit : pcev (default is 111)
         *      p = 1;  print Probability
         *      c = 1;  print Chisquare/Number of degrees of freedom
         *      e = 1;  print errors (if e=1, v must be 1)
         *      v = 1;  print name/values of parameters
         */
        void AddOption(TString option, double value) { LKMisc::AddOption(fGlobalOption,option,value); }
        void AddOption(TString option, int value) { LKMisc::AddOption(fGlobalOption,option,value); }
        void AddOption(TString option, TString value) { LKMisc::AddOption(fGlobalOption,option,value); }

        ////////////////////////////////////////////////////////////////////////////////////
        void SetLogx()  { AddOption("logx"); }
        void SetLogy()  { AddOption("logy"); }
        void SetLogz()  { AddOption("logz"); }
        void SetGridx() { AddOption("gridx"); }
        void SetGridy() { AddOption("gridy"); }
        void SetLegendBelowStats() { AddOption("legend_below_stats"); }
        void SetHistCCMode() { AddOption("histcc"); }
        void SetMergePaveTextToStats() { AddOption("merge_pvtt_stats"); }

        void SetLeftMargin(double mg)   { AddOption("l_margin",mg); }
        void SetRightMargin(double mg)  { AddOption("r_margin",mg); }
        void SetBottomMargin(double mg) { AddOption("b_margin",mg); }
        void SetTopMargin(double mg)    { AddOption("t_margin",mg); }
        void SetCanvasMargin(double lmg, double rmg, double bmg, double tmg) {
            SetLeftMargin(lmg);
            SetRightMargin(rmg);
            SetBottomMargin(bmg);
            SetTopMargin(tmg);
        }
        void SetCanvasSize(double dx, double dy, bool resize=true) { AddOption("cvs_dx",dx); AddOption("cvs_dy",dy); if (resize) AddOption("cvs_resize"); }
        void SetRangeUser(double x1, double x2, double y1, double y2) { SetRangeUserX(x1, x2); SetRangeUserY(y1, y2); }
        void SetRangeUserX(double x1, double x2) { AddOption("x1",x1); AddOption("x2",x2); }
        void SetRangeUserY(double y1, double y2) { AddOption("y1",y1); AddOption("y2",y2); }
        //void SetLegendCorner(int iCorner) { RemoveOption("legend_below_stats"); AddOption("legend_corner",iCorner); }
        void SetLegendCorner(int iCorner) { AddOption("legend_corner",iCorner); }
        void SetStatCorner(int iCorner) { AddOption("stats_corner",iCorner); }
        void SetPaveCorner(int iCorner) { AddOption("pave_corner",iCorner); }
        void SetPaveAttribute(int attribute) { AddOption("pave_attribute",attribute); }
        void SetStatTopRightCorner() { AddOption("stats_corner",0); }
        void SetStatTopLeftCorner() { AddOption("stats_corner",1); }
        void SetStatBottomLeftCorner() { AddOption("stats_corner",2); }
        void SetStatBottomRightCorner() { AddOption("stats_corner",3); }
        void SetStatsFillStyle(int fillStyle) { AddOption("stats_fillstyle",fillStyle); }
        void SetOptStat(int mode) { AddOption("opt_stat",mode); }
        void SetOptFit(int mode) { AddOption("opt_fit",mode); }
        //void SetMainTitleAttribute();
        //void SetTitleAttribute(int i, int font, double size, double offset);
        //void SetLabelAttribute(int i, int font, double size, double offset);
        void SetPaveDx(double dx) { AddOption("pave_dx",dx); }
        void SetPaveLineDy(double dy_line) { AddOption("pave_line_dy",dy_line); }
        void SetPaveSize(double dx, double dy_line) { SetPaveDx(dx); SetPaveLineDy(dy_line); }
        void SetCreateFrame(TString name="", TString title="") { AddOption("create_gframe"); AddOption("gframe_name",name); AddOption("gframe_title",title); } ///< Create frame if no frame histogram exist.
        void SetCreateLegend(int iCorner=-1, double dx=0, double dyline=0) {
            AddOption("create_legend");
            SetLegendBelowStats();
            if (iCorner>=0) SetLegendCorner(iCorner);
            if (dx>0) SetPaveDx(dx);
            if (dyline>0) SetPaveLineDy(dyline);
        }

    private:
        void MakeLegend(bool remake=false);

    private:
        TString fGlobalOption = "stats_corner:font=132:opt_stat=1110";
        vector<TString> fTitleArray;
        vector<TString> fDrawOptionArray;

    private:
        LKCut*   fCuts         = nullptr; //!
        TPad*    fCvs          = nullptr; //!
        TH1*     fMainHist     = nullptr;
        //TGraph*  fMainGraph    = nullptr;
        //TF1*     fMainFunction = nullptr;
        TH1*     fHistPixel    = nullptr; //!
        TLegend* fLegend       = nullptr; //!

    ClassDef(LKDrawing, 1)
};

#endif
