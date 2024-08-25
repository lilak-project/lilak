#include "LKDrawingCluster.h"

ClassImp(LKDrawingCluster)

LKDrawingCluster::LKDrawingCluster()
: TObjArray()
{
}

void LKDrawingCluster::Draw(Option_t *option)
{
    auto numDrawings = GetEntries();
    for (auto iGroup=0; iGroup<numDrawings; ++iGroup) {
        auto group = (LKDrawingGroup*) At(iGroup);
        group -> Draw();
    }
}

void LKDrawingCluster::AddCluster(LKDrawingCluster* cluster)
{
    auto numDrawings = cluster -> GetEntries();
    for (auto iGroup=0; iGroup<numDrawings; ++iGroup) {
        auto group = (LKDrawingGroup*) cluster -> At(iGroup);
        AddGroup(group);
    }
}

void LKDrawingCluster::AddDrawing(LKDrawing* drawing)
{
    auto group = new LKDrawingGroup(Form("group_%s",drawing->GetName()));
    group -> AddDrawing(drawing);
    AddGroup(group);
}

void LKDrawingCluster::AddGroup(LKDrawingGroup* group)
{
    Add(group);
}
