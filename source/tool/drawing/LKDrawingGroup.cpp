#include "LKDrawingGroup.h"
#include "LKDataViewer.h"
#include "LKMisc.h"
#include "TKey.h"
#include "TObjString.h"
#include "TSystem.h"

ClassImp(LKDrawingGroup)

LKDrawingGroup::LKDrawingGroup(TString name, int groupLevel)
: TObjArray()
{
    Init();
    if (name.EndsWith(".root"))
        if (AddFile(name))
            return;
    SetName(name);
}

LKDrawingGroup::LKDrawingGroup(TString fileName, TString groupSelection)
: TObjArray()
{
    Init();
    AddFile(fileName,groupSelection);
}

LKDrawingGroup::LKDrawingGroup(TFile* file, TString groupSelection)
: TObjArray()
{
    Init();
    AddFile(file,groupSelection);
}

LKDrawingGroup::LKDrawingGroup(TObject* obj, TString drawStyle)
: TObjArray()
{
    if (obj->InheritsFrom(TFile::Class())) {
        Init();
        AddFile((TFile*)obj,drawStyle);
        return;
    }

    Init();
    SetName(Form("%s",obj->GetName()));
    Add(obj);
    if (!drawStyle.IsNull())
        SetStyle(drawStyle);
}

void LKDrawingGroup::Init()
{
    fName = "DrawingGroup";
    fCvs = nullptr;
    fDivX = 0;
    fDivY = 0;
    fGroupLevel = 0;
}

LKDataViewer* LKDrawingGroup::CreateViewer()
{
    if (fViewer==nullptr)
        fViewer = new LKDataViewer(this);
    return fViewer;
}

void LKDrawingGroup::Draw(Option_t *option)
{
    TString optionString(option);
    optionString.ToLower();

    bool usingDataViewer = false;
    if (fViewer!=nullptr)
        usingDataViewer = true;
    if (usingDataViewer==false)
        usingDataViewer = LKMisc::CheckOption(optionString,"viewer # (dg) draw using LKDataViewer",true);

    if (usingDataViewer)
    {
        if (fViewer==nullptr)
            fViewer = new LKDataViewer(this);

        if (fViewer->IsActive())
            lk_warning << "viewer already running!" << endl;
        else
            fViewer -> Draw(optionString);
    }
    else
    {
        if (CheckIsGroupGroup())
        {
            auto numSub = GetEntries();
            for (auto iSub=0; iSub<numSub; ++iSub) {
                auto sub = (LKDrawingGroup*) At(iSub);
                sub -> AppenGlobalOption(fGlobalOption);
                sub -> Draw(optionString);
            }
        }
        else
        {
            auto numDrawings = GetEntries();
            if (numDrawings>0)
            {
                ConfigureCanvas();
                if (numDrawings==1) {
                    auto drawing = (LKDrawing*) At(0);
                    drawing -> SetCanvas(fCvs);
                    drawing -> Draw(optionString);
                }
                else {
                    for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
                    {
                        auto drawing = (LKDrawing*) At(iDrawing);
                        drawing -> SetCanvas(fCvs->cd(iDrawing+1));
                        drawing -> Draw(optionString);
                    }
                }
            }
            //fCvs -> SetWindowSize(800,800);
        }
    }
}

void LKDrawingGroup::Update(TString option)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            sub -> Update(option);
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            drawing -> Update(option);
        }
    }
}

void LKDrawingGroup::WriteFitParameterFile(TString tag)
{
    Save(true,true,false,"data_lilak","",Form("FITPARAMETERS%s",tag.Data()));
}

void LKDrawingGroup::WriteFile(TString fileName, TString option)
{
    if (option=="flat")
    {
        if (fileName.IsNull())
            fileName = Form("data_lilak/%s.flat.root",fName.Data());
        lk_info << "Writting" << endl;
        e_cout << "    " << fileName << endl;
        auto file = new TFile(fileName,"recreate");
        Write("flat");
    }
    else {
        if (fileName.IsNull())
            fileName = Form("data_lilak/%s.root",fName.Data());
        lk_info << "Writting" << endl;
        e_cout << "    " << fileName << endl;
        auto file = new TFile(fileName,"recreate");
        Write();
    }
}

