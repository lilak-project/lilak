#include "example_functions.h"

void draw()
{
    auto set = new LKDrawingSet();

    int iDrawing = 0;

    for (auto iGroup=0; iGroup<3; ++iGroup)
    {
        auto group = new LKDrawingGroup(Form("G%d",iGroup));
        set -> AddGroup(group);
        for (auto i=0; i<6; ++i)
            group -> AddDrawing(make_drawing(iDrawing++));
    }

    auto group = new LKDrawingGroup(Form("G%d",99));
    set -> AddGroup(group);
    for (auto sub=0; sub<2; sub++) {
        for (auto i=0; i<25; ++i) {
            group -> AddDrawing(sub,make_drawing(iDrawing++));
        }
    }

    auto group1 = new LKDrawingGroup(Form("G%d",100));
    set -> AddGroup(group1);
    for (auto sub=0; sub<3; sub++) {
        for (auto i=0; i<15; ++i) {
            group1 -> AddDrawing(sub,make_drawing(iDrawing++));
        }
    }

    //set -> Draw("all");

    auto viewer = new LKDataViewer();
    viewer -> AddSet(set);
    viewer -> Draw();
}
