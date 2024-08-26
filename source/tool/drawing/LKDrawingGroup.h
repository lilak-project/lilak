#ifndef LKDRAWINGGROUP_HH
#define LKDRAWINGGROUP_HH

#include "TObjArray.h"
#include "TGraph.h"
#include "TH1.h"
#include "TCanvas.h"

#include "LKDrawing.h"
#include "LKPainter.h"

class LKDrawingGroup : public TObjArray
{
    protected:
        TCanvas *fCvs = nullptr; //!
        int fnx = 1;
        int fny = 1;

    public:
        bool ConfigureCanvas();

    public:
        LKDrawingGroup(TString name="");
        ~LKDrawingGroup() {}

        void Init();
        virtual void Draw(Option_t *option="");

        TCanvas* GetCanvas() { return fCvs; }

        void SetCanvas(TCanvas* pad) { fCvs = pad; }
        void AddDrawing(LKDrawing* drawing) { Add(drawing); }
        void AddHist(TH1 *hist) { Add(new LKDrawing(hist)); }
        void AddGraph(TGraph* graph) { Add(new LKDrawing(graph)); }

    ClassDef(LKDrawingGroup, 1)
};

#endif