Int_t LKDrawingGroup::Write(const char *name, Int_t option, Int_t bsize) const
{
    TString name0 = TString(name);
    bool flat = (name0=="flat");
    bool write_only_fit = LKMisc::CheckOption(name0,"FITPARAMETERS");
    int countDrawings = LKMisc::FindOptionInt(name0,"draw_count",0);
    int countDrawingsLocal = 0;

    if (flat) // flat
    {
        if (IsDrawingGroup())
        {
            TIter next(this);
            LKDrawing *draw;
            while ((draw = (LKDrawing*) next())) {
                LKMisc::AddOption(name0,"draw_count",countDrawings);
                draw -> Write(name0, 0, bsize);
                countDrawings++;
                countDrawingsLocal++;
            }
        }
        else {
            auto numSub = GetNumGroups();
            for (auto iSub=0; iSub<numSub; ++iSub) {
                auto sub = (LKDrawingGroup*) At(iSub);
                LKMisc::AddOption(name0,"draw_count",countDrawings);
                auto numDrawings = sub -> Write(name0, 0);
                countDrawings += numDrawings;
                countDrawingsLocal += numDrawings;
            }
        }
    }
    else if (GetGroupDepth()>=3 || GetNumAllDrawingObjects()>1280)
    {
        auto numSub = GetNumGroups();
        lk_info << "Writting " << numSub << " groups in " << fName << " (" << GetNumAllDrawingObjects() << ")" << endl;
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            LKMisc::AddOption(name0,"draw_count",countDrawings);
            auto numDrawings = sub -> Write(name0, option);
            countDrawings += numDrawings;
            countDrawingsLocal += numDrawings;
        }
        if (!flat && fPar!=nullptr) fPar -> Write();
    }
    else // single key
    {
        if (write_only_fit) {
            TCollection::Write(name0, 0, bsize);
        }
        else {
            TString name1 = fName;
            if (name1.IsNull()) name1 = "group";
            TCollection::Write(name1, option, bsize);
            if (!flat && fPar!=nullptr) fPar -> Write();
        }
    }

    return countDrawingsLocal;
}

void LKDrawingGroup::Browse(TBrowser *b)
{
    if (fIsGroupGroup)
        TObjArray::Browse(b);
    else {
        //TObjArray::Browse(b);
        if (gPad!=nullptr)
            SetCanvas(gPad);
        auto gPadBackup = gPad;
        gPad -> Clear();
        Draw(b ? b->GetDrawOption() : "");
        gPad = gPadBackup;
        gPad -> Update();
    }
}

void LKDrawingGroup::Save(bool recursive, bool saveRoot, bool saveImage, TString dirName, TString header, TString tag)
{
    if (dirName.IsNull()) {
        dirName = "data_lilak";
    }
    if (fName.IsNull()) {
        fName = "top";
    }
    gSystem -> Exec(Form("mkdir -p %s/",dirName.Data()));

    TString write_name = tag;
    if (tag.Index("FITPARAMETERS")==0) {
        write_name = "FITPARAMETERS";
        tag.ReplaceAll("FITPARAMETERS","fit_parameters");
    }

    TString uheader = header;
    uheader.ReplaceAll(":","_");
    uheader.ReplaceAll(" ","_");
    if (uheader.IsNull()==false) uheader = uheader + "__";
    TString fullName = fName;//GetFullName();
    fullName.ReplaceAll(":","_");
    fullName.ReplaceAll(" ","_");
    if (!tag.IsNull()) tag = Form(".%s",tag.Data());

    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        if (numSub<0) {
            lk_warning << "empty group" << endl;
            return;
        }

        if (GetGroupDepth()>=3) {
            dirName = dirName + "/" + fName;
            gSystem -> Exec(Form("mkdir -p %s/",dirName.Data()));
        }
        else {
            if (header.IsNull()) header = fName;
            else header = header + "_" + fName;
        }

        if (saveRoot) {
            TString fileName = dirName + "/" + uheader + fullName + tag + ".root";
            lk_info << "Writting" << endl;
            e_cout << "    " << fileName << endl;
            auto file = new TFile(fileName,"recreate");
            Write(write_name);
        }

        if (saveImage) {
            auto sub0 = (LKDrawingGroup*) At(0);
            auto num0 = sub0 -> GetEntries();
            if (num0>4) {

            }
            for (auto iSub=0; iSub<numSub; ++iSub) {
                auto sub = (LKDrawingGroup*) At(iSub);
                sub -> Save(recursive, false, saveImage, dirName, header, tag);
            }
        }
    }
    else
    {
        if (saveRoot) {
            TString fileName = dirName + "/" + uheader + fullName + tag + ".root";
            lk_info << "Writting" << endl;
            e_cout << "    " << fileName << endl;
            auto file = new TFile(fileName,"recreate");
            Write(write_name);
        }
        if (saveImage)
        {
            {
                TString dirNameImage = dirName + "/png";
                gSystem -> Exec(Form("mkdir -p %s/",dirNameImage.Data()));
                TString fileName = dirNameImage + "/" + uheader + fullName + tag + ".png";
                fCvs -> SaveAs(fileName);
            }
            {
                TString dirNameImage = dirName + "/pdf";
                gSystem -> Exec(Form("mkdir -p %s/",dirNameImage.Data()));
                TString fileName = dirNameImage + "/" + uheader + fullName + tag + ".pdf";
                fCvs -> SaveAs(fileName);
            }
            {
                TString dirNameImage = dirName + "/eps";
                gSystem -> Exec(Form("mkdir -p %s/",dirNameImage.Data()));
                TString fileName = dirNameImage + "/" + uheader + fullName + tag + ".eps";
                fCvs -> SaveAs(fileName);
            }
        }
    }
}

