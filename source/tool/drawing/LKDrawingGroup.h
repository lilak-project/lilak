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
        TString fParentName;
        TString fFileName; //

    private:
        bool ConfigureCanvas();
        bool CheckIsGroupGroup(bool add=false);
        bool CheckIsDrawingGroup(bool add=false);

    public:
        LKDrawingGroup(TString name="", int groupLevel=0);
        LKDrawingGroup(TString fileName, TString groupSelection);
        LKDrawingGroup(TFile* file, TString groupSelection="");
        ~LKDrawingGroup() {}

        void Init();
        int DrawGroup(TString option="all");
        virtual void Draw(Option_t *option="all");
        virtual void Print(Option_t *option="") const;
        virtual Int_t Write(const char *name = nullptr, Int_t option=TObject::kSingleKey, Int_t bufsize = 0) const;
        void WriteFile(TString fileName="");

        void Save(bool recursive=true, bool saveRoot=true, bool savePNG=true, TString dirName="", TString header="");

        int GetGroupDepth() const;
        bool IsGroupGroup() const { return fIsGroupGroup; }
        bool IsDrawingGroup() const { return !fIsGroupGroup; }

        int  GetGroupLevel() { return fGroupLevel; }
        void SetGroupLevel(int level) { fGroupLevel = level; }

        TString GetFullName() const { return fParentName.IsNull()?fName:(fParentName+":"+fName); }
        TString GetParentName() const { return fParentName; }
        void SetParentName(TString name) { fParentName = name; }

        // canvas
        TCanvas* GetCanvas() { return fCvs; }
        void     SetCanvas(TCanvas* pad) { fCvs = pad; }
        int GetDivX() const { return fDivX; }
        int GetDivY() const { return fDivY; }

        // find
        LKDrawing* FindDrawing(TString name);
        TH1*       FindHist(TString name);

        // file
        TString GetFileName() { return fFileName; }
        bool AddFile(TFile* file, TString groupSelection="");
        bool AddFile(TString fileName, TString groupSelection="");

        // sub group
        void            AddGroup(LKDrawingGroup *sub);
        void            AddGroupInStructure(LKDrawingGroup *group);
        bool            CheckGroup(LKDrawingGroup *find);
        LKDrawingGroup* FindGroup(TString name="", int option=0); // option=0: exact, option=1: full name, option=2: start with 
        LKDrawingGroup* CreateGroup(TString name="", bool addToList=true);
        LKDrawingGroup* GetGroup(int i);
        int             GetNumGroups() const;
        int             GetNumAllGroups() const;

        // drawing
        LKDrawing* GetDrawing(int iDrawing);
        LKDrawing* CreateDrawing(TString name="");
        void       AddDrawing(LKDrawing* drawing);
        void       AddGraph(TGraph* graph);
        void       AddHist(TH1 *hist);
        int        GetNumDrawings() const;
        int        GetNumAllDrawings() const;
        int        GetNumAllDrawingObjects() const;

    ClassDef(LKDrawingGroup, 1)
};

#endif
