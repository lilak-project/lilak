#include "LKDrawingGroup.h"
#include "LKDataViewer.h"
#include "LKMisc.h"
#include "TKey.h"

ClassImp(LKDrawingGroup)

LKDrawingGroup::LKDrawingGroup(TString name, int groupLevel)
: TObjArray()
{
    Init();
    if (name.EndsWith(".root"))
        AddFile(name);
    SetName(name);
}

LKDrawingGroup::LKDrawingGroup(TString fileName, TString groupName)
: TObjArray()
{
    Init();
    AddFile(fileName,groupName);
    SetName(fileName);
}

void LKDrawingGroup::Init()
{
    fName = "drawings";
    fCvs = nullptr;
    fDivX = 1;
    fDivY = 1;
    fGroupLevel = 0;
}

void LKDrawingGroup::Draw(Option_t *option)
{
    TString ops(option);
    ops.ToLower();
    if (ops.Index("viewer")>=0) {
        (new LKDataViewer(this))->Draw();
        return;
    }

    if (CheckIsGroupGroup() && TString(option)=="all")
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            sub -> Draw(option);
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        if (numDrawings>0)
        {
            ConfigureCanvas();

            if (numDrawings==1) {
                auto drawing = (LKDrawing*) At(0);
                drawing -> SetCanvas(fCvs);
                drawing -> Draw(option);
            }
            else {
                for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
                {
                    auto drawing = (LKDrawing*) At(iDrawing);
                    drawing -> SetCanvas(fCvs->cd(iDrawing+1));
                    drawing -> Draw(option);
                }
            }
        }
    }
}

Int_t LKDrawingGroup::Write(const char *name, Int_t option, Int_t bsize) const
{
    auto depth = GetGroupDepth();
    if (option==TObject::kSingleKey && depth>=3) {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            lk_debug << "passing to lower level " << sub->GetName() << endl;
            sub -> Write("", option);
        }
    }
    TString wName = name;
    if (wName.IsNull()) wName = fName;
    if (wName.IsNull()) wName = "DrawingGroup";
    return TCollection::Write(wName, option, bsize);
}

void LKDrawingGroup::Print(Option_t *opt) const
{
    TString option = opt;
    bool isTop = false;
    if (LKMisc::CheckOption(option,"level")==false) {
        isTop = true;
        e_info << "LKDrawingGroup" << endl;
        option = option + ":level=0";
    }

    int tab = 0;
    if (LKMisc::CheckOption(option,"level"))
        tab = TString(LKMisc::FindOption(option,"level",false,1)).Atoi();
    TString header; for (auto i=0; i<tab; ++i) header += "  ";

    if (fIsGroupGroup)
    {

        auto numSub = GetEntries();
        TString title = header + Form("DrawingGroup[%d] %s",numSub,fName.Data());
        e_cout << title << endl;

        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            sub -> Print(option);
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        e_cout << header << "Group " << fName << " containing " << numDrawings << " drawings" << endl;
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            drawing -> Print(option);
        }
        if (LKMisc::CheckOption(option,"!drawing")==false) e_cout << endl;
    }
    if (isTop)
        e_info << GetNumAllDrawings() << " drawings in total" << endl;
}

bool LKDrawingGroup::ConfigureCanvas()
{
    auto numDrawings = GetEntries();

    if      (numDrawings== 1) { fDivX =  1; fDivY =  1; }
    else if (numDrawings<= 4) { fDivX =  2; fDivY =  2; }
    else if (numDrawings<= 6) { fDivX =  3; fDivY =  2; }
    else if (numDrawings<= 8) { fDivX =  4; fDivY =  2; }
    else if (numDrawings<= 9) { fDivX =  3; fDivY =  3; }
    else if (numDrawings<=12) { fDivX =  4; fDivY =  3; }
    else if (numDrawings<=16) { fDivX =  4; fDivY =  4; }
    else if (numDrawings<=20) { fDivX =  5; fDivY =  4; }
    else if (numDrawings<=25) { fDivX =  6; fDivY =  4; }
    else if (numDrawings<=25) { fDivX =  5; fDivY =  5; }
    else if (numDrawings<=30) { fDivX =  6; fDivY =  5; }
    else if (numDrawings<=35) { fDivX =  7; fDivY =  5; }
    else if (numDrawings<=36) { fDivX =  6; fDivY =  6; }
    else if (numDrawings<=40) { fDivX =  8; fDivY =  5; }
    else if (numDrawings<=42) { fDivX =  7; fDivY =  6; }
    else if (numDrawings<=48) { fDivX =  8; fDivY =  6; }
    else if (numDrawings<=63) { fDivX =  9; fDivY =  7; }
    else if (numDrawings<=80) { fDivX = 10; fDivY =  8; }
    else                      { fDivX = 12; fDivY = 10; }

    if (fCvs==nullptr)
        fCvs = LKPainter::GetPainter() -> CanvasResize(Form("c%s",fName.Data()), 125*fDivX, 100*fDivY);

    if (numDrawings==1)
        return true;

    TObject *obj;
    int nPads = 0;
    TIter next(fCvs -> GetListOfPrimitives());
    while ((obj=next())) {
        if (obj->InheritsFrom(TVirtualPad::Class()))
            nPads++;
    }
    if (nPads<numDrawings)
        fCvs -> Divide(fDivX, fDivY);

    return true;
}