void LKDrawingGroup::Print(Option_t *opt) const
{
    TString option = opt;
    bool isTop = false;
    if (LKMisc::CheckOption(option,"level")==false) {
        isTop = true;
        lk_info << "LKDrawingGroup " << fName << endl;
        option = option + ":level=0";
    }
    bool print_all = false;
    if (LKMisc::CheckOption(option,"all"))
        print_all = true;

    int tab = 0;
    if (LKMisc::CheckOption(option,"level"))
        tab = TString(LKMisc::FindOption(option,"level",false,1)).Atoi();
    TString header; for (auto i=0; i<tab; ++i) header += "  ";

    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        TString title = header + Form("DrawingGroup[%d] %s",numSub,fName.Data());
        e_cout << title << endl;

        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            sub -> Print(option);
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        e_cout << header << "Group " << fName << " containing " << numDrawings << " drawings" << endl;
        if (print_all) {
            e_cout << "  - level=" << fGroupLevel << endl;
            e_cout << "  - div=(" << fDivX << "," << fDivY << ")" << endl;
            e_cout << "  - option=" << fGlobalOption << endl;
        }
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            drawing -> Print(option);
        }
        if (LKMisc::CheckOption(option,"!drawing")==false) e_cout << endl;
    }
    if (isTop)
        lk_info << GetNumAllDrawings() << " drawings in total" << endl;
}

bool LKDrawingGroup::ConfigureCanvas()
{
    auto numDrawings = GetEntries();
    int numDrawingsChange = numDrawings - fDivX*fDivY;

    if ((fDivX==0 || fDivY==0) || numDrawingsChange>0)
    {
        if (CheckOption("wide_canvas"))
        {
            if      (numDrawings== 1) { fDivX =  1; fDivY =  1; }
            else if (numDrawings<= 2) { fDivX =  2; fDivY =  1; }
            else if (numDrawings<= 3) { fDivX =  3; fDivY =  1; }
            else if (numDrawings<= 6) { fDivX =  3; fDivY =  2; }
            else if (numDrawings<= 8) { fDivX =  4; fDivY =  2; }
            else if (numDrawings<= 9) { fDivX =  3; fDivY =  3; }
            else if (numDrawings<=12) { fDivX =  4; fDivY =  3; }
            else if (numDrawings<=16) { fDivX =  4; fDivY =  4; }
            else if (numDrawings<=20) { fDivX =  4; fDivY =  5; }
            else if (numDrawings<=24) { fDivX =  4; fDivY =  6; }
            else if (numDrawings<=28) { fDivX =  4; fDivY =  7; }
            else if (numDrawings<=32) { fDivX =  4; fDivY =  8; }
            else if (numDrawings<=36) { fDivX =  4; fDivY =  9; }
            else if (numDrawings<=40) { fDivX =  4; fDivY =  10; }
            else {
                lk_error << "Too many drawings!!! " << numDrawings << endl;
                return false;
            }
        }

        else if (CheckOption("vertical_canvas"))
        {
            if      (numDrawings== 1) { fDivY =  1; fDivX =  1; }
            else if (numDrawings<= 2) { fDivY =  2; fDivX =  1; }
            else if (numDrawings<= 4) { fDivY =  2; fDivX =  2; }
            else if (numDrawings<= 6) { fDivY =  3; fDivX =  2; }
            else if (numDrawings<= 8) { fDivY =  4; fDivX =  2; }
            else if (numDrawings<= 9) { fDivY =  3; fDivX =  3; }
            else if (numDrawings<=12) { fDivY =  4; fDivX =  3; }
            else if (numDrawings<=16) { fDivY =  4; fDivX =  4; }
            else if (numDrawings<=20) { fDivY =  5; fDivX =  4; }
            else if (numDrawings<=25) { fDivY =  6; fDivX =  4; }
            else if (numDrawings<=25) { fDivY =  5; fDivX =  5; }
            else if (numDrawings<=30) { fDivY =  6; fDivX =  5; }
            else if (numDrawings<=35) { fDivY =  7; fDivX =  5; }
            else if (numDrawings<=36) { fDivY =  6; fDivX =  6; }
            else if (numDrawings<=40) { fDivY =  8; fDivX =  5; }
            else if (numDrawings<=42) { fDivY =  7; fDivX =  6; }
            else if (numDrawings<=48) { fDivY =  8; fDivX =  6; }
            else if (numDrawings<=63) { fDivY =  9; fDivX =  7; }
            else if (numDrawings<=80) { fDivY = 10; fDivX =  8; }
            else if (numDrawings<=90) { fDivY = 10; fDivX =  9; }
            else if (numDrawings<=100){ fDivY = 10; fDivX = 10; }
            else if (numDrawings<=110){ fDivY = 12; fDivX = 10; }
            else if (numDrawings<=120){ fDivY = 12; fDivX = 10; }
            else {
                lk_error << "Too many drawings!!! " << numDrawings << endl;
                return false;
            }
        }

        else
        {
            if      (numDrawings== 1) { fDivX =  1; fDivY =  1; }
            else if (numDrawings<= 2) { fDivX =  2; fDivY =  1; }
            else if (numDrawings<= 4) { fDivX =  2; fDivY =  2; }
            else if (numDrawings<= 6) { fDivX =  3; fDivY =  2; }
            else if (numDrawings<= 8) { fDivX =  4; fDivY =  2; }
            else if (numDrawings<= 9) { fDivX =  3; fDivY =  3; }
            else if (numDrawings<=12) { fDivX =  4; fDivY =  3; }
            else if (numDrawings<=16) { fDivX =  4; fDivY =  4; }
            else if (numDrawings<=20) { fDivX =  5; fDivY =  4; }
            else if (numDrawings<=25) { fDivX =  6; fDivY =  4; }
            else if (numDrawings<=25) { fDivX =  5; fDivY =  5; }
            else if (numDrawings<=30) { fDivX =  6; fDivY =  5; }
            else if (numDrawings<=35) { fDivX =  7; fDivY =  5; }
            else if (numDrawings<=36) { fDivX =  6; fDivY =  6; }
            else if (numDrawings<=40) { fDivX =  8; fDivY =  5; }
            else if (numDrawings<=42) { fDivX =  7; fDivY =  6; }
            else if (numDrawings<=48) { fDivX =  8; fDivY =  6; }
            else if (numDrawings<=63) { fDivX =  9; fDivY =  7; }
            else if (numDrawings<=80) { fDivX = 10; fDivY =  8; }
            else if (numDrawings<=90) { fDivX = 10; fDivY =  9; }
            else if (numDrawings<=100){ fDivX = 10; fDivY = 10; }
            else if (numDrawings<=110){ fDivX = 12; fDivY = 10; }
            else if (numDrawings<=120){ fDivX = 12; fDivY = 10; }
            else {
                lk_error << "Too many drawings!!! " << numDrawings << endl;
                return false;
            }
        }
    }

    double resize_factor = 0.5;
    if      (fDivX>=5||fDivY>=5) resize_factor = 1.00;
    else if (fDivX>=4||fDivY>=4) resize_factor = 0.95;
    else if (fDivX>=3||fDivY>=3) resize_factor = 0.90;
    else if (fDivX>=2||fDivY>=2) resize_factor = 0.85;

    if (fCvs==nullptr)
    {
        if (!fFixCvsSize)
        {
            if (fDXCvs==0 || fDYCvs==0) {
                fDXCvs = 800*fDivX;
                fDYCvs = 680*fDivY;
            }
            fCvs = LKPainter::GetPainter() -> CanvasResize(Form("c%s",fName.Data()), fDXCvs, fDYCvs, resize_factor);
        }
        else
            fCvs = new TCanvas(Form("c%s",fName.Data()),Form("c%s",fName.Data()), fDXCvs, fDYCvs);
    }

    if (fPadArray!=nullptr && numDrawingsChange<=0)
    {
        auto numPads = fPadArray -> GetEntries();
        for (auto iPad=0; iPad<numPads; ++iPad) {
            auto pad = fPadArray -> At(iPad);
            fCvs -> cd();
            pad -> Draw();
        }
        return true;
    }

    if (numDrawings==1)
        return true;

    TObject *obj;
    int nPads = 0;
    TIter next(fCvs->GetListOfPrimitives());
    while ((obj=next())) {
        if (obj->InheritsFrom(TVirtualPad::Class()))
            nPads++;
    }
    if (nPads>numDrawings) {
        next.Reset();
        while ((obj=next())) {
            obj -> Clear();
        }
    }
    else if (nPads<numDrawings) {
        next.Reset();
        while ((obj=next())) {
            delete obj;
        }
        DividePad(fCvs,fDivX, fDivY, 0.001, 0.001);
    }

    return true;
}

