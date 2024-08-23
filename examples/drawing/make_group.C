#include "make_drawing.C"

LKDrawingGroup* make_group(bool draw=true, int groupNo=0)
{
    auto group = new LKDrawingGroup(Form("Group%d",groupNo));
    group -> Add(make_drawing(false,groupNo*10+0));
    group -> Add(make_drawing(false,groupNo*10+1));
    group -> Add(make_drawing(false,groupNo*10+2));
    group -> Add(make_drawing(false,groupNo*10+3));

    if (draw) group -> Draw();
    
    return group;
}
