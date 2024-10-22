#ifndef LKDRAWING_HH
#define LKDRAWING_HH

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
        //void Add(TObject *obj, TString drawOption) { Add(obj, "", drawOption); }
        //void Add(TObject *obj, TString title, TString drawOption);
        void Add(TObject *obj, TString drawOption, TString title="");
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

        ////////////////////////////////////////////////////////////////////////////////////
        void SetHistColor(TH2* hist, int color, int max);
        void GetPadCorner(TPad *cvs, int iCorner, double &x_corner, double &y_corner, double &x_unit, double &y_unit);
        void GetPadCornerBoxDimension(TPad *cvs, int iCorner, double dx, double dy, double &x1, double &y1, double &x2, double &y2);
        bool MakeStatsCorner(TPad *cvs, int iCorner=0);
        void MakeLegendBelowStats(TLegend *legend);
        void MakeLegendCorner(TLegend *legend);
        void SetMainHist(TPad *pad, TH1* hist);

        ////////////////////////////////////////////////////////////////////////////////////
        bool CheckOption(TString option) { return LKMisc::CheckOption(fGlobalOption,option); }
        int FindOptionInt(TString option, int value) { return LKMisc::FindOptionInt(fGlobalOption,option,value); }
        double FindOptionDouble(TString option, double value) { return LKMisc::FindOptionDouble(fGlobalOption,option,value); }
        TString FindOptionString(TString &option, TString value) { return LKMisc::FindOptionString(fGlobalOption,option,value); }

        ////////////////////////////////////////////////////////////////////////////////////
        void RemoveOption(TString option);
        /// - log[x,y,z]
        /// - grid[x,y]
        /// - stats_corner : place statistics box at top right corner of histogram frame
        /// - legend_corner : place legend box at top right corner of histogram frame
        /// - legend_below_stats : place legend just below statistics box
        /// - histcc : enable color comparisons of 2d-histograms by setting histogram contents to have different values
        void AddOption(TString option);
        /// - [x,y][1,2]       : SetRangeUser x,y
        /// - [l,r,b,t]lmargin : canvas margin
        /// - statsdx, 0.280   : statistics box dx
        /// - statsdy, 0.050   : statistics box dy for each line
        /// - font, 132        : default font
        /// - [m,x,y,z]_[title/label]_[size/font/offset] : text attributes (m for top main title)
        void AddOption(TString option, double value);

        ////////////////////////////////////////////////////////////////////////////////////
        void SetLogx()  { AddOption("logx"); }
        void SetLogy()  { AddOption("logy"); }
        void SetLogz()  { AddOption("logz"); }
        void SetGridx() { AddOption("gridx"); }
        void SetGridy() { AddOption("gridy"); }
        void SetLeftMargin(double mg)   { AddOption("lmargin",mg); }
        void SetRightMargin(double mg)  { AddOption("rmargin",mg); }
        void SetBottomMargin(double mg) { AddOption("bmargin",mg); }
        void SetTopMargin(double mg)    { AddOption("tmargin",mg); }
        void SetRangeUser(double x1, double x2, double y1, double y2) { SetRangeUserX(x1, x2); SetRangeUserY(y1, y2); }
        void SetRangeUserX(double x1, double x2) { AddOption("x1",x1); AddOption("x2",x2); }
        void SetRangeUserY(double y1, double y2) { AddOption("y1",y1); AddOption("y2",y2); }
        void SetHistCCMode() { AddOption("histcc"); }
        //void SetMainTitleAttribute();
        //void SetTitleAttribute(int i, int font, double size, double offset);
        //void SetLabelAttribute(int i, int font, double size, double offset);

    private:
        void MakeLegend();

    private:
        TString fGlobalOption = "stats_corner:legend_below_stats:font=132";
        vector<TString> fTitleArray;
        vector<TString> fDrawOptionArray;

        LKCut* fCuts = nullptr; //!
        TPad* fCvs = nullptr; //!
        TH1* fMainHist = nullptr;
        TH1* fHistPixel = nullptr; //!
        TLegend* fLegend = nullptr; //!

    ClassDef(LKDrawing, 1)
};

#endif