void LKDrawingGroup::DividePad(TPad* cvs, Int_t nx, Int_t ny, Float_t xmargin, Float_t ymargin, Int_t color)
{
    cvs -> cd();
    if (nx <= 0) nx = 1;
    if (ny <= 0) ny = 1;
    Int_t ix, iy;
    Double_t x1, y1, x2, y2, dx, dy;
    TPad *pad;
    TString name, title;
    Int_t n = 0;
    //if (color == 0) color = GetFillColor();
    //if (xmargin > 0 && ymargin > 0)
    dy = 1/Double_t(ny);
    dx = 1/Double_t(nx);
    if (CheckOption("vertical_pad_numbering")) {
        //for (ix=0;ix<nx;ix++)
        for (ix=nx-1;ix>=0;ix--)
        {
            x2 = 1 - ix*dx - xmargin;
            x1 = x2 - dx + 2*xmargin;
            if (x1 < 0) x1 = 0;
            if (x1 > x2) continue;
            for (iy=ny-1;iy>=0;iy--) {
                y1 = iy*dy + ymargin;
                y2 = y1 +dy -2*ymargin;
                if (y1 > y2) continue;
                n++;
                name.Form("%s_%d", GetName(), n);
                pad = new TPad(name.Data(), name.Data(), x1, y1, x2, y2, color);
                pad->SetNumber(n);
                pad->SetFillColor(cvs->GetFillColor());
                pad->Draw();
            }
        }
    }
    else { //general case
        for (iy=0;iy<ny;iy++) {
            y2 = 1 - iy*dy - ymargin;
            y1 = y2 - dy + 2*ymargin;
            if (y1 < 0) y1 = 0;
            if (y1 > y2) continue;
            for (ix=0;ix<nx;ix++) {
                x1 = ix*dx + xmargin;
                x2 = x1 +dx -2*xmargin;
                if (x1 > x2) continue;
                n++;
                name.Form("%s_%d", GetName(), n);
                pad = new TPad(name.Data(), name.Data(), x1, y1, x2, y2, color);
                pad->SetNumber(n);
                pad->SetFillColor(cvs->GetFillColor());
                pad->Draw();
            }
        }
    }
    cvs -> Modified();
}

