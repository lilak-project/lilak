#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdlib.h>

#include "TROOT.h"
#include "TSystem.h"
#include "TFormula.h"
#include "TObjString.h"
#include "TDirectory.h"
#include "TApplication.h"
#include "TSystemDirectory.h"
#include "TList.h"
#include "TFile.h"

#include "LKCompiled.h"
#include "LKParameter.h"
#include "LKParameterContainer.h"

using namespace std;

ClassImp(LKParameterContainer)

LKParameterContainer::LKParameterContainer()
    :TObjArray()
{
    fName = "ParameterContainer";
}

LKParameterContainer::LKParameterContainer(const char *parName)
{
    AddFile(TString(parName));
}

void LKParameterContainer::ProcessTypeError(TString name, TString value, TString type) const {
    lk_error << "parameter " << name << " = " << value << " is not convertable to " << type << endl;
    gApplication -> Terminate();
}

bool LKParameterContainer::CheckFormulaValidity(TString formula, bool isInt) const
{
    if (isInt && formula.Index(".")>=0)
        return false;

    if (formula.Index("+")<0
      &&formula.Index("-")<0
      &&formula.Index("/")<0
      &&formula.Index("*")<0)
        return false;

    TString formula2 = formula;
    formula2.ReplaceAll("+"," ");
    formula2.ReplaceAll("-"," ");
    formula2.ReplaceAll("/"," ");
    formula2.ReplaceAll("*"," ");
    formula2.ReplaceAll("."," ");
    formula2.ReplaceAll("e","1");
    formula2.ReplaceAll("E","1");

    if (!formula2.IsDigit())
        return false;

    return true;
}

double LKParameterContainer::Eval(TString formula) const
{
    return TFormula("formula",formula).Eval(0);
}

void LKParameterContainer::SaveAs(const char *fileName, Option_t *) const
{
    TString fileName0(fileName);
    if (fileName0.Index(".") < 0)
        fileName0 = fileName0 + ".mac";
    Print(fileName0);
}

void LKParameterContainer::Recompile()
{
    R__COLLECTION_READ_LOCKGUARD(ROOT::gCoreMutex);
    TIter iterator(this);
    LKParameter *parameter = nullptr;
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        TString value = parameter -> GetRaw();
        ReplaceVariables(value);
        parameter -> SetValue(value);
    }
}

bool LKParameterContainer::IsEmpty() const
{
    if (GetEntries()>0)
        return false;
    return true;
}

int LKParameterContainer::FindAndRetrieveColumnValue(TString fileName, int searchColumn, TString searchValue, int getColumn, TString &getValue)
{
    std::ifstream file(fileName.Data());

    if (!file.is_open()) {
        lk_error << "Could not open file " << fileName << std::endl;
        return 1;
    }

    TString line;
    while (line.ReadLine(file)) {
        std::vector<TString> columns;
        std::istringstream stream(line.Data());
        TString token;

        while (true) {
            if (stream.eof()) break;
            stream >> token;
            columns.push_back(token);
        }

        if (searchColumn >= columns.size() || getColumn >= columns.size()) {
            lk_error << "Column index out of bounds" << std::endl;
            return 2;
        }

        if (columns[searchColumn] == searchValue) {
            getValue = columns[getColumn];
            return 0;
        }
    }

    return 3;
}

void LKParameterContainer::ReplaceEnvVariables(TString &valInput)
{
    int trySpTypes[] = {0,1};
    int isp = INT_MAX;
    int type = -1;
    int idx = 0;

    while (true)
    {
        isp = INT_MAX;
        type = -1;
        for (int trySpType : trySpTypes)
        {
            if      (trySpType==0) idx = valInput.Index( "e{");
            else if (trySpType==1) idx = valInput.Index("cv{");
            if (idx>=0 && idx<isp) {
                isp = idx;
                type = trySpType;
            }
        }
        if (isp==INT_MAX)
            break;

        int fsp = valInput.Index("}",1,isp,TString::kExact);

        if (type==0)
        {
            TString replaceFrom = TString(valInput(isp+2,fsp-isp-2));
            TString replaceFrom2 = replaceFrom;
            replaceFrom2.ToLower();
            TString replaceTo;
            replaceTo = getenv(replaceFrom);
            valInput.Replace(isp,fsp-isp+1,replaceTo);
        }
        else if (type==1)
        {
            int isp1 = valInput.Index("{",1,isp+3,TString::kExact);
            while (isp1>=0 && isp1<fsp)
            {
                TString replaceFrom = TString(valInput(isp1+2,fsp-isp1-2));
                TString replaceFrom2 = replaceFrom;
                replaceFrom2.ToLower();
                TString replaceTo;
                replaceTo = getenv(replaceFrom);
                valInput.Replace(isp1,fsp-isp1+1,replaceTo);
                fsp = valInput.Index("}",1,isp+3,TString::kExact);
                isp1 = valInput.Index("{",1,isp+3,TString::kExact);
            }
            valInput.Replace(isp,fsp-isp+1,"lk");
        }
    }
}

