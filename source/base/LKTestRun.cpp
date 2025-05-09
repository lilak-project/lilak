#include "LKTestRun.h"
#include "LKDataViewer.h"
#include "TSystemFile.h"
#include "TSystemDirectory.h"
#include "TObjString.h"

ClassImp(LKTestRun)

LKTestRun::LKTestRun()
:LKVirtualRun("TestRun", "Test run")
{
    fBranchPtr = new TClonesArray*[100];
    CreateParameterContainer();
}

void LKTestRun::Print(Option_t *option) const
{
}

void LKTestRun::Add(LKTask *task)
{
    fTaskArray.push_back(task->GetName());
}

bool LKTestRun::Init()
{
    int countParOrder = 1;
    fPar -> Require("lilak/add",          "LKTask",         "add task or detector class", "t/", countParOrder++);
    fPar -> Require("lilak/print",        "all",            "print after init gen:par:out:in:det:task", "t/", countParOrder++);
    fPar -> Require("lilak/collect_par",  "print",          "file name to write collected parameters. 'print' to print out on screen", "t/", countParOrder++);
    //fPar -> Require("lilak/auto_exit",    "0",              "set(1)/unset(0) auto termination", "t/", countParOrder++);
    fPar -> Require("lilak/run",          "0",              "run [no] after init. Execute all events if [no] is 0", "t/", countParOrder++);
    fPar -> Require("lilak/draw",         "0",              "execute Draw()", "t/", countParOrder++);
    fPar -> Require("lilak/execute",      "0",              "execute event [no] after init", "t/", countParOrder++);

    fPar -> Require("LKRun/Name",         "run",            "name of the run", "",  countParOrder++);
    fPar -> Require("LKRun/RunID",        0,                "run number",      "",  countParOrder++);
    fPar -> Require("LKRun/Tag",          "tag",            "tag",             "t", countParOrder++);
    fPar -> Require("LKRun/OutputPath",   "{lilak_data}",   "path to the output. Default path {lilak_data} is lilak/data/",  "", countParOrder++);
    fPar -> Require("LKRun/InputPath",    "/path/to/in/",   "LKRun will search files from input paths when LKRun/SearchRun", "t/", countParOrder++);
    fPar -> Require("LKRun/SearchRun",    "mfm",            "search input files with LKRun/RunID. opt=mfm: search mfm files, opt=[tag]: search run_runNo.*.[tag].root", "t", countParOrder++);
    fPar -> Require("LKRun/Division",     0,                "division within the run [optional]", "t/", countParOrder++);
    fPar -> Require("LKRun/InputFile",    "to/input/file",  "input file. Cannot be used with LKRun/SearchRun", "t", countParOrder++);
    fPar -> Require("LKRun/FriendFile",   "to/friend/file", "input friend file",                  "t/", countParOrder++);
    fPar -> Require("LKRun/RunIDList",    "1, 2, 3, 4",     "list of run numbers separated by ,", "t/", countParOrder++);
    fPar -> Require("LKRun/RunIDRange",   "1, 10",          "list of run numbers ranged by ,",    "t/", countParOrder++);
    fPar -> Require("LKRun/EntriesLimit",         100000,   "limit number of run entries",        "t/", countParOrder++);
    fPar -> Require("LKRun/EventCountForMessage", 20000,    "",                                   "t", countParOrder++);

    bool useManualInputFiles = false;
    bool useInputFileParameter = false;
    bool useSearchRunParameter = false;
    if (fInputFileNameArray.size()!=0)
    {
        if (fPar->CheckPar("LKRun/InputFile") || fPar->CheckPar("LKRun/SearchRun"))
        {
            lk_error << "Cannot use two parameters LKRun/InputFile and LKRun/SearchRun when input file is manually given by method!" << endl;
            lk_error << "Using user input file ..." << endl;
        }
        useManualInputFiles = true;
    }
    if (fPar->CheckPar("LKRun/InputFile"))
    {
        if (fPar->CheckPar("LKRun/SearchRun")) {
            lk_error << "Cannot use two parameters LKRun/InputFile and LKRun/SearchRun at the same time!" << endl;
            lk_error << "Using LKRun/InputFile ..." << endl;
        }
        useInputFileParameter = true;
    }
    else if (fPar->CheckPar("LKRun/SearchRun"))
        useSearchRunParameter = true;

    TString searchTag;
    TString searchOption2;

    TString collecteParAndPrintTo = "";

    Int_t idxInput = 1;
    if (fInputFileName.IsNull())
    {
        if (useManualInputFiles)
        {}
        else if (useInputFileParameter)
        {
            LKParameterContainer* fileNameArray = fPar -> CreateMultiParContainer("LKRun/InputFile");
            TIter next(fileNameArray);
            LKParameter *parameter = nullptr;
            while ((parameter = (LKParameter*) next())) {
                auto fileName = parameter -> GetValue();
                if (!fileName.IsNull())
                    fInputFileNameArray.push_back(fileName);
            }
        }
        else if (useSearchRunParameter)
        {
            LKParameterContainer* inputPathArray = fPar -> CreateMultiParContainer("LKRun/InputPath");
            TIter next(inputPathArray);
            LKParameter *parameter = nullptr;
            while ((parameter = (LKParameter*) next())) {
                auto pathName = parameter -> GetValue();
                if (!pathName.IsNull())
                    fInputPathArray.push_back(pathName);
            }

            auto numSearchOptions = fPar -> GetParN("LKRun/SearchRun");
            if (numSearchOptions==1)
                searchTag = fPar -> GetParString("LKRun/SearchRun");
            if (numSearchOptions==2) {
                searchTag = fPar -> GetParString("LKRun/SearchRun",0);
                searchOption2 = fPar -> GetParString("LKRun/SearchRun",1);
            }
            if (fRunIDList.size()>0)
            {
                for (auto runID : fRunIDList)
                {
                    vector<TString> inputFiles = SearchRunFiles(runID,searchTag, searchOption2);
                    for (auto fileName : inputFiles)
                        fInputFileNameArray.push_back(fileName);
                }
            }
            else {
                vector<TString> inputFiles = SearchRunFiles(fRunID,searchTag, searchOption2);
                for (auto fileName : inputFiles)
                    fInputFileNameArray.push_back(fileName);
            }
        }

        if (searchTag!="mfm" && fInputFileNameArray.size()>0) {
            fInputFileName = fInputFileNameArray[0];
            idxInput = 1;
        }
    }

    if (!collecteParAndPrintTo.IsNull()) {
        fPar -> PrintCollection(collecteParAndPrintTo);
        fPar -> SetCollectParameters(false);
    }

    return true;
}

