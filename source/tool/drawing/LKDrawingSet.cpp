#include "LKDrawingSet.h"

ClassImp(LKDrawingSet)

LKDrawingSet::LKDrawingSet()
: TObjArray()
{
    SetName("DrawingSet");
}

void LKDrawingSet::Draw(Option_t *option)
{
    auto numDrawings = GetEntries();
    for (auto iGroup=0; iGroup<numDrawings; ++iGroup) {
        auto group = (LKDrawingGroup*) At(iGroup);
        group -> Draw(option);
    }
}

void LKDrawingSet::AddSet(LKDrawingSet* set)
{
    auto numDrawings = set -> GetEntries();
    for (auto iGroup=0; iGroup<numDrawings; ++iGroup) {
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