bool LKDrawingGroup::AddFile(TFile* file, TString groupSelection)
{
    if (fFileName.IsNull())
        fFileName = file -> GetName();
    else
        fFileName = fFileName + ", " + file->GetName();
    SetName(fFileName);

    bool allowPrint = true;
    if (groupSelection=="xprint") {
        allowPrint = false;
        groupSelection = "";
    }

    LKDrawingGroup* sub = nullptr;

    if (!groupSelection.IsNull())
    {
        vector<TString> groupNameArray;
        if (groupSelection.Index(":")<0)
            groupNameArray.push_back(groupSelection);
        else {
            TObjArray *tokens = groupSelection.Tokenize(":");
            auto n = tokens -> GetEntries();
            for (auto i=0; i<n; ++i) {
                TString token0 = ((TObjString*)tokens->At(i))->GetString();
                groupNameArray.push_back(token0);
            }
        }
        for (auto groupName : groupNameArray)
        {
            if (groupName.EndsWith("*"))
            {
                groupName.Remove(groupName.Sizeof()-2);
                auto listOfKeys = file -> GetListOfKeys();
                TKey* key = nullptr;
                TIter nextKey(listOfKeys);
                while ((key=(TKey*)nextKey())) {
                    if (TString(key->GetName()).Index(groupName)==0) {
                        auto group = (LKDrawingGroup*) key -> ReadObj();
                        if (allowPrint) lk_info << "Adding " << group->GetName() << " from " << file->GetName() << endl;
                        AddGroupInStructure(group);
                    }
                }
            }
            else
            {
                auto obj = file -> Get(groupName);
                if (obj==nullptr) {
                    lk_error << groupName << " is nullptr" << endl;
                    continue;
                }
                if (TString(obj->ClassName())=="LKDrawingGroup") {
                    auto group = (LKDrawingGroup*) obj;
                    if (allowPrint) lk_info << "Adding " << group->GetName() << " from " << file->GetName() << endl;
                    AddGroupInStructure(group);
                }
                else {
                    lk_error << groupName << " type is not LKDrawingGroup" << endl;
                    return false;
                }
            }
        }
    }
    else
    {
        auto listOfKeys = file -> GetListOfKeys();
        TKey* key = nullptr;
        TIter nextKey(listOfKeys);
        while ((key=(TKey*)nextKey())) {
            if (TString(key->GetClassName())=="LKDrawingGroup") {
                auto group = (LKDrawingGroup*) key -> ReadObj();
                if (allowPrint) lk_info << "Adding group (" << group->GetName() << ") from " << file->GetName() << endl;
                AddGroupInStructure(group);
            }
            else if (TString(key->GetClassName())=="LKDrawing") {
                auto draw = (LKDrawing*) key -> ReadObj();
                if (allowPrint) lk_info << "Adding drawing (" << draw->GetName() << ") from " << file->GetName() << endl;
                if (sub==nullptr)
                    sub = CreateGroup("sub");
                sub -> AddDrawing(draw);
            }
            else if (TString(key->GetClassName())=="TObjArray") {
                auto array = (TObjArray*) key -> ReadObj();
                if (allowPrint) lk_info << "Adding TObjArray (" << array->GetName() << ") from " << file->GetName() << endl;
                if (sub==nullptr)
                    sub = CreateGroup("sub");
                sub -> AddDrawing(array);
            }
        }
    }
    return true;
}

bool LKDrawingGroup::AddFile(TString fileName, TString groupSelection)
{
    TFile* file = new TFile(fileName);
    if (!file->IsOpen()) {
        e_error << fileName << " cannot be openned" << endl;
        return false;
    }
    return AddFile(file,groupSelection);
}

int LKDrawingGroup::GetNumGroups() const
{
    if (fIsGroupGroup)
        return GetEntries();
    return 0;
}

bool LKDrawingGroup::CheckIsGroupGroup(bool add)
{
    if (GetEntries()==0) {
        if (add) fIsGroupGroup = true;
        return true;
    }
    if (!fIsGroupGroup) {
        if (add) lk_error << "group is drawing-group" << endl;
        return false;
    }
    return true;
}