void LKTestRun::InitAndCollectParameters(TString fileName)
{
    fPar -> SetCollectParameters(true);
    Init();
    fPar -> PrintCollection(fileName);
}

bool LKTestRun::RegisterObject(TString name, TObject *obj)
{
    return true;
}

TClonesArray* LKTestRun::RegisterBranchA(TString name, const char* className, Int_t size, bool persistent)
{
    if (fBranchPtrMap[name] != nullptr) {
        lk_error << "The branch with name " << name << " already exist!" << endl;
        return (TClonesArray*) nullptr;
    }

    TClonesArray *array = new TClonesArray(className, size);
    array -> SetName(name);

    TString persistencyParName = TString("persistency/") + name;
    TString persistencyMessage;
    if (fPar -> CheckPar(persistencyParName + " true")) {
        persistent = fPar -> GetParBool(persistencyParName);
        if (persistent)
            persistencyMessage = TString("(persistent by par. ") + persistencyParName + ")";
        else
            persistencyMessage = TString("(temporary by par. ") + persistencyParName + ")";
    }
    else {
        if (persistent)
            persistencyMessage = "(persistent)";
        else
            persistencyMessage = "(temporary)";
    }

    fBranchPtr[fCountBranches] = array;
    fBranchPtrMap[name] = fBranchPtr[fCountBranches];
    fBranchNames.push_back(name);
    fCountBranches++;

    lk_info << "Output branch " << name << " " << persistencyMessage << endl;

    return array;
}

TString LKTestRun::GetBranchName(Int_t idx) const
{
    TString branchName = fBranchNames[idx];
    return branchName;
}

TClonesArray *LKTestRun::GetBranchA(Int_t idx)
{
    auto dataContainer = fBranchPtr[idx];
    if (dataContainer!=nullptr) // && dataContainer -> InheritsFrom("TClonesArray"))
    {
        return (TClonesArray *) dataContainer;
    }
    return (TClonesArray *) nullptr;
}

TClonesArray *LKTestRun::GetBranchA(TString name, bool complainIfDoNotExist)
{
    fKeepBranchArray.push_back(name);
    fKeepClassArray.push_back("");

    auto dataContainer = fBranchPtrMap[name];
    if (dataContainer==nullptr) {
        if (complainIfDoNotExist)
            lk_error << "Branch " << name << " does not exist!" << endl;
    }
    //else if (dataContainer->InheritsFrom("TClonesArray")==false)
    //    lk_error << "Branch " << name << " is not TClonesArray object!" << endl;
    else
        return (TClonesArray *) dataContainer;
    return (TClonesArray *) nullptr;
}

