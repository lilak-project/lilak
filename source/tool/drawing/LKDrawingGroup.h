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
        TObjArray *fSubGroupArray = nullptr;

    private:
        int fID = -1;

    public:
        bool ConfigureCanvas();

    public:
        LKDrawingGroup(TString name="");
        ~LKDrawingGroup() {}

        void Init();
        virtual void Draw(Option_t *option="all");
        void DrawSubGroups(Option_t *option="");
        virtual void Print(Option_t *option="") const;

        TCanvas* GetCanvas() { return fCvs; }

        void SetCanvas(TCanvas* pad) { fCvs = pad; }

        LKDrawingGroup* CreateSubGroup(TString name);
        LKDrawing* CreateDrawing(TString name);
        LKDrawing* FindDrawing(TString name, TString option="");
        TH1* FindHist(TString name);

        TObjArray* GetSubGroupArray();
        int GetNumSubGroups();
        void AddSubGroup(LKDrawingGroup *group) { GetSubGroupArray() -> Add(group); }
        LKDrawingGroup* GetSubGroup(int ii);
        LKDrawingGroup* GetOrCreateSubGroup(int ii);

        void AddDrawing(int ii, LKDrawing* drawing) { auto subGroup = GetOrCreateSubGroup(ii); subGroup -> Add(drawing); }
        void AddHist   (int ii, TH1 *hist)          { auto subGroup = GetOrCreateSubGroup(ii); subGroup -> Add(new LKDrawing(hist)); }
        void AddGraph  (int ii, TGraph* graph)      { auto subGroup = GetOrCreateSubGroup(ii); subGroup -> Add(new LKDrawing(graph)); }

        void AddDrawing(LKDrawing* drawing) { Add(drawing); }
        void AddHist(TH1 *hist) { Add(new LKDrawing(hist)); }
        void AddGraph(TGraph* graph) { Add(new LKDrawing(graph)); }

    ClassDef(LKDrawingGroup, 1)
};

#endif