void LKParameterContainer::ReplaceVariables(TString &valInput)
{
    ReplaceEnvVariables(valInput);

    int ipar = valInput.Index("{");
    while (ipar>=0) {
        int fpar = valInput.Index("}",1,ipar,TString::kExact);
        int mpar = valInput.Index(":",1,ipar,TString::kExact);
        TString parName2 = valInput(ipar+1,fpar-ipar-1);
        int width = 0;
        bool setWidth = false;
        bool fillZeros = false;
        bool alignLeft = false;
        if (mpar>0&&mpar<fpar) {
            parName2 = valInput(ipar+1,mpar-ipar-1);
            TString parOption = valInput(mpar+1,fpar-mpar-1);
            if (parOption.Index("<")>=0) {
                parOption.ReplaceAll("<","");
                alignLeft = true;
            }
            if (parOption.Index(">")>=0) {
                parOption.ReplaceAll(">","");
                alignLeft = false;
            }
            if (parOption.IsDec()) {
                setWidth = true;
                if (parOption[0]=='0') {
                    fillZeros = true;
                    parOption = parOption(1,parOption.Sizeof()-2);
                }
                width = parOption.Atoi();
            }
        }
        TString replaceTo;
        if (parName2=="lilak_mainproject_version") replaceTo = LILAK_MAINPROJECT_VERSION;
        else if (parName2=="lilak_mainproject_hash") replaceTo = LILAK_MAINPROJECT_HASH;
        else if (parName2=="lilak_version" ) replaceTo = LILAK_VERSION;
        else if (parName2=="lilak_hash"    ) replaceTo = LILAK_HASH;
        else if (parName2=="lilak_hostname") replaceTo = LILAK_HOSTNAME;
        else if (parName2=="lilak_username") replaceTo = LILAK_USERNAME;
        else if (parName2=="lilak_hostuser") replaceTo = LILAK_HOSTUSER;
        else if (parName2=="lilak_home"    ) replaceTo = LILAK_PATH;
        else if (parName2=="lilak_path"    ) replaceTo = LILAK_PATH;
        else if (parName2=="lilak_version" ) replaceTo = LILAK_VERSION;
        else if (parName2=="lilak_data"    ) replaceTo = TString(LILAK_PATH)+"/data";
        else if (parName2=="lilak_common"  ) replaceTo = TString(LILAK_PATH)+"/common";
        else if (parName2.EndsWith("]")) {
            int idx = TString(parName2(parName2.Index("[")+1,parName2.Index("]")-parName2.Index("[")-1)).Atoi();
            parName2 = parName2(0,parName2.Index("["));
            replaceTo = GetParString(parName2,idx);
        }
        else {
            replaceTo = GetParString(parName2);
        }
        if (setWidth) {
            int replaceWidth = replaceTo.Sizeof() - 1;
            //lk_error << "!!" << " width is " << width << ", replaced width is " << replaceWidth << endl;
            if (width>replaceWidth) {
                int dWidth = width - replaceWidth;
                TString addSpace = " ";
                if (fillZeros) addSpace = "0";
                TString fill;
                if (alignLeft) {
                    for (auto i=0; i<dWidth; ++i)
                        fill += addSpace;
                    replaceTo = replaceTo + fill;
                }
                else {
                    for (auto i=0; i<dWidth; ++i)
                        fill += addSpace;
                    replaceTo = fill + replaceTo;
                }
            }
        }
        valInput.Replace(ipar,fpar-ipar+1,replaceTo);
        ipar = valInput.Index("{");
    }

    valInput.ReplaceAll("kWhite"  ,"0");
    valInput.ReplaceAll("kBlack"  ,"1");
    valInput.ReplaceAll("kGray"   ,"920");
    valInput.ReplaceAll("kRed"    ,"632");
    valInput.ReplaceAll("kGreen"  ,"416");
    valInput.ReplaceAll("kBlue"   ,"600");
    valInput.ReplaceAll("kYellow" ,"400");
    valInput.ReplaceAll("kMagenta","616");
    valInput.ReplaceAll("kCyan"   ,"432");
    valInput.ReplaceAll("kOrange" ,"800");
    valInput.ReplaceAll("kSpring" ,"820");
    valInput.ReplaceAll("kTeal"   ,"840");
    valInput.ReplaceAll("kAzure"  ,"860");
    valInput.ReplaceAll("kViolet" ,"880");
    valInput.ReplaceAll("kPink"   ,"900");

    if (CheckFormulaValidity(valInput)) {
        valInput = Form("%f",Eval(valInput));
        while (valInput.Index(".")>=0 && valInput.EndsWith("0") && valInput.Index(".")<valInput.Sizeof()-2) {
            valInput.Remove(valInput.Sizeof()-2,1);
        }
        if (valInput.EndsWith("."))
            valInput.Remove(valInput.Sizeof()-2,1);
    }
}

Int_t LKParameterContainer::AddFile(TString parName, TString fileName)
{
    ReplaceEnvVariables(fileName);

    TString fileNameFull;

    if (fileName[0]=='!')
        fileName = fileName(1, fileName.Sizeof()-2);

    bool existFile = false;

    if (fileName[0]=='/' || fileName[0]=='$' || fileName =='~'|| fileName[0]=='.') {
        fileNameFull = fileName;
        if (!TString(gSystem -> Which(".", fileNameFull.Data())).IsNull())
            existFile = true;
    }
    else
    {
        fileNameFull = TString(gSystem -> Getenv("PWD")) + "/" + fileName;
        if (!TString(gSystem -> Which(".", fileNameFull.Data())).IsNull()) {
            existFile = true;
        }
        else
        {
            //fileNameFull = TString(gSystem -> Getenv("LILAK_PATH")) + "/input/" + fileName;
            fileNameFull = TString(LILAK_PATH) + "/input/" + fileName;
            if (!TString(gSystem -> Which(".", fileNameFull.Data())).IsNull())
                existFile = true;
        }
    }

    if (!existFile) {
        lk_error << "Parameter file " << fileNameFull << " does not exist!" << endl;
        return 0;
    }

    fileNameFull.ReplaceAll("//","/");

    lk_info << "Adding parameter file " << fileNameFull << endl;

    if (parName.IsNull())
        parName = Form("input_par_%d", fNumInputFiles);

    fNumInputFiles++;
    //LKParameter *parFile;
    //if (addFilePar)
    //    parFile = SetParFile(fileNameFull);
    //else {
        SetLineComment(parName + " " + fileNameFull);
    //}

    ifstream file(fileNameFull);

    Int_t countParameters = 0;

    if (fileNameFull.EndsWith(".json")) {
#ifdef LILAK_BUILD_JSONCPP
        file >> fJsonValues;
        countParameters = AddJsonTree(fJsonValues);
#else
        lk_error << "jsoncpp is not built. Configure build with BUILD_JSONCPP ON" << endl;
#endif
    }
    else {
        string line;

        while (getline(file, line)) {
            int RVAddLine = AddLine(line);
            if (RVAddLine==1)
                countParameters++;
        }

        if (countParameters == 0) {
            //if (addFilePar)
            //    this -> Remove(parFile);
            fNumInputFiles--;
        }

        //parName.Replace(0,11,"<<");
        SetLineComment(Form("%s %d parameters were added",parName.Data(),countParameters));
    }

    Recompile();

    return countParameters;
}