bool LKDrawingGroup::CheckIsDrawingGroup(bool add)
{
    if (GetEntries()==0) {
        if (add) fIsGroupGroup = false;
        return true;
    }
    if (fIsGroupGroup) {
        if (add) lk_error << "group is group-group" << endl;
        return false;
    }
    return true;
}

int LKDrawingGroup::GetGroupDepth() const
{
    if (fIsGroupGroup) {
        auto maxDepth = 0;
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            auto depth = sub -> GetGroupDepth();
            if (depth>maxDepth)
                maxDepth = depth;
        }
        return maxDepth + 1;
    }
    else
        return 1;
}

TString LKDrawingGroup::GetFullName() const
{
    if (fParentName.IsNull())
        return fName;
    else {
        return fParentName+":"+fName;
    }
}

void LKDrawingGroup::Add(TObject *obj)
{
    auto IsMotherPad = [](TPad* pad)
    {
        bool isMotherPad = false;
        auto list = pad -> GetListOfPrimitives();
        TIter next(list);
        TObject *objIn;
        while ((objIn = next()))
        {
            if (objIn->InheritsFrom(TPad::Class())) {
                isMotherPad = true;
                break;
            }
        }
        return isMotherPad;
    };

    if      (obj->InheritsFrom(TH1::Class())) { AddHist((TH1*) obj); return; }
    else if (obj->InheritsFrom(TGraph::Class())) { AddGraph((TGraph*) obj); return; }
    else if (obj->InheritsFrom(LKDrawing::Class()) || obj->InheritsFrom(LKDrawingGroup::Class())) TObjArray::Add(obj);
    else if (obj->InheritsFrom(TPad::Class()))
    {
        auto pad = (TPad*) obj;
        bool isMotherPad = IsMotherPad(pad);
        SetCanvas(pad);

        auto list = pad -> GetListOfPrimitives();
        TIter next(list);
        if (isMotherPad)
        {
            int countSubs = 0;
            TObject *objIn;
            while ((objIn = next()))
            {
                if (objIn->InheritsFrom(TPad::Class())) {
                    if (IsMotherPad((TPad*)objIn)) {
                        auto sub = CreateGroup(Form("%s_%d",fName.Data(),countSubs++));
                        sub -> Add(objIn);
                    }
                    else {
                        LKDrawing *draw = CreateDrawing(Form("draw_%s_%d",fName.Data(),countSubs++));
                        draw -> Add(objIn);
                    }
                }
                else
                    lk_error << "not TPad" << endl;
            }
        }
        else
        {
            LKDrawing *draw = CreateDrawing(Form("draw_%s",fName.Data()));
            draw -> Add(pad);
        }
    }
    else
        lk_error << "Cannot add " << obj->ClassName() << " directly!" << endl;
}

int LKDrawingGroup::HAdd(TString fileName)
{
    lk_info << "Adding histograms in " << fileName << " (";
    int numHistograms = HAdd(new LKDrawingGroup(fileName,"xprint"));
    e_cout << numHistograms << ")" << endl;
    return numHistograms;
}

int LKDrawingGroup::HAdd(LKDrawingGroup* atop)
{
    if (GetEntries()==0)
    {
        auto numGroupA = atop -> GetEntries();
        for (auto iGroupA=0; iGroupA<numGroupA; ++iGroupA)
        {
            auto groupA = (LKDrawingGroup*) atop -> At(iGroupA);
            AddGroup(groupA);
        }
        return 0;
    }

    auto nameArray = MakeHistNameArray();
    int countHist = 0;
    for (auto name : nameArray) {
        auto hist0 = FindHist(name);
        auto hist1 = atop -> FindHist(name);
        auto added = hist0 -> Add(hist1);
        countHist++;
    }
    return countHist;
}

vector<TString> LKDrawingGroup::MakeHistNameArray()
{
    vector<TString> nameArray;

    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            auto nameArraySub = sub -> MakeHistNameArray();
            for (auto name : nameArraySub)
                nameArray.push_back(name);
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            auto numObjects = drawing -> GetEntries();
            for (auto iObj=0; iObj<numObjects; ++iObj)
            {
                auto obj = drawing -> At(iObj);
                if (obj->InheritsFrom(TH1::Class()))
                    nameArray.push_back(obj->GetName());
            }
        }
    }

    return nameArray;
}

void LKDrawingGroup::AddDrawing(LKDrawing* drawing)
{
    if (CheckIsDrawingGroup(true))
        Add(drawing);
}

