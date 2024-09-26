#include "LKDrawingGroup.h"
#include "LKDataViewer.h"
#include "LKMisc.h"
#include "TKey.h"
#include "TObjString.h"
#include "TSystem.h"

ClassImp(LKDrawingGroup)

LKDrawingGroup::LKDrawingGroup(TString name, int groupLevel)
: TObjArray()
{
    Init();
    if (name.EndsWith(".root"))
        if (AddFile(name))
            return;
    SetName(name);
}

LKDrawingGroup::LKDrawingGroup(TString fileName, TString groupSelection)
: TObjArray()
{
    Init();
    AddFile(fileName,groupSelection);
}

LKDrawingGroup::LKDrawingGroup(TFile* file, TString groupSelection)
: TObjArray()
{
    Init();
    AddFile(file,groupSelection);
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
    if (ops.Index("v")>=0) {
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

void LKDrawingGroup::WriteFile(TString fileName)
{
    if (fileName.IsNull())
        fileName = Form("data_drawing/%s.root",fName.Data());
    e_info << "Writting " << fileName << endl;
    auto file = new TFile(fileName,"recreate");
    Write();
}

Int_t LKDrawingGroup::Write(const char *name, Int_t option, Int_t bsize) const
{
    int numWrite = 0;
    auto depth = GetGroupDepth();
    if (depth>=3) {
        auto numSub = GetNumGroups();
        e_info << "Writting " << numSub << " groups in " << fName << endl;
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            numWrite += sub -> Write("", option);
        }
        return numWrite;
    }
    else if (GetNumAllDrawingObjects()>1280)
    {
        auto numSub = GetNumGroups();
        e_info << "Writting " << numSub << " groups in " << fName << " (" << GetNumAllDrawingObjects() << ")" << endl;
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            numWrite += sub -> Write("", option);
        }
        return numWrite;
    }

    TString wName = name;
    if (wName.IsNull()) wName = fName;
    if (wName.IsNull()) wName = "top";
    TCollection::Write(wName, option, bsize);
    return 1;
}

void LKDrawingGroup::Save(bool recursive, bool saveRoot, bool savePNG, TString dirName, TString header)
{
    if (dirName.IsNull()) {
        dirName = "data_drawing";
        gSystem -> Exec(Form("mkdir -p %s/",dirName.Data()));
    }

    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        if (saveRoot) {
            TString fileName = Form("%s/drawings_%s.root",dirName.Data(),fName.Data());
            auto file = new TFile(fileName,"recreate");
            Write();
        }

        //lk_debug << fName << " " << GetGroupDepth() << endl;
        if (GetGroupDepth()>=3) {
            dirName = dirName + "/" + fName;
            gSystem -> Exec(Form("mkdir -p %s/",dirName.Data()));
        }
        else {
            if (header.IsNull()) header = fName;
            else header = header + "_" + fName;
        }
        //lk_debug << dirName << " " << header << endl;
        if (savePNG) {
            for (auto iSub=0; iSub<numSub; ++iSub) {
                auto sub = (LKDrawingGroup*) At(iSub);
                sub -> Save(recursive, false, savePNG, dirName, header);
            }
        }
    }
    else
    {
        //auto numDrawings = GetEntries();
        //e_cout << header << "Group " << fName << " containing " << numDrawings << " drawings" << endl;
        if (header.IsNull()==false) header = header + "_";
        TString name = dirName + "/" + header + fName + ".png";
        //lk_debug << name << endl;
        fCvs -> SaveAs(name);
    }
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

bool LKDrawingGroup::AddFile(TFile* file, TString groupSelection)
{
    if (fFileName.IsNull())
        fFileName = file -> GetName();
    else
        fFileName = fFileName + ", " + file->GetName();
    SetName(fFileName);

    if (!groupSelection.IsNull())
    {
        vector<TString> groupNameArray;
        if (groupSelection.Index(":")<0)
            groupNameArray.push_back(groupSelection);
        else {
            TObjArray *tokens = groupSelection.Tokenize(":");
            auto n = tokens -> GetEntries();
            for (auto i=0; i<n; ++i) {
                TString token0 = ((TObjString*)tokens->At(i))->GetString();
                groupNameArray.push_back(token0);
            }
        }
        for (auto groupName : groupNameArray)
        {
            if (groupName.EndsWith("*"))
            {
                groupName.Remove(groupName.Sizeof()-2);
                auto listOfKeys = file -> GetListOfKeys();
                TKey* key = nullptr;
                TIter nextKey(listOfKeys);
                while ((key=(TKey*)nextKey())) {
                    if (TString(key->GetName()).Index(groupName)==0) {
                        auto group = (LKDrawingGroup*) key -> ReadObj();
                        e_info << "Adding " << group->GetName() << " from " << file->GetName() << endl;
                        AddGroupInStructure(group);
                    }
                }
            }
            else{
                auto obj = file -> Get(groupName);
                if (obj==nullptr) {
                    e_error << groupName << " is nullptr" << endl;
                    return false;
                }
                if (TString(obj->ClassName())=="LKDrawingGroup") {
                    auto group = (LKDrawingGroup*) obj;
                    e_info << "Adding " << group->GetName() << " from " << file->GetName() << endl;
                    AddGroupInStructure(group);
                }
                else {
                    e_error << groupName << " type is not LKDrawingGroup" << endl;
                    return false;
                }
            }
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
                AddGroupInStructure(group);
            }
        }
    }
    return true;
}

