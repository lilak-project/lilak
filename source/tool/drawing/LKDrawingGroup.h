#ifndef LKDRAWINGGROUP_HH
#define LKDRAWINGGROUP_HH

#include "TObjArray.h"
#include "TGraph.h"
#include "TH1.h"
#include "TVirtualPad.h"

#include "LKDrawing.h"
#include "LKPainter.h"

class LKDrawingGroup : public TObjArray
{
    protected:
        TVirtualPad *fCvs = nullptr;
        int fnx = 1;
        int fny = 1;

        bool ConfigureCanvas();

    public:
        LKDrawingGroup(TString name="");
        ~LKDrawingGroup() {}

        virtual void Draw(Option_t *option="");

        void SetCanvas(TVirtualPad* pad) { fCvs = pad; }
        void AddHist(TH1 *hist) { Add(new LKDrawing(hist)); }
        void AddGraph(TGraph* graph) { Add(new LKDrawing(graph)); }

    ClassDef(LKDrawingGroup, 1)
};

#endif
