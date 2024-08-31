#ifndef LKDRAWINGGROUP_HH
#define LKDRAWINGGROUP_HH

#include "TObjArray.h"
#include "TGraph.h"
#include "TH1.h"
#include "TCanvas.h"
#include "TFile.h"

#include "LKDrawing.h"
#include "LKPainter.h"

class LKDrawingGroup : public TObjArray
{
    protected:
        TCanvas *fCvs = nullptr; //!
        int fDivX = 1;
        int fDivY = 1;
        int fGroupLevel = 0;
        bool fIsGroupGroup = false;
        TString fFileName; //

    private:
        bool ConfigureCanvas();
        bool CheckIsGroupGroup(bool add=false);
        bool CheckIsDrawingGroup(bool add=false);

    public:
        LKDrawingGroup(TString name="", int groupLevel=0);
        LKDrawingGroup(TString fileName, TString groupName);
        ~LKDrawingGroup() {}

        void Init();
        virtual void Draw(Option_t *option="all");
        virtual void Print(Option_t *option="") const;
        virtual Int_t Write(const char *name = nullptr, Int_t option=TObject::kSingleKey, Int_t bufsize = 0) const;

        int GetGroupDepth() const;
        bool IsGroupGroup() const { return fIsGroupGroup; }
        bool IsDrawingGroup() const { return !fIsGroupGroup; }

        int  GetGroupLevel() { return fGroupLevel; }
        void SetGroupLevel(int level) { fGroupLevel = level; }

        // canvas
        TCanvas* GetCanvas() { return fCvs; }
        void     SetCanvas(TCanvas* pad) { fCvs = pad; }
        int GetDivX() const { return fDivX; }
        int GetDivY() const { return fDivY; }

        // find
        LKDrawing* FindDrawing(TString name, TString option="");
        TH1*       FindHist(TString name);

        // file
        TString GetFileName() { return fFileName; }
        bool AddFile(TFile* file, TString groupName="");
        bool AddFile(TString fileName, TString groupName="");

        // sub group
        void            AddGroup(LKDrawingGroup *sub);
        bool            FindGroup(LKDrawingGroup *find);
        LKDrawingGroup* CreateGroup(TString name="");
        LKDrawingGroup* GetGroup(int i);
        int             GetNumGroups();
        int             GetNumAllDrawings() const;

        // drawing
        LKDrawing* GetDrawing(int iDrawing);
        LKDrawing* CreateDrawing(TString name="");
        void       AddDrawing(LKDrawing* drawing);
        void       AddGraph(TGraph* graph);
        void       AddHist(TH1 *hist);
        int        GetNumDrawings();

    ClassDef(LKDrawingGroup, 1)
};

#endif
