#include "LKDrawingGroup.h"
#include "LKDataViewer.h"
#include "LKMisc.h"
#include "TKey.h"
#include "TObjString.h"
#include "TSystem.h"
#include "TBufferFile.h"
#include "TBuffer.h"
#include "TNamed.h"
#include <functional>
#include <map>

ClassImp(LKDrawingGroup)

void LKDrawingGroup::Streamer(TBuffer &buffer)
{
    if (buffer.IsWriting()) {
        buffer.WriteClassBuffer(LKDrawingGroup::Class(),this);
        return;
    }

    UInt_t start = 0;
    UInt_t count = 0;
    auto version = buffer.ReadVersion(&start,&count);
    buffer.ReadClassBuffer(LKDrawingGroup::Class(),this,version,start,count);
    if (version<3) {
        fNodeType = fIsGroupGroup ? NodeType::Groups : NodeType::Drawings;
        fEntries.Clear();
        for (auto index=0; index<TObjArray::GetEntries(); ++index)
            fEntries.Add(TObjArray::At(index));
        TObjArray::Clear();
        fIsGroupGroup = false;
    }
}

LKDrawingGroup::LKDrawingGroup(TString name, int groupLevel)
: TObjArray()
{
    Init();
    if (name.EndsWith(".root"))
        if (AddFile(name))
            return;
    SetName(name);
    fGroupLevel = groupLevel;
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
    fEntries.Clear();
    fCvs = nullptr;
    fDivX = 0;
    fDivY = 0;
    fGroupLevel = 0;
    fNodeType = NodeType::Drawings;
}