Int_t LKParameterContainer::AddParameterContainer(LKParameterContainer *parc)
{
    lk_info << "Adding parameter container " << parc -> GetName() << endl;

    TString parName = Form("%d", fNumInputFiles);
    fNumInputFiles++;
    auto parameter_parc = SetParCont(parc->GetName());

    Int_t countParameters = 0;
    //Int_t countSameParameters = 0;

    TIter iterator(parc);
    LKParameter *parameter;
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        if (parameter -> IsLineComment()) {
            SetLineComment(parameter->GetComment());
            continue;
        }
        TString name = parameter -> GetName();
        LKParameter *found = FindPar(name);
        if (found != nullptr) {
            lk_warning << "Parameter " << name << " already exist!" << endl;
            continue;
        }
        else {
            SetPar(parameter->GetName(),parameter->GetRaw(),parameter->GetValue(),parameter->GetComment(),parameter->GetType());
            ++countParameters;
        }
    }

    if (countParameters == 0) {
        this -> Remove(parameter_parc);
        fNumInputFiles--;
    }

    Recompile();

    return countParameters;
}

bool LKParameterContainer::SearchAndAddPar(TString dirName)
{
    if (dirName.IsNull())
        dirName = gSystem -> pwd();

    lk_info << "Looking for parameter file in " << dirName << endl;

    TSystemDirectory sysDir(dirName, dirName);
    TList *listFiles = sysDir.GetListOfFiles();
    vector<TString> listDir;

    if (listFiles)
    {
        TSystemFile *sysFile;
        TString fileName;
        TIter next(listFiles);

        while ((sysFile=(TSystemFile*)next()))
        {
            fileName = sysFile -> GetName();
            if (sysFile->IsDirectory()) {
                if (fileName.Index("input")==0
                        ||fileName.Index("conf")==0
                        ||fileName.Index("json")==0
                        ||fileName.Index("par")==0
                        ||fileName.Index("mac")==0
                   ) {
                    listDir.push_back(fileName);
                }
                listFiles -> Remove(sysFile);
            }
            else if (  fileName.EndsWith(".conf")
                    || fileName.EndsWith(".json")
                    || fileName.EndsWith(".par")
                    || fileName.EndsWith(".mac")
                    ) {
            }
            else {
                listFiles -> Remove(sysFile);
            }
        }

        int countAll = 0;
        int countDir = 0;
        int countFile = 0;

        TIter next2(listFiles);
        e_info << "* Select index from the below list: " << endl;
        e_info << "  " << "0) Exit" << endl;
        while ((sysFile=(TSystemFile*)next2()))
        {
            countAll++;
            countFile++;
            fileName = sysFile -> GetName();
            e_info << "  " << countAll << ") " << fileName << endl;
        }

        int numDir = listDir.size();
        for (auto iDir=0; iDir<numDir; ++iDir) {
            countAll++;
            countDir++;
            TString dirName2 = dirName + "/" + listDir[iDir];
            e_info << "  " << countAll << ") " << dirName2 << endl;
        }

        TString strSelected;
        e_info << "Enter index: ";
        cin >> strSelected;

        if (strSelected.IsDigit()) {
            int idxSelected = strSelected.Atoi();
            if (idxSelected==0) {
                e_info << "Exit" << endl;
                return false;
            }
            else if (idxSelected>0 && idxSelected <= countAll) {
                if (idxSelected <= countFile) {
                    TString nameSelected = listFiles -> At(idxSelected-1) -> GetName();
                    e_info << nameSelected << " selected!" << endl;
                    LKParameterContainer::AddFile(nameSelected);
                    return true;
                }
                else {
                    int idxDir = countAll - countFile - 1;
                    TString dirName2 = dirName + "/" + listDir[idxDir];
                    return SearchAndAddPar(dirName2);
                }
            }
            else {
                e_error << "Invalid index selected." << endl;
                return false;
            }
        }
        else {
            e_error << "Invalid input." << endl;
            return false;
        }
    }
    e_warning << "No parameter files found in current directory" << endl;

    int numDir = listDir.size();
    for (auto iDir=0; iDir<numDir; ++iDir) {
        TString dirName2 = dirName + "/" + listDir[iDir];
        if (SearchAndAddPar(dirName2))
            return true;
    }

    return false;
}