void LKDrawingGroup::AddDrawing(TObjArray* array)
{
    auto tokenizeToStringArray = [](TString title) {
        vector<TString> stringArray;
        while (title.Index(";;")>=0)
            title.ReplaceAll(";;","; ;");
        auto tokens = title.Tokenize(";");
        auto numTokens = tokens -> GetEntries();
        for (auto iTok=0; iTok<numTokens; ++iTok)
        {
            TString value = ((TObjString*)tokens->At(iTok))->GetString();
            if (value==" ") value = "";
            stringArray.push_back(value);
        }
        return stringArray;
    };

    vector<TString> titleArray;
    vector<TString> drawOptionArray;

    auto draw = CreateDrawing(array->GetName());
    auto numObjects = array -> GetEntries();
    auto countDrawObj = 0;
    for (auto iObj=0; iObj<numObjects; ++iObj)
    {
        auto obj = array -> At(iObj);
        if (TString(obj->ClassName())=="TNamed")
        {
            auto named = (TNamed*) obj;
            TString name = named -> GetName();
            TString title = named -> GetTitle();
            if (name=="global_option") draw -> SetGlobalOption(title);
            else if (name=="title_array") titleArray = tokenizeToStringArray(title);
            else if (name=="draw_option_array") drawOptionArray = tokenizeToStringArray(title);
        }
        else if (titleArray.size()>=0 && drawOptionArray.size()>=0) {
            draw -> Add(obj,drawOptionArray.at(countDrawObj),titleArray.at(countDrawObj));
            countDrawObj++;
        }
    }
}

void LKDrawingGroup::AddGraph(TGraph* graph, TString drawOption, TString title)
{
    if (CheckIsDrawingGroup(true)) {
        auto drawing = new LKDrawing();
        drawing -> Add(graph,drawOption,title);
        Add(drawing);
    }
}

void LKDrawingGroup::AddHist(TH1 *hist, TString drawOption, TString title)
{
    if (CheckIsDrawingGroup(true)) {
        auto drawing = new LKDrawing();
        drawing -> Add(hist,drawOption,title);
        Add(drawing);
    }
}

int LKDrawingGroup::GetNumDrawings() const
{
    if (!fIsGroupGroup)
        return GetEntries();
    return 0;
}

int LKDrawingGroup::GetNumAllGroups() const
{
    int numDrawings = 0;
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            numDrawings += sub -> GetNumAllGroups();
        }
    }
    else
        numDrawings = 1;

    return numDrawings;
}

int LKDrawingGroup::GetNumAllDrawings() const
{
    int numDrawings = 0;
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            numDrawings += sub -> GetNumAllDrawings();
        }
    }
    else
        numDrawings += GetEntries();

    return numDrawings;
}

int LKDrawingGroup::GetNumAllDrawingObjects() const
{
    int numObjects = 0;
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            numObjects += sub -> GetNumAllDrawingObjects();
        }
    }
    else {
        auto numDrawing = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawing; ++iDrawing) {
            auto drawing = (LKDrawingGroup*) At(iDrawing);
            numObjects += drawing -> GetEntries();
        }
    }

    return numObjects;
}

void LKDrawingGroup::AddGroupInStructure(LKDrawingGroup *group)
{
    auto parentLevel = group -> GetGroupLevel() - 1;
    auto parentName = group -> GetParentName();
    if (parentLevel<=0)
    {
        AddGroup(group);
    }
    else
    {
        vector<TString> parentNameArray;
        parentName = Form(" %s ",parentName.Data());
        TObjArray *tokens = parentName.Tokenize(":");
        auto n = tokens -> GetEntries();
        for (auto i=0; i<n; ++i) {
            TString token0 = ((TObjString*)tokens->At(i))->GetString();
            token0.ReplaceAll(" ","");
            parentNameArray.push_back(token0);
        }
        if (parentLevel==1) {
            TString pname = parentNameArray.at(1);
            LKDrawingGroup* pGroup = FindGroup(pname);
            if (pGroup==nullptr)
                pGroup = CreateGroup(pname);
            pGroup -> AddGroup(group);
        }
    }
}

void LKDrawingGroup::AddGroup(LKDrawingGroup *group)
{
    if (CheckIsGroupGroup(true)) {
        group -> SetGroupLevel(fGroupLevel+1);
        TString fullName = GetFullName();
        group -> SetParentName(fullName);
        Add(group);
    }
}

LKDrawingGroup* LKDrawingGroup::CreateGroup(TString name, bool addToList)
{
    if (CheckIsGroupGroup(true)) {
        if (name.IsNull()) name = Form("%s_%d",fName.Data(),GetEntries());
        auto sub = new LKDrawingGroup(name,fGroupLevel+1);
        if (addToList) AddGroup(sub);
        return sub;
    }
    return (LKDrawingGroup*) nullptr;
}

LKDrawingGroup* LKDrawingGroup::GetGroup(int iSub)
{
    if (CheckIsDrawingGroup())
        return (LKDrawingGroup*) nullptr;
    if (iSub<0 || iSub>=GetEntries()) {
        return (LKDrawingGroup*) nullptr;
    }
    auto subGroup = (LKDrawingGroup*) At(iSub);
    return subGroup;
}

LKDrawing* LKDrawingGroup::GetDrawing(int iDrawing)
{
    if (CheckIsGroupGroup())
        return (LKDrawing*) nullptr;
    if (iDrawing<0 || iDrawing>=GetEntries()) {
        return (LKDrawing*) nullptr;
    }
    auto drawing = (LKDrawing*) At(iDrawing);
    return drawing;
}

