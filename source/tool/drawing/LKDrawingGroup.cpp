#include "LKDrawingGroup.h"

ClassImp(LKDrawingGroup)

LKDrawingGroup::LKDrawingGroup(TString name)
: TObjArray()
{
    SetName(name);
}

void LKDrawingGroup::Init()
{
    fCvs = nullptr;
    fnx = 1;
    fny = 1;
}

void LKDrawingGroup::Draw(Option_t *option)
{
    ConfigureCanvas();

    auto numDrawings = GetEntries();
    for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing) {
        auto drawing = (LKDrawing*) At(iDrawing);
        drawing -> SetCanvas(fCvs->cd(iDrawing+1));
        drawing -> Draw();
    }
}

bool LKDrawingGroup::ConfigureCanvas()
{
    auto numDrawings = GetEntries();

    if      (numDrawings== 1) { fnx =  1; fny =  1; }
    else if (numDrawings<= 4) { fnx =  2; fny =  2; }
    else if (numDrawings<= 6) { fnx =  3; fny =  2; }
    else if (numDrawings<= 8) { fnx =  4; fny =  2; }
    else if (numDrawings<= 9) { fnx =  3; fny =  3; }
    else if (numDrawings<=12) { fnx =  4; fny =  3; }
    else if (numDrawings<=16) { fnx =  4; fny =  4; }
    else if (numDrawings<=20) { fnx =  5; fny =  4; }
    else if (numDrawings<=25) { fnx =  6; fny =  4; }
    else if (numDrawings<=25) { fnx =  5; fny =  5; }
    else if (numDrawings<=30) { fnx =  6; fny =  5; }
    else if (numDrawings<=35) { fnx =  7; fny =  5; }
    else if (numDrawings<=36) { fnx =  6; fny =  6; }
    else if (numDrawings<=40) { fnx =  8; fny =  5; }
    else if (numDrawings<=42) { fnx =  7; fny =  6; }
    else if (numDrawings<=48) { fnx =  8; fny =  6; }
    else if (numDrawings<=63) { fnx =  9; fny =  7; }
    else if (numDrawings<=80) { fnx = 10; fny =  8; }
    else                      { fnx = 12; fny = 10; }

    if (fCvs==nullptr)
        fCvs = LKPainter::GetPainter() -> CanvasResize("", 125*fnx, 100*fny);

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
        fCvs -> Divide(fnx, fny);

    return true;
}