#ifdef LILAK_BUILD_JSONCPP
Int_t LKParameterContainer::AddJsonTree(const Json::Value &jsonTree, TString treeName)
{
    Int_t count_par = 0;
    for (Json::ValueConstIterator it = jsonTree.begin(); it != jsonTree.end(); it++) {
        auto branch = jsonTree[it.name()];

        TString parName;
        if (treeName.IsNull())
            parName = TString(it.name());
        else
            parName = treeName + "/" + TString(it.name());

        // nullValue
        // intValue
        // uintValue
        // realValue
        // stringValue
        // booleanValue
        // arrayValue
        // objectValue
        if (branch.type()==Json::ValueType::stringValue || branch.type()==Json::ValueType::booleanValue) {
            AddPar(parName, branch.asString());
            count_par = count_par + 1;
        }
        else if (branch.type()==Json::ValueType::intValue || branch.type()==Json::ValueType::uintValue) {
            AddPar(parName, branch. asInt());
            count_par = count_par + 1;
        }
        else if (branch.type()==Json::ValueType::realValue) {
            AddPar(parName, branch. asDouble());
            count_par = count_par + 1;
        }
        else if (branch.type()==Json::ValueType::arrayValue) {
            int nValues = branch.size();
            TString valueAll = "";
            for (auto iVal=0; iVal<nValues; ++iVal) {
                auto valueString = branch.get(iVal,Json::Value(-999)).asString();
                valueAll = valueAll + valueString + " ";
            }
            SetPar(parName, valueAll);
        }
        else if (branch.type()==Json::ValueType::objectValue) {
            Int_t count_par2 = AddJsonTree(*it, parName);
            count_par = count_par + count_par2;
        }
    }

    return count_par;
}
#endif

void LKParameterContainer::Print(Option_t *option) const
{
    TString printOptions(option);

    if (printOptions.Index("raw")>=0) {
        TObjArray::Print();
        return;
    }

    bool evaluatePar = false;
    bool showLineComment = true;
    bool showParComments = true;
    bool showLineIndex = true;
    bool printToScreen = true;
    bool printToFile = false;
    ofstream fileOut;

    if (printOptions.Index("all")>=0) {
        evaluatePar = true;
        showLineComment = true;
        showParComments = true;
        printToScreen = true;
    }
    else {
        if (printOptions.Index("!eval")>=0) { evaluatePar = false; printOptions.ReplaceAll("!eval", ""); }
        if (printOptions.Index( "eval")>=0) { evaluatePar = true;  printOptions.ReplaceAll("eval", ""); }

        if (printOptions.Index("!line#")>=0) { showLineComment = false; printOptions.ReplaceAll("!line#", ""); }
        if (printOptions.Index( "line#")>=0) { showLineComment = true;  printOptions.ReplaceAll("line#", ""); }

        if (printOptions.Index("!par#")>=0) { showParComments = false; printOptions.ReplaceAll("!par#", ""); }
        if (printOptions.Index( "par#")>=0) { showParComments = true;  printOptions.ReplaceAll("par#", ""); }

        if (printOptions.Index("!idx")>=0) { showLineIndex = false; printOptions.ReplaceAll("!idx", ""); }
        if (printOptions.Index( "idx")>=0) { showLineIndex = true;  printOptions.ReplaceAll("idx", ""); }
    }

    TString fileName = printOptions;
    if (fileName.IsNull()) {
        printToScreen = true;
    }
    else if (fileName.Index(".")>0) {
        printToFile = true;
        printToScreen = false;
    }

    if (printToScreen) {
        e_cout << endl;
        lk_info << "Parameter Container " << fName << endl;
    }

    if (printToFile)
    {
        lk_info << "Writting " << fileName << endl;
        fileOut.open(fileName);
        fileOut << "# " << fileName << " created from LKParameterContainer::Print" << endl;
    }

    int parNumber = 0;
    TIter iterator(this);
    LKParameter *parameter;
    TString preGroup = "";
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        TString parName = parameter -> GetName();
        TString parGroup = parameter -> GetGroup();
        TString parRaw = parameter -> GetRaw();
        TString parValue = parameter -> GetValue();
        TString parComment = parameter -> GetComment();

        if (!evaluatePar)
            parValue = parRaw;

        bool addEmptyLine = false;
        //if (preGroup!="" && preGroup!=parGroup)
            //addEmptyLine = true;

        bool isLineComment = false;
        bool isParameter = true;
        if (parameter -> IsLineComment()) {
            isLineComment = true;
            isParameter = false;
        }
        else {
            isLineComment = false;
            isParameter = true;
        }

        if (isParameter) {
            if (!showParComments)
                parComment = "";
            else if (!parComment.IsNull())
                parComment = TString(" # ") + parComment;
        }

        parName = parameter -> GetTypeHeader() + parName;

        int nwidth = 20;
             if (parName.Sizeof()>60) nwidth = 70;
        else if (parName.Sizeof()>50) nwidth = 60;
        else if (parName.Sizeof()>40) nwidth = 50;
        else if (parName.Sizeof()>30) nwidth = 40;
        //else if (parName.Sizeof()>20) nwidth = 30;
        else                          nwidth = 30;

        int vwidth = 5;
             if (parValue.Sizeof()>60) vwidth = 70;
        else if (parValue.Sizeof()>50) vwidth = 60;
        else if (parValue.Sizeof()>40) vwidth = 50;
        else if (parValue.Sizeof()>30) vwidth = 40;
        else if (parValue.Sizeof()>20) vwidth = 30;
        else if (parValue.Sizeof()>10) vwidth = 20;
        else if (parValue.Sizeof()>5)  vwidth = 10;
        else                           vwidth = 5;

        if (addEmptyLine) {
            if (printToScreen) e_cout << endl;
            if (printToFile)   fileOut << endl;
        }
        if (preGroup!=parGroup && printToFile)
            fileOut << endl;

        if (isLineComment && showLineComment) {
            if (showLineComment) {
                if (printToScreen) e_cout << "# " << parComment << endl;
                //if (printToFile)   fileOut << "# " << parComment << endl;
                if (printToFile)   fileOut << parameter->GetLine(TString(option)) << endl;
            }
        }
        else if (isParameter) {
            if (printToScreen) {
                //if (showLineIndex) e_list(parNumber) << left << setw(nwidth) << parName << " " << setw(vwidth) << parValue << " " << parComment << endl;
                if (showLineIndex) e_list(parNumber) << left << parameter->GetLine(TString(option)) << endl;
                else               e_cout            << left << parameter->GetLine(TString(option)) << endl;
                //else               e_cout << left << setw(nwidth) << parName << " " << setw(vwidth) << parValue << " " << parComment << endl;
            }
            //if (printToFile) fileOut << left << setw(nwidth) << parName << " " << setw(vwidth) << parValue << " " << parComment << endl;
            if (printToFile) fileOut << parameter->GetLine(TString(option)) << endl;
        }

        preGroup = parGroup;
        if (isParameter)
            parNumber++;
    }

    if (printToScreen)
        lk_info << "End of Parameter Container " << fName << endl;
        e_cout << endl;

    //if (printToFile) fileOut << endl;
}

