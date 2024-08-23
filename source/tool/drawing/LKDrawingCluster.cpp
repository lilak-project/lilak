#include "LKDrawingCluster.h"
#include "LKDrawingGroup.h"

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
