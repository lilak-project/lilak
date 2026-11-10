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
#include <vector>
using namespace std;

class LKDataViewer;

class LKDrawingGroup : public TObjArray
{
    public:
        enum class NodeType : UChar_t { Drawings = 0, Groups = 1 };

    protected:
        TObjArray fEntries;
        TCanvas *fCvs = nullptr; //!
        int fDivX = 0;
        int fDivY = 0;
        int fGroupLevel = 0;
        NodeType fNodeType = NodeType::Drawings;
        bool fIsGroupGroup = false; ///< legacy schema-v2 read buffer; unused in v3
        TString fParentName;
        TString fFileName; //
        TString fGlobalOption;

        bool fFixCvsSize = false;
        bool fUserCvsSize = false; //! true once the user set the canvas size/ratio explicitly (transient: adding a persistent member here breaks reading existing v3 files)
        int fDXCvs = 0;
        int fDYCvs = 0;
        TObjArray* fPadArray = nullptr; //!

        LKDataViewer* fViewer = nullptr; //!
        LKParameterContainer *fPar = nullptr; //!

    private:
        bool ConfigureCanvas();
        bool CheckIsGroupGroup(bool add=false);
        bool CheckIsDrawingGroup(bool add=false);

        LKDrawingGroup* CloneStructureOnly() const;
        void CopyGroupMetadata(const LKDrawingGroup* source);
        void AddEntry(TObject* object) { fEntries.Add(object); }
        void ClearEntries() { fEntries.Clear(); }
        void UpdateHierarchyMetadata(int level, TString parentName);
        LKDrawingGroup* FindGroupByFullName(TString fullName);
        void MergeChunk(LKDrawingGroup* chunk);
        Int_t WriteChunked(Long64_t targetSize) const;
        bool AddChunkedFile(TFile* file, TString groupSelection="");

        using TObjArray::AddAt;
        using TObjArray::AddAtAndExpand;
        using TObjArray::AddFirst;
        using TObjArray::AddLast;
        using TObjArray::AddAtFree;
        using TObjArray::AddAfter;
        using TObjArray::AddBefore;
        using TObjArray::RemoveAt;
        using TObjArray::Remove;
        using TObjArray::RemoveRange;
        using TObjArray::RecursiveRemove;
        using TObjArray::Compress;
        using TObjArray::Delete;
        using TObjArray::Expand;
        using TObjArray::SetLast;
        using TObjArray::Randomize;
        using TObjArray::Sort;
        using TObjArray::operator[];

        void DividePad(TPad* cvs, Int_t nx, Int_t ny, Float_t xmargin=0.001, Float_t ymargin=0.001, Int_t color=0);

    public:
        LKDrawingGroup(TString name="", int groupLevel=0);
        LKDrawingGroup(TString fileName, TString groupSelection);
        LKDrawingGroup(TObject* obj, TString drawStyle="");
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
        void Draw(Option_t *option="") override;
        void Print(Option_t *option="") const override;
        /// Use name="chunked:chunk_mb=512" to preserve the complete group
        /// structure while splitting large payloads into adaptive ROOT keys.
        Int_t Write(const char *name = nullptr, Int_t option=TObject::kSingleKey, Int_t bufsize = 0) const override;
        void Browse(TBrowser *b) override;
        void Clear(Option_t *option="") override;
        void WriteFile(TString fileName="", TString option="");
        void WriteFitParameterFile(TString tag="");

        LKDataViewer *GetViewer() { return fViewer; }

        void Update(TString option="");

        LKDataViewer* CreateViewer();

        void Save(bool recursive=true, bool saveRoot=true, bool saveImage=true, TString dirName="", TString header="", TString tag="");

        void Add(TObject *obj) override;

        int HAdd(TString fileName);
        int HAdd(LKDrawingGroup* group);
        vector<TString> MakeHistNameArray();

        int GetGroupDepth() const;
        bool IsGroupGroup() const { return fNodeType==NodeType::Groups; }
        bool IsDrawingGroup() const { return fNodeType==NodeType::Drawings; }
        NodeType GetNodeType() const { return fNodeType; }

        int GetEntries() const override { return fEntries.GetEntries(); }
        int GetEntriesFast() const { return fEntries.GetEntriesFast(); }
        TObject* At(int index) const override { return fEntries.At(index); }

        int  GetGroupLevel() { return fGroupLevel; }
        void SetGroupLevel(int level) { fGroupLevel = level; }

        TString GetFullName() const;
        TString GetParentName() const { return fParentName; }
        void SetParentName(TString name) { fParentName = name; }

        // canvas
        TCanvas* GetCanvas() { return fCvs; }
        void DetachCanvas() { fCvs = nullptr; }
        void SetCanvas(TVirtualPad* cvs) { fCvs = (TCanvas*) cvs; }
        void SetCanvas(TPad* cvs) { fCvs = (TCanvas*) cvs; }
        void SetCanvas(TCanvas* pad) { fCvs = pad; }
        void SetCanvasDivision(int divX, int divY) { fDivX = divX; fDivY = divY; }
        int GetDivX() const { return fDivX; }
        int GetDivY() const { return fDivY; }
        /// Set the canvas size in pixels.  With resize>0, (dx,dy) is used as an
        /// aspect ratio instead and the canvas is fitted to the display by that factor.
        void SetCanvasSize(int dx, int dy, double resize=-1) { fDXCvs = dx; fDYCvs = dy; fUserCvsSize = true; if (resize>0) { fFixCvsSize = false; AddOption("cvs_r",resize); } else fFixCvsSize = true; }
        /// (dx,dy) is an aspect ratio only; the canvas is still fitted to the display.
        void SetCanvasSizeRatio(int dx, int dy) { fDXCvs = dx; fDYCvs = dy; fUserCvsSize = true; fFixCvsSize = false; }
        void AddPad(TPad *pad) { if (fPadArray==nullptr) fPadArray = new TObjArray(); fPadArray -> Add(pad); }
        void SetPadVerticalNumbering(bool v=true) { AddOption("vertical_pad_numbering"); }

        // find
        LKDrawing* FindDrawing(TString name);
        TH1*       FindHist(TString name);
        TGraph*    FindGraph(TString name);
        TF1*       FindFunction(TString name);
        TObject*   FindClassObject(TString name, TClass *tclass);
        TObject*   FindObject(const char* name) const override;
        TObject*   FindObject(const TObject* object) const override;

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
        void       AddDrawing(TObjArray* array);
        void       AddGraph(TGraph* graph, TString drawOption="", TString title="");
        void       AddHist(TH1 *hist, TString drawOption="", TString title="");
        int        GetNumDrawings() const;
        int        GetNumAllDrawings() const;
        int        GetNumAllDrawingObjects() const;

        void ApplyStyle(TString drawStyle);
        void SetStyle(TString drawStyle);

        void SetDraw(bool draw);

        ////////////////////////////////////////////////////////////////////////////////////
        TString GetGlobalOption() const { return fGlobalOption; }
        void SetGlobalOption(TString option) { fGlobalOption = option; }
        void AppenGlobalOption(TString option) { fGlobalOption = fGlobalOption + ":" + option; }
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


    ClassDefOverride(LKDrawingGroup, 3)
};

#endif