LKParameterContainer *LKParameterContainer::CloneParameterContainer(TString name, bool addTemporary) const
{
    LKParameterContainer *new_collection = new LKParameterContainer();
    if (name.IsNull()) name = "ParameterContainer_Clone";
    new_collection -> SetName(name);

    TIter iterator(this);
    LKParameter *parameter;
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        if (!addTemporary && parameter->IsTemporary()) {
            new_collection -> SetLineComment(Form("t %s",parameter->GetLine().Data()));
        }
        else if (parameter->IsLineComment()) {
            auto comment = parameter->GetComment();
            new_collection -> SetLineComment(Form("> %s",parameter->GetComment().Data()));
        }
        else
            new_collection -> Add(parameter);
    }

    new_collection -> Sort();

    return new_collection;
}

Int_t LKParameterContainer::AddLine(std::string ssline)
{
#ifdef DEBUG_ADDPAR
    e_cout << endl;
    lk_info << ssline << endl;
#endif
    if (ssline.find_first_not_of(" \t\n\v\f\r") == std::string::npos)
        return 0;

    size_t endpos = ssline.find_last_not_of(" \t"); // Includes tabs as well
    if (std::string::npos != endpos) {
        ssline.erase(endpos+1);
    }

    if (ssline.find("#") == 0) {
        TString line2 = TString(ssline);
        line2 = line2(1,line2.Sizeof()-2);
        SetLineComment(line2);
        return 3;
    }

    int sizeOfHead = 0;

    // group -------------------------------------------
    TString line = ssline;
    int currentTabSize = 0;
    while (true) {
        if (line[currentTabSize]==' ') ++currentTabSize;
        else if (line[currentTabSize]=='#') {
            TString line2 = TString(ssline);
            line2 = line2(currentTabSize+1,line2.Sizeof()-2-currentTabSize);
            SetLineComment(line2);
            return 3;
        }
        else break;
    }
#ifdef DEBUG_ADDPAR
    lk_debug << "currentTabSize = " << currentTabSize << endl;
#endif
    if (currentTabSize==0) {
#ifdef DEBUG_ADDPAR
    lk_debug << "currentTabSize==0" << endl;
#endif
        fPreviousTabSize = 0;
        fCurrentGroupName = "";
        fTabSizeArray.clear();
        fGroupNameArray.clear();
    }
    else if (currentTabSize>0 && currentTabSize==fPreviousTabSize)
    {
#ifdef DEBUG_ADDPAR
        lk_debug << "currentTabSize>0 && currentTabSize==fPreviousTabSize" << endl;
        lk_debug << fGroupNameArray.size() << " " << fTabSizeArray.size() << endl;
#endif
        if (fGroupNameArray.size()>0) fCurrentGroupName = fGroupNameArray.back();
        // this is a case when privous defined group name was just parameter without value
        if (fGroupNameArray.size()==fTabSizeArray.size()+1) {
            fGroupNameArray.pop_back();
            if (fGroupNameArray.size()>0) fCurrentGroupName = fGroupNameArray.back();
            else fCurrentGroupName = "";
        }
    }
    else if (currentTabSize>0 && currentTabSize<fPreviousTabSize) {
#ifdef DEBUG_ADDPAR
    lk_debug << "currentTabSize>0 && currentTabSize<fPreviousTabSize" << endl;
    lk_debug << fGroupNameArray.size() << " " << fTabSizeArray.size() << endl;
#endif
        while (true) {
            fPreviousTabSize = 0;
            fCurrentGroupName = "";
            if (fTabSizeArray.size()>0) {
#ifdef DEBUG_ADDPAR
                lk_debug << "fTabSizeArray size   = " << fTabSizeArray.size() << endl;
                lk_debug << "fGroupNameArray size = " << fGroupNameArray.size() << endl;
                lk_debug << "fPreviousTabSize     = " << fPreviousTabSize << endl;
                lk_debug << "fCurrentGroupName    = " << fCurrentGroupName  << endl;
#endif
                int tabSize = fTabSizeArray.back();
                TString groupName = fGroupNameArray.back();
#ifdef DEBUG_ADDPAR
                lk_debug << "tabSize   = " << tabSize << endl;
                lk_debug << "groupName = " << groupName  << endl;
#endif
                fGroupNameArray.pop_back();
                fTabSizeArray.pop_back();
                if (tabSize==currentTabSize) {
                    fPreviousTabSize = tabSize;
                    fCurrentGroupName = groupName;
                    break;
                }
            }
            else
                break;
        }
    }
    else if (currentTabSize>0 && currentTabSize>fPreviousTabSize) {
#ifdef DEBUG_ADDPAR
    lk_debug << "currentTabSize>0 && currentTabSize>fPreviousTabSize" << endl;
    lk_debug << fGroupNameArray.size() << " " << fTabSizeArray.size() << endl;
#endif
        if (fGroupNameArray.size()==fTabSizeArray.size()+1) {
#ifdef DEBUG_ADDPAR
            lk_debug << "first element of the current group" << endl;
#endif
            // This is the first element of current group
            fTabSizeArray.push_back(currentTabSize);
            fPreviousTabSize = currentTabSize;
            fCurrentGroupName = fGroupNameArray.back();
        }
        else {
            lk_error << "Error at : " << line << endl;
            lk_error << "Current tab-size is larger than previous, but new tab-size already exist!" << endl;
            gApplication -> Terminate();
        }
    }
    else {
        lk_error << "Error at : " << line << endl;
        lk_error << "Current tab-size = " << currentTabSize << ", previous tab-size = " << fPreviousTabSize << endl;
        gApplication -> Terminate();
    }
    sizeOfHead += currentTabSize;
#ifdef DEBUG_ADDPAR
    lk_debug << "sizeOfHead = " << sizeOfHead << endl;
    lk_debug << "fPreviousTabSize  = " << fPreviousTabSize  << endl;
    lk_debug << "fCurrentGroupName = " << fCurrentGroupName << endl;
#endif

    // name -------------------------------------------
    istringstream ss(ssline);
    TString parName;
    ss >> parName;
    sizeOfHead += parName.Sizeof()-1;
    parName = fCurrentGroupName + parName;

    // check if this is group definition -------------------------------------------
    TString parValues;
    ss >> parValues;
#ifdef DEBUG_ADDPAR
    lk_debug << "parValues = " << parValues << endl;
#endif
    if (parValues[0]=='#' || parValues.IsNull()) { // group definition
        TString groupName = parName;
        if (groupName.EndsWith("/")==false)
            groupName = groupName + "/";
        if (fGroupNameArray.size()==fTabSizeArray.size()+1)
            fTabSizeArray.push_back(currentTabSize);
        fGroupNameArray.push_back(groupName);
#ifdef DEBUG_ADDPAR
        lk_debug << "groupName = " << groupName << endl;
#endif
        if (parName.EndsWith("/"))
            return 2;
    }

    // value -------------------------------------------
    parValues = ssline;
    parValues.Remove(0,sizeOfHead);
    while (parValues[0]==' ')
        parValues.Remove(0,1);

    // comment -------------------------------------------
    int icomment = parValues.Index("#");
    TString parComment;
    if (icomment>0) {
        parComment = parValues(icomment+1,parValues.Sizeof()-1);
        while (parComment[0]==' ')
            parComment.Remove(0,1);
        // split parValues
        parValues = parValues(0,icomment);
        while (parValues[0]==' ')
            parValues.Remove(0,1);
        while (parValues[parValues.Sizeof()-2]==' ')
            parValues.Remove(parValues.Sizeof()-2,1);
    }

    // add parameter -------------------------------------------
    auto RVAddPar = AddPar(parName, parValues, parComment);

    if (RVAddPar)
        return 1;
    return 0;
}