void LKDrawingGroup::Clear(Option_t *)
{
    fEntries.Clear();
    fNodeType = NodeType::Drawings;
    fCvs = nullptr;
    fViewer = nullptr;
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

    auto numDrawings = GetEntries();

    bool usingDataViewer = false;
    if (fViewer!=nullptr)
        usingDataViewer = true;
    if (usingDataViewer==false)
        usingDataViewer = LKMisc::CheckOption(optionString,"viewer # (dg) draw using LKDataViewer",true);

    bool drawGroup = true;
    if (usingDataViewer)
    {
        drawGroup = false;
        if (fViewer==nullptr) {
            fViewer = new LKDataViewer(this);
            fViewer -> Draw(optionString);
        }

        if (fViewer->IsActive())
            lk_warning << "viewer already running!" << endl;
        else
            drawGroup = true;
    }

    if (drawGroup)
    {
        if (CheckIsGroupGroup())
        {
            auto numSub = GetEntries();
            for (auto iSub=0; iSub<numSub; ++iSub) {
                auto sub = (LKDrawingGroup*) At(iSub);
                auto subOption = sub -> GetGlobalOption();
                sub -> AppenGlobalOption(fGlobalOption);
                sub -> Draw(optionString);
                sub -> SetGlobalOption(subOption);
            }
        }
        else
        {
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
        file -> Close();
        delete file;
    }
    else {
        if (fileName.IsNull())
            fileName = Form("data_lilak/%s.root",fName.Data());
        lk_info << "Writting" << endl;
        e_cout << "    " << fileName << endl;
        auto file = new TFile(fileName,"recreate");
        Write(option);
        file -> Close();
        delete file;
    }
}

Int_t LKDrawingGroup::Write(const char *name, Int_t option, Int_t bsize) const
{
    TString name0 = (name==nullptr ? "" : name);
    if (LKMisc::CheckOption(name0,"chunked")) {
        int chunkSizeMB = LKMisc::FindOptionInt(name0,"chunk_mb",512);
        if (chunkSizeMB<64) chunkSizeMB = 64;
        if (chunkSizeMB>768) {
            lk_warning << "chunk_mb=" << chunkSizeMB << " is too close to ROOT's 1 GB object-buffer limit; using 768 MB" << endl;
            chunkSizeMB = 768;
        }
        return WriteChunked(Long64_t(chunkSizeMB)*1024*1024);
    }
    bool flat = (name0=="flat");
    bool write_only_fit = LKMisc::CheckOption(name0,"FITPARAMETERS");
    int countDrawings = LKMisc::FindOptionInt(name0,"draw_count",0);
    int countDrawingsLocal = 0;
    if (flat) // flat
    {
        if (IsDrawingGroup())
        {
            for (auto iDrawing=0; iDrawing<GetEntries(); ++iDrawing) {
                auto draw = (LKDrawing*) At(iDrawing);
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
            TObject::Write(name0, 0, bsize);
        }
        else {
            TString name1 = fName;
            if (name1.IsNull()) name1 = "group";
            TObject::Write(name1, option, bsize);
            if (!flat && fPar!=nullptr) fPar -> Write();
        }
    }

    return countDrawingsLocal;
}

void LKDrawingGroup::CopyGroupMetadata(const LKDrawingGroup* source)
{
    SetName(source->GetName());
    fDivX = source->fDivX;
    fDivY = source->fDivY;
    fGroupLevel = source->fGroupLevel;
    fNodeType = source->fNodeType;
    fParentName = source->fParentName;
    fGlobalOption = source->fGlobalOption;
    fFixCvsSize = source->fFixCvsSize;
    fUserCvsSize = source->fUserCvsSize;
    fDXCvs = source->fDXCvs;
    fDYCvs = source->fDYCvs;
}

LKDrawingGroup* LKDrawingGroup::CloneStructureOnly() const
{
    auto clone = new LKDrawingGroup();
    clone -> CopyGroupMetadata(this);
    if (IsGroupGroup()) {
        auto numGroups = GetEntries();
        for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
            auto group = (LKDrawingGroup*) At(iGroup);
            clone -> AddEntry(group->CloneStructureOnly());
        }
    }
    return clone;
}

Int_t LKDrawingGroup::WriteChunked(Long64_t targetSize) const
{
    TDirectory* directory = gDirectory;
    if (directory==nullptr) {
        lk_error << "No output directory is selected for chunked LKDrawingGroup output" << endl;
        return 0;
    }

    map<const LKDrawing*,Long64_t> drawingSizes;
    map<const LKDrawingGroup*,Long64_t> groupSizes;
    function<Long64_t(const LKDrawingGroup*)> estimateGroup;
    estimateGroup = [&](const LKDrawingGroup* group) -> Long64_t
    {
        Long64_t payloadSize = 0;
        if (group->IsGroupGroup()) {
            auto mutableGroup = const_cast<LKDrawingGroup*>(group);
            auto numGroups = group->GetNumGroups();
            for (auto iGroup=0; iGroup<numGroups; ++iGroup)
                payloadSize += estimateGroup(mutableGroup->GetGroup(iGroup));
        }
        else {
            auto mutableGroup = const_cast<LKDrawingGroup*>(group);
            auto numDrawings = group->GetNumDrawings();
            for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing) {
                auto drawing = mutableGroup->GetDrawing(iDrawing);
                TBufferFile buffer(TBuffer::kWrite);
                buffer.WriteObjectAny(drawing,drawing->IsA());
                auto drawingSize = Long64_t(buffer.Length());
                drawingSizes[drawing] = drawingSize;
                payloadSize += drawingSize;
            }
        }
        // TBufferFile lengths of the children are nearly additive.  Keep a
        // generous margin for collection streamers, object maps, and schema.
        auto estimatedSize = payloadSize + payloadSize/10 + 64*1024;
        groupSizes[group] = estimatedSize;
        return estimatedSize;
    };
    estimateGroup(this);

    // Keep ordinary-sized groups as an ordinary ROOT key.  Apart from being
    // easier to inspect interactively, this also keeps files compatible with
    // existing code which expects to find the top group by its real name.
    if (groupSizes[this]<=targetSize) {
        TString keyName = fName;
        if (keyName.IsNull()) keyName = "group";
        auto result = TObject::Write(keyName,TObject::kSingleKey);
        if (fPar!=nullptr) fPar -> Write();
        directory -> SaveSelf(true);
        lk_info << "Group fits in one ROOT key (estimated "
                << groupSizes[this]/(1024*1024) << " MB); wrote "
                << keyName << " without chunk metadata" << endl;
        return GetNumAllDrawings();
    }

    TNamed format("__lkdg_format__","chunked-v1");
    format.Write("__lkdg_format__",TObject::kOverwrite);

    auto structure = CloneStructureOnly();
    directory -> WriteTObject(structure,"__lkdg_structure__","Overwrite");

    int chunkIndex = 0;
    int numDrawingsWritten = 0;
    auto writeGroup = [&](const LKDrawingGroup* group)
    {
        TString keyName = Form("__lkdg_chunk_%06d",chunkIndex++);
        directory -> WriteTObject(group,keyName,"Overwrite");
    };

    function<void(const LKDrawingGroup*)> writeAdaptive;
    writeAdaptive = [&](const LKDrawingGroup* group)
    {
        if (groupSizes[group]<=targetSize) {
            writeGroup(group);
            numDrawingsWritten += group->GetNumAllDrawings();
            return;
        }

        if (group->IsGroupGroup()) {
            auto mutableGroup = const_cast<LKDrawingGroup*>(group);
            auto numGroups = group->GetNumGroups();
            for (auto iGroup=0; iGroup<numGroups; ++iGroup)
                writeAdaptive(mutableGroup->GetGroup(iGroup));
            return;
        }

        // A leaf group may itself exceed the target.  Pack consecutive
        // drawings into metadata-compatible group chunks below the target.
        auto mutableGroup = const_cast<LKDrawingGroup*>(group);
        auto chunk = group->CloneStructureOnly();
        Long64_t chunkSize = 64*1024;
        auto flushChunk = [&]()
        {
            if (chunk->GetEntries()==0) return;
            writeGroup(chunk);
            numDrawingsWritten += chunk->GetEntries();
            chunk -> ClearEntries();
            chunkSize = 64*1024;
        };

        auto numDrawings = group->GetNumDrawings();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing) {
            auto drawing = mutableGroup->GetDrawing(iDrawing);
            auto drawingSize = drawingSizes[drawing] + drawingSizes[drawing]/10;
            if (chunk->GetEntries()>0 && chunkSize+drawingSize>targetSize)
                flushChunk();
            chunk -> AddEntry(drawing);
            chunkSize += drawingSize;
        }
        flushChunk();
        delete chunk;
    };
    writeAdaptive(this);

    if (fPar!=nullptr) fPar -> Write();
    directory -> SaveSelf(true);
    lk_info << "Wrote " << chunkIndex << " adaptive chunks (target "
            << targetSize/(1024*1024) << " MB) for " << numDrawingsWritten
            << " drawings" << endl;
    return numDrawingsWritten;
}

