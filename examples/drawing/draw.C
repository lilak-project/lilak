#include "example_functions.h"

void draw()
{
    auto v = new LKDataViewer();

    int iDrawing = 0;
    for (auto iGroup=0; iGroup<7; ++iGroup)
    {
        auto group = new LKDrawingGroup(Form("G%d",iGroup));
        for (auto i=0; i<6; ++i)
            group -> AddDrawing(make_drawing(iDrawing++));
        v -> AddGroup(group);
    }

    v -> Draw();
}
