#ifndef LKDRAWINGGROUP_HH
#define LKDRAWINGGROUP_HH

#include "TObjArray.h"
#include "TGraph.h"
#include "TH1.h"
#include "TCanvas.h"
#include "TFile.h"

#include "LKDrawing.h"
#include "LKPainter.h"
#include "LKParameterContainer.h"

class LKDataViewer;

class LKDrawingGroup : public TObjArray
{
    protected:
        TCanvas *fCvs = nullptr; //!
        int fDivX = 0;
        int fDivY = 0;
        int fGroupLevel = 0;
        bool fIsGroupGroup = false;
        TString fParentName;
        TString fFileName; //
        TString fGlobalOption;

        bool fFixCvsSize = false;
        int fDXCvs = 0;
        int fDYCvs = 0;
        TObjArray* fPadArray = nullptr;

        LKDataViewer* fViewer = nullptr; //!
        LKParameterContainer *fPar = nullptr; //!

    private:
        bool ConfigureCanvas();
        bool CheckIsGroupGroup(bool add=false);
        bool CheckIsDrawingGroup(bool add=false);

        void DividePad(TPad* cvs, Int_t nx, Int_t ny, Float_t xmargin=0.001, Float_t ymargin=0.001, Int_t color=0);

    public:
        LKDrawingGroup(TString name="", int groupLevel=0);
        LKDrawingGroup(TString fileName, TString groupSelection);
        LKDrawingGroup(TFile* file, TString groupSelection="");
        ~LKDrawingGroup() {}

        void Init();
        int DrawGroup(TString option="all");
        /// v : open data viewer
        /// h : Load all UIs
        /// l : Load all canvas from the start
        /// s : Save all canvas from the start
        /// wx=900 : set widow size x
        /// wy=800 : set widow size x
        /// resize=1 : resize by factor
        virtual void Draw(Option_t *option="all");
        virtual void Print(Option_t *option="") const;
        virtual Int_t Write(const char *name = nullptr, Int_t option=TObject::kSingleKey, Int_t bufsize = 0) const;
        void WriteFile(TString fileName="");

        LKDataViewer* CreateViewer();

        void Save(bool recursive=true, bool saveRoot=true, bool saveImage=true, TString dirName="", TString header="", TString tag="");

        virtual void Add(TObject *obj);

        int GetGroupDepth() const;
        bool IsGroupGroup() const { return fIsGroupGroup; }
        bool IsDrawingGroup() const { return !fIsGroupGroup; }

        int  GetGroupLevel() { return fGroupLevel; }
        void SetGroupLevel(int level) { fGroupLevel = level; }

        TString GetFullName() const;
        TString GetParentName() const { return fParentName; }
        void SetParentName(TString name) { fParentName = name; }

        // canvas
        TCanvas* GetCanvas() { return fCvs; }
        void SetCanvas(TCanvas* pad) { fCvs = pad; }
        void SetCanvasDivision(int divX, int divY) { fDivX = divX; fDivY = divY; }
        int GetDivX() const { return fDivX; }
        int GetDivY() const { return fDivY; }
        void SetCanvasSize(int dx, int dy) { fFixCvsSize = true; fDXCvs = dx; fDYCvs = dy; }
        void SetCanvasSizeRatio(int dx, int dy) { fDXCvs = dx; fDYCvs = dy; }
        void AddPad(TPad *pad) { if (fPadArray==nullptr) fPadArray = new TObjArray(); fPadArray -> Add(pad); }
        void SetPadVerticalNumbering(bool v=true) { AddOption("vertical_pad_numbering"); }

        // find
        LKDrawing* FindDrawing(TString name);
        TH1*       FindHist(TString name);
        TGraph*    FindGraph(TString name);

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
        LKDrawing* CreateDrawing(TString name="", bool addToList=true);
        void       AddDrawing(LKDrawing* drawing);
        void       AddGraph(TGraph* graph, TString drawOption="", TString title="");
        void       AddHist(TH1 *hist, TString drawOption="", TString title="");
        int        GetNumDrawings() const;
        int        GetNumAllDrawings() const;
        int        GetNumAllDrawingObjects() const;

        ////////////////////////////////////////////////////////////////////////////////////
        TString GetGlobalOption() const { return fGlobalOption; }
        void SetGlobalOption(TString option) { fGlobalOption = option; }
        bool CheckOption(TString option) { return LKMisc::CheckOption(fGlobalOption,option); }
        int FindOptionInt(TString option, int value) { return LKMisc::FindOptionInt(fGlobalOption,option,value); }
        double FindOptionDouble(TString option, double value) { return LKMisc::FindOptionDouble(fGlobalOption,option,value); }
        TString FindOptionString(TString &option, TString value) { return LKMisc::FindOptionString(fGlobalOption,option,value); }

        ////////////////////////////////////////////////////////////////////////////////////
        void RemoveOption(TString option) { LKMisc::RemoveOption(fGlobalOption,option); }
        /// - wide_canvas
        /// - vertical_canvas
        void AddOption(TString option) { LKMisc::AddOption(fGlobalOption,option); }
        void AddOption(TString option, double value) { LKMisc::AddOption(fGlobalOption,option,value); }


    ClassDef(LKDrawingGroup, 1)
};

#endif