Bool_t LKParameterContainer::AddPar(TString name, TString value, TString comment)
{
#ifdef DEBUG_ADDPAR
    e_cout << endl;
    lk_info << name << " " << value << " " << comment << endl;
#endif
    LKParameter *parameterPrev = FindParFree(name);
    if (parameterPrev != nullptr) {
        if (fThisIsNewCollection)
            return false;
        parameterPrev -> SetIsLegacy();
        parameterPrev -> SetIsMultiple();
        lk_warning << "Parameter " << name << " already exist! Overwritting ... to " << name << " " << value << endl;
    }

    if (name.IsNull()&&value.IsNull()&&!comment.IsNull()) {
        SetLineComment(comment);
        return true;
    }
    else
    {
        bool allowSetPar = true;
        TString groupName;

        LKParameter parameter0;
        if (parameterPrev!=nullptr)
            parameter0.SetIsMultiple();

        if (name[0]=='<') {
            name = name(1, name.Sizeof()-2);
            AddFile(name, value);
            return true;
        }

        while (true)
        {
            if (name[0]=='!') {
                name = name(1, name.Sizeof()-2);
                parameter0.SetIsRewrite();
                continue;
            }
            if (name[0]=='*') {
                name = name(1, name.Sizeof()-2);
                parameter0.SetIsTemporary();
                continue;
            }
            else if (name[0]=='&') {
                name = name(1, name.Sizeof()-2);
                parameter0.SetIsMultiple();
                continue;
            }
            else if (name[0]=='@') {
                parameter0.SetIsConditional();
                if (name.Index("/")<0) {
                    lk_error << "Parameter name " << name << " is out of naming rule" << endl;
                    break;
                }
                name = name(1, name.Sizeof()-2);
                bool setParIfGroupNameIsNotSet = false;
                if (name[0]=='-') {
                    setParIfGroupNameIsNotSet = true;
                    name = name(1, name.Sizeof()-2);
                }
                groupName = name(0,name.Index("/"));
                if (setParIfGroupNameIsNotSet) {
                    allowSetPar = false;
                    if (CheckPar(groupName)==false)
                        allowSetPar = true;
                    else if (GetParString(groupName).IsNull())
                        allowSetPar = true;
                }
                else if (CheckPar(groupName)==false&&CheckValue(groupName)==false)
                    allowSetPar = false; // @todo save as hidden parameter when they are not allowed to be set
                continue;
            }
            break;
        }

        if (allowSetPar)
        {
            SetPar(name,value,value,comment,parameter0.GetType());
            return true;
        }
        else
            return false;
    }

    return false;
}

