#include "LKDrawingSet.h"
#include "LKDataViewer.h"

ClassImp(LKDrawingSet)

LKDrawingSet::LKDrawingSet()
: TObjArray()
{
    SetName("DrawingSet");
}

void LKDrawingSet::Draw(Option_t *option)
{
    TString ops(option);
    ops.ToLower();
    if (ops.Index("viewer")>=0) {
        (new LKDataViewer(this))->Draw();
        return;
    }

    auto numGroups = GetEntries();
    for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
        auto group = (LKDrawingGroup*) At(iGroup);
        group -> Draw(option);
    }
}

void LKDrawingSet::Print(Option_t *option) const
{
    auto numGroups = GetEntries();
    for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
        auto group = (LKDrawingGroup*) At(iGroup);
        group -> Print(option);
    }
}

void LKDrawingSet::AddSet(LKDrawingSet* set)
{
    auto numGroups = set -> GetEntries();
    for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
        auto group = (LKDrawingGroup*) set -> At(iGroup);
        AddGroup(group);
    }
}

void LKDrawingSet::AddDrawing(LKDrawing* drawing)
{
    auto group = new LKDrawingGroup(Form("group_%s",drawing->GetName()));
    group -> AddDrawing(drawing);
    AddGroup(group);
}

void LKDrawingSet::AddGroup(LKDrawingGroup* group)
{
    Add(group);
}

LKDrawingGroup* LKDrawingSet::CreateGroup(TString name)
{
    auto group = new LKDrawingGroup(name);
    AddGroup(group);
    return group;
}

Int_t LKDrawingSet::Write(const char *name, Int_t option, Int_t bsize) const
{
    return TCollection::Write(name, option, bsize);
}

LKDrawing* LKDrawingSet::FindDrawing(TString name, TString option)
{
    auto numGroups = GetEntries();
    for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
        auto group = (LKDrawingGroup*) At(iGroup);
        //lk_debug << "group "  << iGroup << " " << group << endl;
        auto drawing = group -> FindDrawing(name,option);
        //lk_debug << drawing << endl;
        if (drawing!=nullptr)
            return drawing;
    }
    return (LKDrawing*) nullptr;
}

TH1* LKDrawingSet::FindHist(TString name)
{
    auto drawing = FindDrawing(name, "hist");
    if (drawing==nullptr)
        return (TH1*) nullptr;
    auto hist = drawing -> GetMainHist();
    if (hist==nullptr)
        return (TH1*) nullptr;
    return hist;
}