void LKDrawingGroup::Browse(TBrowser *b)
{
    if (IsGroupGroup()) {
        if (b!=nullptr)
            for (auto iEntry=0; iEntry<GetEntries(); ++iEntry)
                b->Add(At(iEntry));
    }
    else {
        if (gPad==nullptr)
            return;
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

    if (IsGroupGroup())
    {
        auto numSub = GetEntries();
        if (numSub<=0) {
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
            if (fCvs==nullptr) {
                lk_warning << "canvas is nullptr; skipping image output for " << fName << endl;
                return;
            }
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

    if (IsGroupGroup())
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

    int dXCvs = fDXCvs;
    int dYCvs = fDYCvs;

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

    // Only fall back to the per-pad default size when the user did not ask for a
    // size: numDrawingsChange is >0 on the first Draw() (fDivX/fDivY are still 0
    // there), so without this guard the requested size is always overwritten.
    if (!fUserCvsSize && (fDXCvs==0 || fDYCvs==0 || numDrawingsChange>0))
    {
        fDXCvs = 800*fDivX;
        fDYCvs = 680*fDivY;
    }
    double cvs_r = LKMisc::FindOptionDouble(fGlobalOption,"cvs_r",-1);
    if (cvs_r>0) resize_factor = cvs_r;

    if (fCvs==nullptr)
    {
        TString canvasName = "c" + GetFullName();
        canvasName.ReplaceAll(":", "_");
        if (!fFixCvsSize)
            fCvs = LKPainter::GetPainter() -> CanvasResize(canvasName, fDXCvs, fDYCvs, resize_factor);
        else {
            fCvs = new TCanvas(canvasName,canvasName, fDXCvs, fDYCvs);
            if (gROOT->IsBatch())
                fCvs -> SetCanvasSize(fDXCvs, fDYCvs);
            else {
                // The constructor argument is the outer window size.  Calling
                // SetCanvasSize(dx,dy) afterwards makes the drawable area
                // larger than that window by the title/menu frame and ROOT
                // consequently adds scrollbars.  Enlarge the window by the
                // measured frame instead, leaving the drawable area at the
                // exact size requested by SetCanvasSize().
                auto frameWidth = fDXCvs - fCvs->GetWw();
                auto frameHeight = fDYCvs - fCvs->GetWh();
                fCvs -> SetWindowSize(fDXCvs + frameWidth,
                                      fDYCvs + frameHeight);
            }
            fCvs -> Modified();
            fCvs -> Update();
        }
    }
    //else if (!(fCvs->GetWw()==fDXCvs && fCvs->GetWh()==fDYCvs))
    //{
    //    if (!fFixCvsSize)
    //        fCvs = LKPainter::GetPainter() -> CanvasResize(fCvs, fDXCvs, fDYCvs, resize_factor);
    //    else {
    //        fCvs -> SetCanvasSize(fDXCvs, fDYCvs);
    //        //fCvs -> SetWindowSize(fDXCvs, fDYCvs);
    //    }
    //}
    //else {
    //}

    fDXCvs = fCvs -> GetWw();
    fDYCvs = fCvs -> GetWh();

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

    auto format = dynamic_cast<TNamed*>(file->Get("__lkdg_format__"));
    if (format!=nullptr && TString(format->GetTitle())=="chunked-v1")
        return AddChunkedFile(file,groupSelection);

    bool allowPrint = true;
    if (groupSelection=="xprint") {
        allowPrint = false;
        groupSelection = "";
    }

    LKDrawingGroup* sub = nullptr;

    if (!groupSelection.IsNull())
    {
        LKDrawingGroup* embeddedTopGroup = nullptr;
        auto listOfKeys = file->GetListOfKeys();
        TIter findEmbeddedTop(listOfKeys);
        TKey* embeddedKey = nullptr;
        while ((embeddedKey=(TKey*)findEmbeddedTop())) {
            if (TString(embeddedKey->GetClassName())!="LKDrawingGroup")
                continue;
            auto candidate = dynamic_cast<LKDrawingGroup*>(file->Get(embeddedKey->GetName()));
            if (candidate!=nullptr && candidate->GetGroupLevel()==0) {
                embeddedTopGroup = candidate;
                break;
            }
        }

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
                TKey* key = nullptr;
                TIter nextKey(listOfKeys);
                while ((key=(TKey*)nextKey())) {
                    if (TString(key->GetName()).Index(groupName)==0) {
                        auto group = (LKDrawingGroup*) key -> ReadObj();
                        if (allowPrint) lk_info << "Adding " << group->GetName() << " from " << file->GetName() << endl;
                        AddGroupInStructure(group);
                    }
                }
                if (embeddedTopGroup!=nullptr) {
                    function<void(LKDrawingGroup*)> addMatchingGroups;
                    addMatchingGroups = [&](LKDrawingGroup* parent) {
                        if (!parent->IsGroupGroup())
                            return;
                        for (auto index=0; index<parent->GetEntries(); ++index) {
                            auto child = (LKDrawingGroup*)parent->At(index);
                            if (TString(child->GetName()).Index(groupName)==0) {
                                if (allowPrint) lk_info << "Adding " << child->GetName()
                                                       << " from embedded top group in " << file->GetName() << endl;
                                AddGroupInStructure(child);
                            }
                            else
                                addMatchingGroups(child);
                        }
                    };
                    addMatchingGroups(embeddedTopGroup);
                }
            }
            else
            {
                auto obj = file -> Get(groupName);
                if (obj==nullptr && embeddedTopGroup!=nullptr)
                    obj = embeddedTopGroup->FindGroup(groupName);
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

        // A file containing one level-0 group represents that group itself,
        // not a new anonymous parent around it.  Adopt its metadata and
        // children so single-key and chunked round trips have the same tree.
        TKey* onlyTopGroupKey = nullptr;
        int numGroupKeys = 0;
        TIter findTopKey(listOfKeys);
        TKey* candidateKey = nullptr;
        while ((candidateKey=(TKey*)findTopKey())) {
            if (TString(candidateKey->GetClassName())=="LKDrawingGroup") {
                onlyTopGroupKey = candidateKey;
                ++numGroupKeys;
            }
        }
        if (GetEntries()==0 && numGroupKeys==1) {
            auto group = dynamic_cast<LKDrawingGroup*>(onlyTopGroupKey->ReadObj());
            if (group!=nullptr && group->GetGroupLevel()==0) {
                CopyGroupMetadata(group);
                auto numEntries = group->GetEntries();
                for (auto iEntry=0; iEntry<numEntries; ++iEntry)
                    AddEntry(group->At(iEntry));
                if (allowPrint)
                    lk_info << "Loaded top group (" << group->GetName()
                            << ") from " << file->GetName() << endl;
                return true;
            }
        }

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

LKDrawingGroup* LKDrawingGroup::FindGroupByFullName(TString fullName)
{
    if (GetFullName()==fullName)
        return this;
    if (!IsGroupGroup())
        return nullptr;

    auto numGroups = GetEntries();
    for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
        auto group = (LKDrawingGroup*) At(iGroup);
        auto found = group -> FindGroupByFullName(fullName);
        if (found!=nullptr)
            return found;
    }
    return nullptr;
}

void LKDrawingGroup::MergeChunk(LKDrawingGroup* chunk)
{
    if (chunk==nullptr)
        return;

    if (fNodeType != chunk->fNodeType) {
        lk_error << "Cannot merge incompatible group chunk " << chunk->GetFullName()
                 << " into " << GetFullName() << endl;
        return;
    }

    if (IsDrawingGroup()) {
        auto numDrawings = chunk->GetEntries();
        for (auto iDrawing=0; iDrawing<numDrawings; ++iDrawing)
            AddEntry(chunk->At(iDrawing));
        return;
    }

    auto numGroups = chunk->GetEntries();
    for (auto iGroup=0; iGroup<numGroups; ++iGroup) {
        auto chunkGroup = (LKDrawingGroup*) chunk->At(iGroup);
        auto target = FindGroupByFullName(chunkGroup->GetFullName());
        if (target==nullptr) {
            lk_error << "Chunk contains a group missing from the structure: "
                     << chunkGroup->GetFullName() << endl;
            continue;
        }
        target -> MergeChunk(chunkGroup);
    }
}

bool LKDrawingGroup::AddChunkedFile(TFile* file, TString groupSelection)
{
    bool allowPrint = true;
    if (groupSelection=="xprint") {
        allowPrint = false;
        groupSelection = "";
    }

    auto structure = dynamic_cast<LKDrawingGroup*>(file->Get("__lkdg_structure__"));
    if (structure==nullptr) {
        lk_error << "Chunked file has no __lkdg_structure__: " << file->GetName() << endl;
        return false;
    }

    ClearEntries();
    CopyGroupMetadata(structure);
    auto numStructureGroups = structure->GetEntries();
    for (auto iGroup=0; iGroup<numStructureGroups; ++iGroup)
        AddEntry(structure->At(iGroup));

    auto listOfKeys = file->GetListOfKeys();
    TIter nextKey(listOfKeys);
    TKey* key = nullptr;
    int numChunks = 0;
    while ((key=(TKey*)nextKey())) {
        if (TString(key->GetName()).Index("__lkdg_chunk_")!=0)
            continue;
        auto chunk = dynamic_cast<LKDrawingGroup*>(key->ReadObj());
        if (chunk==nullptr) {
            lk_error << "Invalid LKDrawingGroup chunk " << key->GetName()
                     << " in " << file->GetName() << endl;
            return false;
        }
        auto target = FindGroupByFullName(chunk->GetFullName());
        if (target==nullptr) {
            lk_error << "Cannot find chunk target " << chunk->GetFullName()
                     << " in " << file->GetName() << endl;
            return false;
        }
        target -> MergeChunk(chunk);
        ++numChunks;
    }

    if (!groupSelection.IsNull()) {
        vector<TString> selections;
        auto tokens = groupSelection.Tokenize(":");
        auto numTokens = tokens->GetEntries();
        for (auto iToken=0; iToken<numTokens; ++iToken)
            selections.push_back(((TObjString*)tokens->At(iToken))->GetString());

        auto matchesSelection = [&](TString groupName)
        {
            for (auto selection : selections) {
                if (selection.EndsWith("*")) {
                    selection.Remove(selection.Length()-1);
                    if (groupName.Index(selection)==0)
                        return true;
                }
                else if (groupName==selection)
                    return true;
            }
            return false;
        };

        function<bool(LKDrawingGroup*,bool)> pruneGroups;
        pruneGroups = [&](LKDrawingGroup* group, bool keepSubtree)
        {
            keepSubtree = keepSubtree || matchesSelection(group->GetName());
            if (!group->IsGroupGroup())
                return keepSubtree;

            bool keepChild = false;
            for (auto iGroup=group->GetEntries()-1; iGroup>=0; --iGroup) {
                auto child = (LKDrawingGroup*) group->At(iGroup);
                if (pruneGroups(child,keepSubtree))
                    keepChild = true;
                else
                    group->fEntries.RemoveAt(iGroup);
            }
            group->fEntries.Compress();
            return keepSubtree || keepChild;
        };
        pruneGroups(this,false);
        delete tokens;
    }
    if (allowPrint)
        lk_info << "Loaded " << numChunks << " adaptive chunks from "
                << file->GetName() << endl;
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
    if (IsGroupGroup())
        return GetEntries();
    return 0;
}

bool LKDrawingGroup::CheckIsGroupGroup(bool add)
{
    if (GetEntries()==0) {
        if (add) {
            fNodeType = NodeType::Groups;
            return true;
        }
        return IsGroupGroup();
    }
    if (!IsGroupGroup()) {
        if (add) lk_error << "group is drawing-group" << endl;
        return false;
    }
    return true;
}

bool LKDrawingGroup::CheckIsDrawingGroup(bool add)
{
    if (GetEntries()==0) {
        if (add) {
            fNodeType = NodeType::Drawings;
            return true;
        }
        return IsDrawingGroup();
    }
    if (IsGroupGroup()) {
        if (add) lk_error << "group is group-group" << endl;
        return false;
    }
    return true;
}

int LKDrawingGroup::GetGroupDepth() const
{
    if (IsGroupGroup()) {
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
    if (obj==nullptr) {
        lk_error << "Cannot add nullptr" << endl;
        return;
    }
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
    else if (obj->InheritsFrom(LKDrawingGroup::Class())) AddGroup((LKDrawingGroup*)obj);
    else if (obj->InheritsFrom(LKDrawing::Class())) AddDrawing((LKDrawing*)obj);
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
    if (drawing==nullptr) {
        lk_error << "Cannot add nullptr drawing" << endl;
        return;
    }
    if (CheckIsDrawingGroup(true))
        AddEntry(drawing);
}

void LKDrawingGroup::AddDrawing(TObjArray* array)
{
    if (array==nullptr) {
        lk_error << "Cannot add nullptr drawing array" << endl;
        return;
    }
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
        else if (countDrawObj<int(titleArray.size()) && countDrawObj<int(drawOptionArray.size())) {
            draw -> Add(obj,drawOptionArray.at(countDrawObj),titleArray.at(countDrawObj));
            countDrawObj++;
        }
        else {
            lk_error << "Flat drawing metadata is missing for object " << obj->GetName() << endl;
        }
    }
}

void LKDrawingGroup::AddGraph(TGraph* graph, TString drawOption, TString title)
{
    if (graph==nullptr) {
        lk_error << "Cannot add nullptr graph" << endl;
        return;
    }
    if (CheckIsDrawingGroup(true)) {
        auto drawing = new LKDrawing(TString("graph_")+graph->GetName());
        drawing -> Add(graph,drawOption,title);
        AddDrawing(drawing);
    }
}

void LKDrawingGroup::AddHist(TH1 *hist, TString drawOption, TString title)
{
    if (hist==nullptr) {
        lk_error << "Cannot add nullptr histogram" << endl;
        return;
    }
    if (CheckIsDrawingGroup(true)) {
        auto drawing = new LKDrawing(TString("draw_")+hist->GetName());
        drawing -> Add(hist,drawOption,title);
        AddDrawing(drawing);
    }
}

int LKDrawingGroup::GetNumDrawings() const
{
    if (IsDrawingGroup())
        return GetEntries();
    return 0;
}

int LKDrawingGroup::GetNumAllGroups() const
{
    int numDrawings = 0;
    if (IsGroupGroup())
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
    if (IsGroupGroup())
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
    if (IsGroupGroup())
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
            auto drawing = (LKDrawing*) At(iDrawing);
            numObjects += drawing -> GetEntries();
        }
    }

    return numObjects;
}

void LKDrawingGroup::AddGroupInStructure(LKDrawingGroup *group)
{
    if (group==nullptr) {
        lk_error << "Cannot add nullptr group to structure" << endl;
        return;
    }

    auto parentName = group->GetParentName();
    if (group->GetGroupLevel()<=1 || parentName.IsNull()) {
        AddGroup(group);
        return;
    }

    auto tokens = parentName.Tokenize(":");
    auto parent = this;
    // The first token names the original file's level-0 group.  `this` is
    // already the level-0 container used while importing that file.
    for (auto index=1; index<tokens->GetEntries(); ++index) {
        TString name = ((TObjString*)tokens->At(index))->GetString();
        name = name.Strip(TString::kBoth);
        if (name.IsNull())
            continue;

        LKDrawingGroup* next = nullptr;
        if (parent->IsGroupGroup()) {
            for (auto childIndex=0; childIndex<parent->GetEntries(); ++childIndex) {
                auto child = (LKDrawingGroup*)parent->At(childIndex);
                if (child!=nullptr && TString(child->GetName())==name) {
                    next = child;
                    break;
                }
            }
        }
        if (next==nullptr)
            next = parent->CreateGroup(name);
        if (next==nullptr) {
            lk_error << "Cannot reconstruct parent path " << parentName << endl;
            delete tokens;
            return;
        }
        parent = next;
    }
    delete tokens;
    parent->AddGroup(group);
}

void LKDrawingGroup::AddGroup(LKDrawingGroup *group)
{
    if (group==nullptr) {
        lk_error << "Cannot add nullptr group" << endl;
        return;
    }
    if (CheckIsGroupGroup(true)) {
        group -> UpdateHierarchyMetadata(fGroupLevel+1,GetFullName());
        AddEntry(group);
    }
}

void LKDrawingGroup::UpdateHierarchyMetadata(int level, TString parentName)
{
    fGroupLevel = level;
    fParentName = parentName;
    if (!IsGroupGroup())
        return;
    auto fullName = GetFullName();
    for (auto index=0; index<GetEntries(); ++index)
        ((LKDrawingGroup*)At(index))->UpdateHierarchyMetadata(level+1,fullName);
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
        if (addToList) AddDrawing(drawing);
        return drawing;
    }
    return (LKDrawing*) nullptr;
}

LKDrawingGroup* LKDrawingGroup::FindGroup(TString name, int option)
{
    if (IsGroupGroup())
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
    if (IsGroupGroup())
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

TObject* LKDrawingGroup::FindObject(const TObject* find) const
{
    if (find==nullptr)
        return nullptr;
    if (IsGroupGroup()) {
        for (auto index=0; index<GetEntries(); ++index) {
            auto group = (LKDrawingGroup*)At(index);
            if (group==find)
                return group;
            auto object = group->FindObject(find);
            if (object!=nullptr)
                return object;
        }
    }
    else {
        for (auto index=0; index<GetEntries(); ++index) {
            auto drawing = (LKDrawing*)At(index);
            if (drawing==find)
                return drawing;
            auto object = drawing->FindObject(find);
            if (object!=nullptr)
                return object;
        }
    }
    return nullptr;
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