LKParameter *LKParameterContainer::SetPar(TString name, TString raw, TString value, TString comment, int parameterType)
{
    if (name.EndsWith("="))
        name = name(0,name.Sizeof()-2);
    ReplaceVariables(value);
    auto parameter = new LKParameter(name, raw, value, comment, parameterType);
    Add(parameter);
    return parameter;
}

LKParameter *LKParameterContainer::SetInputFile(TString name, TString value) {
    auto parameter = new LKParameter(name, value);
    parameter -> SetIsInputFile();
    Add(parameter);
    return parameter;
}

LKParameter *LKParameterContainer::SetLineComment(TString comment) {
    auto parameter = new LKParameter();
    while (comment[0]==' ')
        comment.Remove(0,1);
    parameter -> SetLineComment(comment);
    Add(parameter);
    return parameter;
}

LKParameter *LKParameterContainer::SetParFile(TString name) {
    return SetLineComment(Form("input file %s", name.Data()));
}

LKParameter *LKParameterContainer::SetParCont(TString name) {
    return SetLineComment(Form("input container %s", name.Data()));
}

void LKParameterContainer::UpdateBinning(TString name, Int_t &n, Double_t &x1, Double_t &x2) const
{
    if (CheckPar(name) && GetParN(name)>=3)
    {
        n = GetParInt(name,0);
        x1 = GetParDouble(name,1);
        x2 = GetParDouble(name,2);
    }
}

void LKParameterContainer::UpdateV3(TString name, Double_t &x, Double_t &y, Double_t &z) const
{
    if (CheckPar(name) && GetParN(name)>=3)
    {
        x = GetParDouble(name,0);
        y = GetParDouble(name,1);
        z = GetParDouble(name,2);
    }
}

LKParameterContainer* LKParameterContainer::CreateGroupContainer(TString nameGroup)
{
    R__COLLECTION_READ_LOCKGUARD(ROOT::gCoreMutex);

    auto groupContainer = new LKParameterContainer();
    groupContainer -> SetName(nameGroup);

    TIter iterator(this);
    LKParameter *parameter;
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        if (parameter) {
            if (nameGroup==parameter->GetGroup()) {
                TString mainName = parameter -> GetMainName();
                TString value = parameter -> GetValue();
                groupContainer -> AddPar(mainName,value);
                //groupContainer -> Add(parameter);
            }
        }
    }

    return groupContainer;
}

LKParameterContainer* LKParameterContainer::CreateMultiParContainer(TString givenName)
{
    R__COLLECTION_READ_LOCKGUARD(ROOT::gCoreMutex);

    TString justName;
    int index = 0;
    while (index>=0) {
        index = givenName.Index(" ");
        if      (index<0)  { justName  = givenName; break; }
        else if (index==0) { givenName = givenName(1,givenName.Sizeof()-2); continue; }
        else               { justName  = givenName(0,index); break; }
    }
    while (1) {
        if      (justName[0]=='<') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='*') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='@') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='&') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='!') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='#') { justName = justName(1,justName.Sizeof()-2); continue; }
        else
            break;
    }

    auto multiParContainer = new LKParameterContainer();
    multiParContainer -> SetName(givenName);

    TIter iterator(this);
    LKParameter *parameter = nullptr;
    LKParameter *parameterFound = nullptr;
    bool parameterIsFound = false;
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        if (parameter) {
            auto parName = parameter -> GetName();
            if (parameter -> IsConditional()) {
                auto mainName = parameter -> GetMainName();
                if (mainName==justName) {
                    multiParContainer -> Add(parameter);
                }
            }
            else if (parName==justName) {
                multiParContainer -> Add(parameter);
            }
        }
    }

    return multiParContainer;
}

void LKParameterContainer::Require(TString name, TString value, TString comment, TString type, int compare)
{
    if (fParameterCollectionMode && fCollectedParameterContainer->FindPar(name)==nullptr)
    {
        auto parc = fCollectedParameterContainer -> SetPar(name, value, value, comment);
        if (!type.IsNull()) parc -> SetType(type);
        if (compare>=0) parc -> SetCompare(compare);
    }

    auto par = FindParFree(name,false);
    if (par!=nullptr) {
        type.ReplaceAll("/","");
        if (!type.IsNull()) par -> SetType(type);
        if (compare>=0) par -> SetCompare(compare);
    }
}

int LKParameterContainer::GetParIndex(TString name) const
{
    auto numEntries = GetEntries();
    int index = -1;
    for (auto iPar=0; iPar<numEntries; ++iPar) {
        auto parameter = (LKParameter*) At(iPar);
        if (parameter) {
            if (parameter->GetName()==name) {
                index = iPar;
                break;
            }
        }
    }
    return index;
}

Bool_t LKParameterContainer::CheckPar(TString name) const
{
    if (FindPar(name) == nullptr)
        return false;
    return true;
}

Bool_t LKParameterContainer::CheckValue(TString name) const
{
    TIter iterator(this);
    LKParameter *parameter;
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        //TString parValue = parameter -> GetTitle();
        TString parValue = parameter -> GetValue();
        if (parValue==name)
            return true;
    }
    return false;
}

