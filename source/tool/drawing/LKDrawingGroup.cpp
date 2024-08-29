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
    auto numDrawings = GetEntries();
    if (numDrawings>0)
    {
        ConfigureCanvas();

        if (numDrawings==1)
        {
            auto drawing = (LKDrawing*) At(0);
            drawing -> SetCanvas(fCvs);
            drawing -> Draw(option);
        }
        else
        {
            for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
            {
                auto drawing = (LKDrawing*) At(iDrawing);
                drawing -> SetCanvas(fCvs->cd(iDrawing+1));
                drawing -> Draw(option);
            }
        }
    }

    if (TString(option)=="all")
        DrawSubGroups(option);
}

void LKDrawingGroup::DrawSubGroups(Option_t *option)
{
    if (fSubGroupArray!=nullptr)
    {
        auto numGroups = fSubGroupArray->GetEntries();
        for (auto iGroup=0; iGroup<numGroups; ++iGroup)
        {
            auto sub = (LKDrawingGroup*) fSubGroupArray->At(iGroup);
            sub -> Draw();
        }
    }
}

void LKDrawingGroup::Print(Option_t *option) const
{
    auto numSubGroups = 0;
    if (fSubGroupArray!=nullptr)
        numSubGroups = fSubGroupArray->GetEntries();

    if (numSubGroups>0) {
        for (auto iSub=0; iSub<numSubGroups; ++iSub) {
            auto sub = (LKDrawingGroup*) fSubGroupArray->At(iSub);
            sub -> Print(option);
        }
    }
    else {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            drawing -> Print(option);
        }
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
        fCvs = LKPainter::GetPainter() -> CanvasResize(Form("c%s",fName.Data()), 125*fnx, 100*fny);

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

int LKDrawingGroup::GetNumSubGroups()
{
    if (fSubGroupArray==nullptr)
        return 0;
    return fSubGroupArray -> GetEntries();
}

TObjArray* LKDrawingGroup::GetSubGroupArray()
{
    if (fSubGroupArray==nullptr)
        fSubGroupArray = new TObjArray();
    return fSubGroupArray;
}

LKDrawingGroup* LKDrawingGroup::GetSubGroup(int ii)
{
    GetSubGroupArray();
    //lk_debug << fSubGroupArray->GetEntries() << endl;
    if (ii<0 || ii>=fSubGroupArray->GetEntries()) {
        //e_error << ii << endl;
        return (LKDrawingGroup*) nullptr;
    }
    auto subGroup = (LKDrawingGroup*) fSubGroupArray->At(ii);
    //lk_debug << subGroup << endl;
    return subGroup;
}

LKDrawingGroup* LKDrawingGroup::GetOrCreateSubGroup(int ii)
{
    LKDrawingGroup *subGroup = GetSubGroup(ii);
    //lk_debug << ii << " " <<subGroup << endl;
    if (subGroup==nullptr) {
        subGroup = new LKDrawingGroup(Form("%s_%d",GetName(),int(fSubGroupArray->GetEntriesFast())));
        fSubGroupArray->AddAt(subGroup,ii);
    }
    //lk_debug << fSubGroupArray -> GetEntries() << endl;
    return subGroup;
}

LKDrawingGroup* LKDrawingGroup::CreateSubGroup(TString name)
{
    auto sub = new LKDrawingGroup(name);
    AddSubGroup(sub);
    return sub;
}

LKDrawing* LKDrawingGroup::CreateDrawing(TString name)
{
    auto drawing = new LKDrawing(name);
    AddDrawing(drawing);
    return drawing;
}

LKDrawing* LKDrawingGroup::FindDrawing(TString name, TString option)
{
    auto numSubGroups = 0;
    if (fSubGroupArray!=nullptr)
        numSubGroups = fSubGroupArray->GetEntries();
    //lk_debug << numSubGroups << endl;
    if (numSubGroups>0) {
        for (auto iSub=0; iSub<numSubGroups; ++iSub) {
            auto sub = (LKDrawingGroup*) fSubGroupArray->At(iSub);
            //lk_debug << "sub-group "  << iSub << " " << sub << endl;
            auto drawing = sub -> FindDrawing(name, option);
            if (drawing!=nullptr)
                return drawing;
        }
    }
    else {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            //lk_debug << "drawing "  << iDrawing << " " << drawing << " " << drawing->GetName()  << endl;
            if (option=="hist") {
                auto hist = drawing -> GetMainHist();
                //lk_debug << hist << endl;
                if (hist!=nullptr) {
                    //lk_debug << hist->GetName() << endl;
                    if (hist->GetName()==name)
                        return drawing;
                }
            }
            else {
                if (drawing->GetName()==name)
                    return drawing;
            }
        }
    }
    return (LKDrawing*) nullptr;
}

TH1* LKDrawingGroup::FindHist(TString name)
{
    auto drawing = FindDrawing(name, "hist");
    if (drawing==nullptr)
        return (TH1*) nullptr;
    auto hist = drawing -> GetMainHist();
    if (hist==nullptr)
        return (TH1*) nullptr;
    return hist;
}