TClonesArray *LKTestRun::KeepBranchA(TString name)
{
    fKeepBranchArray.push_back(name);
    fKeepClassArray.push_back("");
    TClonesArray* dataContainer = GetBranchA(name);
    return dataContainer;
}

void LKTestRun::AddDetector(LKDetector *detector)
{
    fDetectorArray.push_back(detector->GetName());
}

void LKTestRun::AddDetectorPlane(LKDetectorPlane *plane)
{
    fDetectorPlaneArray.push_back(plane->GetName());
}

void LKTestRun::Draw(Option_t* option)
{
    if (fTopDrawingGroup->GetEntries()==0) {
        lk_warning << "Not using drawings..." << endl;
        return;
    }
    if (fDataViewer!=nullptr && fDataViewer->IsActive()) {
        lk_warning << "Viewer already running" << endl;
        return;
    }
    fDataViewer = fTopDrawingGroup -> CreateViewer();
    fDataViewer -> SetRun(this);
    fTopDrawingGroup -> Draw(TString(option));
}

LKDrawingGroup* LKTestRun::GetTopDrawingGroup()
{
    if (fTopDrawingGroup==nullptr)
        fTopDrawingGroup = new LKDrawingGroup("TestRun");

    return fTopDrawingGroup;
}

vector<TString> LKTestRun::SearchRunFiles(int searchRunNo, TString searchTag, TString searchOption2)
{
    //fInputPathArray.push_back("/home/cens-alpha-00/data/ganacq_manip/test/acquisition/run");
    //fInputPathArray.push_back("/root/lilak/stark/macros/data");

    vector<TString> matchingFiles;
    TSystemFile *sysFile = nullptr;
    int countPath = 0;
    for (auto path : fInputPathArray)
    {
        lk_info << "Looking for " << searchRunNo << "(" << searchTag << ") in " << path << endl;
        TIter nextFile(TSystemDirectory(Form("search_path_%d",countPath),path).GetListOfFiles());
        while ((sysFile=(TSystemFile*)nextFile()))
        {
            TString fileName = sysFile -> GetName();
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (searchTag=="mfm")
            {
                if (sysFile->IsDirectory()==false && fileName.Index("run_")==0 && fileName.Sizeof()>=32)
                {
                    int runNo = TString(fileName(5,4)).Atoi();
                    int division = (fileName.Sizeof()>32) ? TString(fileName(32,fileName.Sizeof()-32-1)).Atoi() : 0;
                    bool matched = false;
                    if (runNo==searchRunNo)
                        matched = true;
                    if (!searchOption2.IsNull()) {
                        if (searchOption2.IsDigit())
                            searchOption2 = Form(".%s",searchOption2.Data());
                        matched = fileName.EndsWith("s");
                    }
                    if (matched) matchingFiles.push_back(path+"/"+fileName);
                }
            }
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
            else if (!searchTag.IsNull())
            {
                if (sysFile->IsDirectory()==false && fileName.Index((fRunName+"_"))==0 && fileName.EndsWith(searchTag+".root"))
                {
                    int runNo = TString(fileName(fRunName.Sizeof(),4)).Atoi();
                    int division = 0;
                    auto tokens = fileName.Tokenize(".");
                    /*
                    if (tokens->GetEntries()>3) {
                        TString divString = TString(((TObjString*)tokens->At(1))->GetString());
                        if (divString[0]=='d') {
                            divString.Remove(0,1);
                            if (division.IsDec()) {
                                division = divString.Atoi();
                            }
                        }
                    }
                    */
                    int numTokens = tokens->GetEntries();
                    if (numTokens>=3)
                    {
                        TString tag = TString(((TObjString*)tokens->At(numTokens-2))->GetString());
                        if (runNo==searchRunNo && tag==searchTag)
                            matchingFiles.push_back(path+"/"+fileName);
                    }
                }
            }
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////
        }
    }

    if (searchTag=="mfm")
    {
        auto customComparator = [](const TString &a, const TString &b) {
            if (a[a.Length()-1]=='s') return true;
            if (b[b.Length()-1]=='s') return false;
            TString fileNumberA = a(a.Last('.') + 1, a.Length());
            TString fileNumberB = b(b.Last('.') + 1, b.Length());
            return std::stoi(fileNumberA.Data()) < std::stoi(fileNumberB.Data());
        };

        sort(matchingFiles.begin(),matchingFiles.end(),customComparator);
    }

    for (auto file : matchingFiles)
        lk_info << "Found " << file << endl;

    return matchingFiles;
}