LKDrawing* LKDrawingGroup::CreateDrawing(TString name, bool addToList)
{
    if (CheckIsDrawingGroup(true)) {
        auto drawing = new LKDrawing(name);
        if (addToList) Add(drawing);
        return drawing;
    }
    return (LKDrawing*) nullptr;
}

LKDrawingGroup* LKDrawingGroup::FindGroup(TString name, int option)
{
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            TString groupSelection = sub -> GetName();
            if (option==2)
            {
                if (name.Index(groupSelection)>=0)
                    return sub;
            }
            else {
                if (option==1)
                    groupSelection = sub -> GetFullName();
                if (groupSelection==name)
                    return sub;
            }
            auto found = sub -> FindGroup(name, option);
            if (found!=nullptr)
                return found;
        }
    }
    return ((LKDrawingGroup *) nullptr);
}

bool LKDrawingGroup::CheckGroup(LKDrawingGroup *find)
{
    if (fIsGroupGroup)
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            if (sub==find)
                return true;
            auto found = sub -> CheckGroup(find);
            if (found)
                return true;
        }
    }
    return false;
}

LKDrawing* LKDrawingGroup::FindDrawing(TString name)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            LKDrawing* drawing = sub -> FindDrawing(name);
            if (drawing!=nullptr)
                return drawing;
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            if (drawing->GetName()==name)
                return drawing;
        }
    }
    return (LKDrawing*) nullptr;
}

TH1* LKDrawingGroup::FindHist(TString name)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            TH1* hist = sub -> FindHist(name);
            if (hist!=nullptr)
                return hist;
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            auto numObjs = drawing -> GetEntries();
            for (auto iObj=0; iObj<numObjs; ++iObj)
            {
                auto obj = drawing -> At(iObj);
                if (obj->InheritsFrom(TH1::Class()))
                {
                    if (obj->GetName()==name) {
                        return (TH1*) obj;
                    }
                }
            }
        }
    }
    return (TH1*) nullptr;
}

TGraph* LKDrawingGroup::FindGraph(TString name)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            TGraph* graph = sub -> FindGraph(name);
            if (graph!=nullptr)
                return graph;
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            auto numObjs = drawing -> GetEntries();
            for (auto iObj=0; iObj<numObjs; ++iObj)
            {
                auto obj = drawing -> At(iObj);
                if (obj->InheritsFrom(TGraph::Class()))
                {
                    if (obj->GetName()==name) {
                        return (TGraph*) obj;
                    }
                }
            }
        }
    }
    return (TGraph*) nullptr;
}

TF1* LKDrawingGroup::FindFunction(TString name)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            TF1* f1 = sub -> FindFunction(name);
            if (f1!=nullptr)
                return f1;
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            auto numObjs = drawing -> GetEntries();
            for (auto iObj=0; iObj<numObjs; ++iObj)
            {
                auto obj = drawing -> At(iObj);
                if (obj->InheritsFrom(TF1::Class()))
                {
                    if (obj->GetName()==name) {
                        return (TF1*) obj;
                    }
                }
            }
        }
    }
    return (TF1*) nullptr;
}

TObject* LKDrawingGroup::FindObject(const char* name) const
{
    if (IsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            TObject* object = sub -> FindObject(name);
            if (object!=nullptr)
                return object;
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            auto numObjs = drawing -> GetEntries();
            for (auto iObj=0; iObj<numObjs; ++iObj)
            {
                auto object = drawing -> At(iObj);
                if (TString(object->GetName())==name) {
                    return object;
                }
            }
        }
    }
    return (TObject*) nullptr;
}

TObject* LKDrawingGroup::FindClassObject(TString name, TClass *tclass)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            TObject* object = sub -> FindClassObject(name,tclass);
            if (object!=nullptr)
                return object;
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            auto numObjs = drawing -> GetEntries();
            for (auto iObj=0; iObj<numObjs; ++iObj)
            {
                auto object = drawing -> At(iObj);
                if (object->InheritsFrom(tclass))
                {
                    if (object->GetName()==name) {
                        return object;
                    }
                }
            }
        }
    }
    return (TObject*) nullptr;
}

void LKDrawingGroup::ApplyStyle(TString drawStyle)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            sub -> ApplyStyle(drawStyle);
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            drawing -> ApplyStyle(drawStyle);
        }
    }
}

void LKDrawingGroup::SetStyle(TString drawStyle)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            sub -> SetStyle(drawStyle);
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            drawing -> SetStyle(drawStyle);
        }
    }
}

void LKDrawingGroup::SetDraw(bool draw)
{
    if (CheckIsGroupGroup())
    {
        auto numSub = GetEntries();
        for (auto iSub=0; iSub<numSub; ++iSub) {
            auto sub = (LKDrawingGroup*) At(iSub);
            sub -> SetDraw(draw);
        }
    }
    else
    {
        auto numDrawings = GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
        {
            auto drawing = (LKDrawing*) At(iDrawing);
            drawing -> SetDraw(draw);
        }
    }
}