LKParameter *LKParameterContainer::FindParFree(TString givenName, bool terminateIfNull)
{
    R__COLLECTION_READ_LOCKGUARD(ROOT::gCoreMutex);

    TString justName;
    int index = 0;
    while (index>=0) {
        index = givenName.Index(" ");
        if      (index<0)  { justName  = givenName; break; }
        else if (index==0) { givenName = givenName(1,givenName.Sizeof()-2); continue; }
        else               { justName  = givenName(0,index); break; }
    }
    while (1) {
        if      (justName[0]=='<') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='*') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='@') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='&') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='!') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='#') { justName = justName(1,justName.Sizeof()-2); continue; }
        else
            break;
    }

    TIter iterator(this);
    LKParameter *parameter = nullptr;
    LKParameter *parameterFound = nullptr;
    bool parameterIsFound = false;
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        if (parameter) {
            auto parName = parameter -> GetName();
            if (parameter -> IsLegacy())
                continue;
            if (parameter -> IsConditional()) {
                auto mainName = parameter -> GetMainName();
                if (mainName==justName) {
                    parameterIsFound = true;
                    parameterFound = parameter;
                    //break;
                }
            }
            else if (parName==justName) {
                parameterIsFound = true;
                parameterFound = parameter;
                //break;
            }
        }
    }

    if (fParameterCollectionMode && fCollectedParameterContainer->FindPar(givenName)==nullptr)
    {
        if (parameterIsFound)
            fCollectedParameterContainer -> AddLine(Form("%s",parameterFound->GetLine().Data()));
        else
            fCollectedParameterContainer -> AddLine(Form("%s",givenName.Data()));
    }

    if (parameterIsFound)
        return parameterFound;

    if (terminateIfNull) {
        lk_error << "parameter " << justName << " does not exist!" << endl;
        gApplication -> Terminate();
    }

    return (LKParameter *) nullptr;
}

LKParameter *LKParameterContainer::FindPar(TString givenName, bool terminateIfNull) const
{
    R__COLLECTION_READ_LOCKGUARD(ROOT::gCoreMutex);

    TString justName;
    int index = 0;
    while (index>=0) {
        index = givenName.Index(" ");
        if      (index<0)  { justName  = givenName; break; }
        else if (index==0) { givenName = givenName(1,givenName.Sizeof()-2); continue; }
        else               { justName  = givenName(0,index); break; }
    }
    while (1) {
        if      (justName[0]=='<') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='*') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='@') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='&') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='!') { justName = justName(1,justName.Sizeof()-2); continue; }
        else if (justName[0]=='#') { justName = justName(1,justName.Sizeof()-2); continue; }
        else
            break;
    }

    TIter iterator(this);
    LKParameter *parameter = nullptr;
    LKParameter *parameterFound = nullptr;
    bool parameterIsFound = false;
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        if (parameter) {
            auto parName = parameter -> GetName();
            if (parameter -> IsLegacy())
                continue;
            if (parameter -> IsConditional()) {
                auto mainName = parameter -> GetMainName();
                if (mainName==justName) {
                    parameterIsFound = true;
                    parameterFound = parameter;
                    //break;
                }
            }
            else if (parName==justName) {
                parameterIsFound = true;
                parameterFound = parameter;
                //break;
            }
        }
    }

    if (fParameterCollectionMode && fCollectedParameterContainer->FindPar(givenName)==nullptr)
    {
        if (parameterIsFound)
            fCollectedParameterContainer -> AddLine(Form("%s",parameterFound->GetLine().Data()));
        else
            fCollectedParameterContainer -> AddLine(Form("%s",givenName.Data()));
    }

    if (parameterIsFound)
        return parameterFound;

    if (terminateIfNull) {
        lk_error << "parameter " << justName << " does not exist!" << endl;
        gApplication -> Terminate();
    }

    return (LKParameter *) nullptr;
}

void LKParameterContainer::SetCollectParameters(bool collect)
{
    fParameterCollectionMode = collect;
    if (fParameterCollectionMode) {
        if (fCollectedParameterContainer==nullptr) {
            fCollectedParameterContainer = new LKParameterContainer();
            fCollectedParameterContainer -> SetName("CollectedPar");
            fCollectedParameterContainer -> SetIsNewCollection();
        }
    }
}

void LKParameterContainer::PrintCollection(TString fileName)
{
    fCollectedParameterContainer -> Sort();
    if (fileName.IsNull() || fileName=="print")
        fCollectedParameterContainer -> Print("!eval line# par# !idx");
    else
        fCollectedParameterContainer -> SaveAs(fileName);
}


LKCut* LKParameterContainer::GetParCut(TString name)
{
    TString name2 = name;
    name2.ReplaceAll("/","_");
    LKCut* cuts = new LKCut(name2);
    auto cutList = CreateMultiParContainer(name);
    auto numCuts = cutList -> GetEntries();
    for (auto iCut=0; iCut<numCuts; ++iCut)
    {
        auto parameter = (LKParameter*) cutList -> At(iCut);
        TString cutString = parameter -> GetString(0);
        bool applyCut = true;
        if (parameter->GetN()>1)
            applyCut = parameter->GetBool(1);
        if (cutString.EndsWith(".root")) {
            auto file = new TFile(cutString,"read");
            if (file->IsOpen()==false) {
                lk_error << "Cannnot open " << cutString << endl;
                return cuts;
            }
            auto cutg = (TCutG*) file -> Get("CUTG");
            if (cutg==nullptr) {
                lk_error << "CUTG is null in " << cutString << endl;
                return cuts;
            }
            cutg -> SetName(Form("cutg_%d",iCut));
            cutg -> SetTitle(cutString);
            cuts -> Add(cutg,applyCut);
        }
        else {
            TCut cut(Form("cut_%d",iCut),cutString);
            cuts -> Add(cut,applyCut);
        }
    }
    return cuts;
}
