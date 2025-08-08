#include "example_functions.h"

void draw()
{
    auto top = new LKDrawingGroup("top");

    int iDrawing = 0;

    for (auto iGroup=0; iGroup<3; ++iGroup)
    {
        auto group = top -> CreateGroup(Form("G%d",iGroup));
        for (auto i=0; i<6; ++i)
            group -> AddDrawing(make_drawing(iDrawing++));
    }

    auto group1 = top -> CreateGroup(Form("G%d",99));
    for (auto i=0; i<1; i++) {
        auto sub = group1 -> CreateGroup();
        for (auto j=0; j<25; ++j) {
            sub -> AddDrawing(make_drawing(iDrawing++));
        }
    }

    auto group2 = top -> CreateGroup(Form("G%d",100));
    for (auto i=0; i<3; i++) {
        auto sub = group2 -> CreateGroup();
        for (auto j=0; j<15; ++j) {
            sub -> AddDrawing(make_drawing(iDrawing++));
        }
    }

    top -> Draw("viewer");
}
