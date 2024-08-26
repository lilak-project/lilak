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

class LKDrawing : public TNamed
{
    public:
        LKDrawing(TString name="");
        LKDrawing(TObject* obj, TObject* obj1=nullptr, TObject* obj2=nullptr, TObject* obj3=nullptr);
        ~LKDrawing() {}

        virtual const char* GetName() const;
        virtual const char* GetTitle() const;
        virtual void Draw(Option_t *option="");
        virtual void Print(Option_t *option="") const;
        virtual void Clear(Option_t *option="");
        virtual Double_t GetEntries() const;

        void Add(TObject *obj, TString title="", TString drawOption="", bool isMain=false);
        void SetTitle(int i, TString title) { fTitleArray[i] = title; }
        void SetOption(int i, TString option) { fOptionArray[i] = option; }
        void SetCanvas(TVirtualPad* cvs) { fCvs = (TPad*) cvs; }
        void SetCanvas(TPad* cvs) { fCvs = (TPad*) cvs; }
        void SetCanvas(TCanvas* cvs) { fCvs = (TPad*) cvs; }

        int GetNumDrawings() const { return fDrawingArray.GetEntries(); }
        TString GetTitle(int i) const { return fTitleArray.at(i); }
        TString GetOption(int i) const { return fOptionArray.at(i); }
        TObject* GetDrawing(int i) const { return fDrawingArray.At(i); }
        TPad* GetCanvas()  const { return fCvs; }
        TH1* GetMainHist() const { return fHist; }
        TObject* GetMain() const { return fMain; }

        void SetCanvasOption(TString value) {
            if (value.Index("logx")>=0)  { value.ReplaceAll("logx","");  fSetLogX  = true; }
            if (value.Index("logy")>=0)  { value.ReplaceAll("logy","");  fSetLogY  = true; }
            if (value.Index("logz")>=0)  { value.ReplaceAll("logz","");  fSetLogZ  = true; }
            if (value.Index("gridx")>=0) { value.ReplaceAll("gridx",""); fSetGridX = true; }
            if (value.Index("gridy")>=0) { value.ReplaceAll("gridy",""); fSetGridY = true; }
        }
        void SetLogx (bool value=true) { fSetLogX  = value; }
        void SetLogy (bool value=true) { fSetLogY  = value; }
        void SetLogz (bool value=true) { fSetLogZ  = value; }
        void SetGridx(bool value=true) { fSetGridX = value; }
        void SetGridy(bool value=true) { fSetGridY = value; }

    private:
        void MakeLegend();

    private:
        TObjArray       fDrawingArray;
        vector<TString> fTitleArray;
        vector<TString> fOptionArray;

        int fMainIndex = -1;

        TPad* fCvs = nullptr; //!
        TObject* fMain = nullptr; //!
        TH1* fHist = nullptr;
        TLegend* fLegend = nullptr;

        TH1* fHistPixel = nullptr; //!

        bool fSetLogX  = false;
        bool fSetLogY  = false;
        bool fSetLogZ  = false;
        bool fSetGridX = false;
        bool fSetGridY = false;

    ClassDef(LKDrawing, 1)
};

#endif