bool LKDrawingGroup::AddFile(TFile* file, TString groupName)
{
    fFileName = file -> GetName();

    if (!groupName.IsNull())
    {
        auto obj = file -> Get(groupName);
        if (obj==nullptr) {
            e_error << groupName << " is nullptr" << endl;
            return false;
        }
        if (TString(obj->ClassName())=="LKDrawingGroup") {
            auto group = (LKDrawingGroup*) obj;
            e_info << "Adding " << group->GetName() << " from " << file->GetName() << endl;
            AddGroup(group);
        }
        else {
            e_error << groupName << " type is not LKDrawingGroup" << endl;
            return false;
        }
    }
    else
    {
        auto listOfKeys = file -> GetListOfKeys();
        TKey* key = nullptr;
        TIter nextKey(listOfKeys);
        while ((key=(TKey*)nextKey())) {
            if (TString(key->GetClassName())=="LKDrawingGroup") {
                auto group = (LKDrawingGroup*) key -> ReadObj();
                e_info << "Adding " << group->GetName() << " from " << file->GetName() << endl;
                AddGroup(group);
            }
        }
    }
    return true;
}

bool LKDrawingGroup::AddFile(TString fileName, TString groupName)
{
    TFile* file = new TFile(fileName);
    if (!file->IsOpen()) {
        e_error << fileName << " is cannot be openned" << endl;
        return false;
    }
    return AddFile(file,groupName);
}

int LKDrawingGroup::GetNumGroups()
{
    if (CheckIsGroupGroup())
        return GetEntries();
    return 0;
}

bool LKDrawingGroup::CheckIsGroupGroup(bool add)
{
    if (GetEntries()==0) {
        if (add) fIsGroupGroup = true;
        return true;
    }
    if (!fIsGroupGroup) {
        if (add) lk_error << "group is drawing-group" << endl;
        return false;
    }
    return true;
}

bool LKDrawingGroup::CheckIsDrawingGroup(bool add)
{
    if (GetEntries()==0) {
        if (add) fIsGroupGroup = false;
        return true;
    }
    if (fIsGroupGroup) {
        if (add) lk_error << "group is group-group" << endl;
        return false;
    }
    return true;
}

int LKDrawingGroup::GetGroupDepth() const
{
    if (fIsGroupGroup) {
        auto maxDepth = 0;
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            auto depth = sub -> GetGroupDepth();
            if (depth>maxDepth)
                maxDepth = depth;
        }
        return maxDepth + 1;
    }
    else
        return 1;
}

void LKDrawingGroup::AddDrawing(LKDrawing* drawing)
{
    if (CheckIsDrawingGroup(true))
        Add(drawing);
}

void LKDrawingGroup::AddGraph(TGraph* graph)
{
    if (CheckIsDrawingGroup(true))
        Add(new LKDrawing(graph));
}

void LKDrawingGroup::AddHist(TH1 *hist)
{
    if (CheckIsDrawingGroup(true))
        Add(new LKDrawing(hist));
}

int LKDrawingGroup::GetNumDrawings()
{
    if (CheckIsDrawingGroup())
        return GetEntries();
    return 0;
}

int LKDrawingGroup::GetNumAllDrawings() const
{
    int numDrawings = 0;
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            numDrawings += sub -> GetNumAllDrawings();
        }
    }
    else
        numDrawings += GetEntries();

    return numDrawings;
}

void LKDrawingGroup::AddGroup(LKDrawingGroup *group)
{
    if (CheckIsGroupGroup(true)) {
        group -> SetGroupLevel(fGroupLevel+1);
        Add(group);
    }
}

LKDrawingGroup* LKDrawingGroup::CreateGroup(TString name)
{
    if (CheckIsGroupGroup(true)) {
        if (name.IsNull()) name = Form("%s_%d",fName.Data(),GetEntries());
        auto sub = new LKDrawingGroup(name,fGroupLevel+1);
        Add(sub);
        return sub;
    }
    return (LKDrawingGroup*) nullptr;
}

LKDrawingGroup* LKDrawingGroup::GetGroup(int iSub)
{
    if (CheckIsDrawingGroup())
        return (LKDrawingGroup*) nullptr;
    if (iSub<0 || iSub>=GetEntries()) {
        return (LKDrawingGroup*) nullptr;
    }
    auto subGroup = (LKDrawingGroup*) At(iSub);
    return subGroup;
}

LKDrawing* LKDrawingGroup::GetDrawing(int iDrawing)
{
    if (CheckIsGroupGroup())
        return (LKDrawing*) nullptr;
    if (iDrawing<0 || iDrawing>=GetEntries()) {
        return (LKDrawing*) nullptr;
    }
    auto drawing = (LKDrawing*) At(iDrawing);
    return drawing;
}

LKDrawing* LKDrawingGroup::CreateDrawing(TString name)
{
    if (CheckIsDrawingGroup(true)) {
        auto drawing = new LKDrawing(name);
        Add(drawing);
        return drawing;
    }
    return (LKDrawing*) nullptr;
}

bool LKDrawingGroup::FindGroup(LKDrawingGroup *find)
{
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            auto found = sub -> FindGroup(find);
            if (found)
                return true;
            if (sub==find)
                return true;
        }
    }
    return false;
}

LKDrawing* LKDrawingGroup::FindDrawing(TString name, TString option)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            LKDrawing* drawing = sub -> FindDrawing(name, option);
            if (drawing!=nullptr)
                return drawing;
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            if (option=="hist") {
                auto hist = drawing -> GetMainHist();
                if (hist!=nullptr) {
                    if (hist->GetName()==name) {
                        return drawing;
                    }
                }
            }
            else {
                if (drawing->GetName()==name)
                    return drawing;
            }
        }
    }
    return (LKDrawing*) nullptr;
}

TH1* LKDrawingGroup::FindHist(TString name)
{
    auto drawing = FindDrawing(name, "hist");
    if (drawing==nullptr)
        return (TH1*) nullptr;
    auto hist = drawing -> GetMainHist();
    if (hist==nullptr)
        return (TH1*) nullptr;
    return hist;
}