bool LKDrawingGroup::AddFile(TString fileName, TString groupSelection)
{
    TFile* file = new TFile(fileName);
    if (!file->IsOpen()) {
        e_error << fileName << " is cannot be openned" << endl;
        return false;
    }
    return AddFile(file,groupSelection);
}

int LKDrawingGroup::GetNumGroups() const
{
    if (fIsGroupGroup)
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

int LKDrawingGroup::GetNumDrawings() const
{
    if (!fIsGroupGroup)
        return GetEntries();
    return 0;
}

int LKDrawingGroup::GetNumAllGroups() const
{
    int numDrawings = 0;
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            numDrawings += sub -> GetNumAllGroups();
        }
    }
    else
        numDrawings = 1;

    return numDrawings;
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

int LKDrawingGroup::GetNumAllDrawingObjects() const
{
    int numObjects = 0;
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            numObjects += sub -> GetNumAllDrawingObjects();
        }
    }
    else {
        auto numDrawing = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawing; ++iDrawing) {
            auto drawing = (LKDrawingGroup*) At(iDrawing);
            numObjects += drawing -> GetEntries();
        }
    }

    return numObjects;
}

void LKDrawingGroup::AddGroupInStructure(LKDrawingGroup *group)
{
    auto parentLevel = group -> GetGroupLevel() - 1;
    auto parentName = group -> GetParentName();
    if (parentLevel==0)
    {
        AddGroup(group);
    }
    else
    {
        vector<TString> parentNameArray;
        parentName = Form(" %s ",parentName.Data());
        TObjArray *tokens = parentName.Tokenize(":");
        auto n = tokens -> GetEntries();
        for (auto i=0; i<n; ++i) {
            TString token0 = ((TObjString*)tokens->At(i))->GetString();
            token0.ReplaceAll(" ","");
            parentNameArray.push_back(token0);
        }
        if (parentLevel==1) {
            TString pname = parentNameArray.at(1);
            LKDrawingGroup* pGroup = FindGroup(pname);
            if (pGroup==nullptr)
                pGroup = CreateGroup(pname);
            pGroup -> AddGroup(group);
        }
    }
}

void LKDrawingGroup::AddGroup(LKDrawingGroup *group)
{
    if (CheckIsGroupGroup(true)) {
        group -> SetGroupLevel(fGroupLevel+1);
        TString fullName = GetFullName();
        group -> SetParentName(fullName);
        Add(group);
    }
}

LKDrawingGroup* LKDrawingGroup::CreateGroup(TString name, bool addToList)
{
    if (CheckIsGroupGroup(true)) {
        if (name.IsNull()) name = Form("%s_%d",fName.Data(),GetEntries());
        auto sub = new LKDrawingGroup(name,fGroupLevel+1);
        if (addToList) AddGroup(sub);
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

LKDrawingGroup* LKDrawingGroup::FindGroup(TString name, int option)
{
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            TString groupSelection = sub -> GetName();
            if (option==2)
            {
                if (name.Index(groupSelection)>=0)
                    return sub;
            }
            else {
                if (option==1)
                    groupSelection = sub -> GetFullName();
                if (groupSelection==name)
                    return sub;
            }
            auto found = sub -> FindGroup(name, option);
            if (found!=nullptr)
                return found;
        }
    }
    return ((LKDrawingGroup *) nullptr);
}

bool LKDrawingGroup::CheckGroup(LKDrawingGroup *find)
{
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            if (sub==find)
                return true;
            auto found = sub -> CheckGroup(find);
            if (found)
                return true;
        }
    }
    return false;
}

LKDrawing* LKDrawingGroup::FindDrawing(TString name)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            LKDrawing* drawing = sub -> FindDrawing(name);
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
            if (drawing->GetName()==name)
                return drawing;
        }
    }
    return (LKDrawing*) nullptr;
}

TH1* LKDrawingGroup::FindHist(TString name)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            TH1* hist = sub -> FindHist(name);
            if (hist!=nullptr)
                return hist;
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            auto numObjs = drawing -> GetEntries();
            for (auto iObj=0; iObj<numObjs; ++iObj)
            {
                auto obj = drawing -> At(iObj);
                if (obj->InheritsFrom(TH1::Class()))
                {
                    if (obj->GetName()==name) {
                        return (TH1*) obj;
                    }
                }
            }
        }
    }
    return (TH1*) nullptr;
}
