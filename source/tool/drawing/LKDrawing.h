#ifndef LKDRAWING_HH
#define LKDRAWING_HH

#include "TObjArray.h"
#include "TString.h"
#include "TLegend.h"
#include "TNamed.h"
#include "TPad.h"
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
        virtual void Clear(Option_t *option="");
        void CopyTo(LKDrawing* drawing, bool clearFirst=true);
        virtual Double_t GetHistEntries() const;

        virtual void Add(TObject *obj) { Add(obj,"","",false); }
        void Add(TObject *obj, TString drawOption) { Add(obj, "", drawOption, false); }
        void Add(TObject *obj, TString title, TString drawOption, bool isMain=false);
        void AddDrawing(LKDrawing *drawing);
        void SetTitle(int i, TString title) { fTitleArray[i] = title; }
        void SetOption(int i, TString option) { fDrawOptionArray[i] = option; }
        void SetCanvas(TVirtualPad* cvs) { fCvs = (TPad*) cvs; }
        void SetCanvas(TPad* cvs) { fCvs = (TPad*) cvs; }
        void SetCanvas(TCanvas* cvs) { fCvs = (TPad*) cvs; }

        void SetRangeUser(double x1, double x2, double y1, double y2) { SetRangeUserX(x1, x2); SetRangeUserY(y1, y2); }
        void SetRangeUserX(double x1, double x2) { fSetXRange = true; fX1 = x1; fX2 = x2; }
        void SetRangeUserY(double y1, double y2) { fSetYRange = true; fY1 = y1; fY2 = y2; }

        int GetNumDrawings() const { return GetEntries(); }
        TString GetTitle(int i) const { return fTitleArray.at(i); }
        TString GetOption(int i) const { return fDrawOptionArray.at(i); }

        TPad* GetCanvas() { return fCvs; }
        TH1* GetMainHist() { return fMainHist; }

        void SetHistColor(TH2* hist, int color, int max);

        void AddOption(TString option);
        void AddOption(TString option, double value);
        void RemoveOption(TString option);

        void SetLogx (bool add=true) { if (add) AddOption("logx" ); else RemoveOption("logx" ); }
        void SetLogy (bool add=true) { if (add) AddOption("logy" ); else RemoveOption("logy" ); }
        void SetLogz (bool add=true) { if (add) AddOption("logz" ); else RemoveOption("logz" ); }
        void SetGridx(bool add=true) { if (add) AddOption("gridx"); else RemoveOption("gridx"); }
        void SetGridy(bool add=true) { if (add) AddOption("gridy"); else RemoveOption("gridy"); }
        void SetHistCCMode(bool value=true) { AddOption("histcc",value); } // 2d-histogram color classification mode
        void SetLeftMargin(double mg)   { AddOption("lmargin",mg); }
        void SetRightMargin(double mg)  { AddOption("rmargin",mg); }
        void SetBottomMargin(double mg) { AddOption("bmargin",mg); }
        void SetTopMargin(double mg)    { AddOption("tmargin",mg); }
        void SetMargin(double ml, double mr, double mb, double mt) {
            AddOption("lmargin",ml);
            AddOption("rmargin",mr);
            AddOption("bmargin",mb);
            AddOption("tmargin",mt);
        }

        bool GetLogx()  { return LKMisc::CheckOption(fGlobalOption,"logx" ); }
        bool GetLogy()  { return LKMisc::CheckOption(fGlobalOption,"logy" ); }
        bool GetLogz()  { return LKMisc::CheckOption(fGlobalOption,"logz" ); }
        bool GetGridx() { return LKMisc::CheckOption(fGlobalOption,"gridx"); }
        bool GetGridy() { return LKMisc::CheckOption(fGlobalOption,"gridy"); }
        bool GetHistCCMode() { return LKMisc::CheckOption(fGlobalOption,"histcc"); }
        double GetLeftMargin()   { auto val=LKMisc::FindOption(fGlobalOption,"lmargin"); return (val.IsNull()?-1:val.Atof()); }
        double GetRightMargin()  { auto val=LKMisc::FindOption(fGlobalOption,"rmargin"); return (val.IsNull()?-1:val.Atof()); }
        double GetBottomMargin() { auto val=LKMisc::FindOption(fGlobalOption,"bmargin"); return (val.IsNull()?-1:val.Atof()); }
        double GetTopMargin()    { auto val=LKMisc::FindOption(fGlobalOption,"tmargin"); return (val.IsNull()?-1:val.Atof()); }

    private:
        void MakeLegend();

    private:
        vector<TString> fTitleArray;
        vector<TString> fDrawOptionArray;
        TString fGlobalOption;

        bool fFirstHistIsSet = false;
        int fMainIndex = -1;
        TString fTitle;

        TPad* fCvs = nullptr; //!
        TH1* fMainHist = nullptr;
        TLegend* fLegend = nullptr;

        TH1* fHistPixel = nullptr; //!

        bool fSetXRange = false;
        bool fSetYRange = false;
        double fX1;
        double fX2;
        double fY1;
        double fY2;

        //bool fSetLogX  = false;
        //bool fSetLogY  = false;
        //bool fSetLogZ  = false;
        //bool fSetGridX = false;
        //bool fSetGridY = false;

    ClassDef(LKDrawing, 1)
};

#endif
