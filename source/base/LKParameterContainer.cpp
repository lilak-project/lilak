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
        fileName0 = fileName0 + ".par";
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

void LKParameterContainer::ReplaceEnvVariables(TString &valInput)
{
    int ienv = valInput.Index("e{");
    while (ienv>=0) {
        int fenv = valInput.Index("}",1,ienv,TString::kExact);

        TString replaceFrom = TString(valInput(ienv+2,fenv-ienv-2));
        TString replaceFrom2 = replaceFrom;
        replaceFrom2.ToLower();
        TString replaceTo;
             if (replaceFrom2=="lilak_version"            ) replaceTo = LILAK_VERSION;
        else if (replaceFrom2=="lilak_hash"               ) replaceTo = LILAK_HASH;
        else if (replaceFrom2=="lilak_mainproject_version") replaceTo = LILAK_MAINPROJECT_VERSION;
        else if (replaceFrom2=="lilak_mainproject_hash"   ) replaceTo = LILAK_MAINPROJECT_HASH;
        else if (replaceFrom2=="lilak_hostname"           ) replaceTo = LILAK_HOSTNAME;
        else if (replaceFrom2=="lilak_username"           ) replaceTo = LILAK_USERNAME;
        else if (replaceFrom2=="lilak_hostuser"           ) replaceTo = LILAK_HOSTUSER;
        else if (replaceFrom2=="lilak_path"               ) replaceTo = LILAK_PATH;
        else if (replaceFrom2=="lilak_version"            ) replaceTo = LILAK_VERSION;
        else
            replaceTo = getenv(replaceFrom);

        valInput.Replace(ienv,fenv-ienv+1,replaceTo);
        ienv = valInput.Index("e{");
    }

    if (valInput[0] == '$') {
        TString env = valInput;
        Ssiz_t nenv = env.First("/");
        env.Resize(nenv);
        env.Remove(0,1);
        valInput.Replace(0, nenv+1, getenv(env));
    }
}

void LKParameterContainer::ReplaceVariables(TString &valInput)
{
    ReplaceEnvVariables(valInput);

    int ipar = valInput.Index("{");
    while (ipar>=0) {
        int fpar = valInput.Index("}",1,ipar,TString::kExact);
        TString parName2 = valInput(ipar+1,fpar-ipar-1);
        TString parValue2 = GetParString(parName2);
        valInput.Replace(ipar,fpar-ipar+1,parValue2);
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

    bool rewriteParameter = false;
    if (fileName[0]=='!') {
        fileName = fileName(1, fileName.Sizeof()-2);
        rewriteParameter = true;
    }

    bool existFile = false;

    if (fileName[0]=='/' || fileName[0]=='$' || fileName =='~'|| fileName[0]=='.') {
        fileNameFull = fileName;
        if (!TString(gSystem -> Which(".", fileNameFull.Data())).IsNull())
            existFile = true;
    }
    else
    {
        fileNameFull = TString(gSystem -> Getenv("PWD")) + "/" + fileName;
        if (!TString(gSystem -> Which(".", fileNameFull.Data())).IsNull())
            existFile = true;
        else
        {
            fileNameFull = TString(gSystem -> Getenv("NEST_PATH")) + "/input/" + fileName;
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
        parName = Form("%d", fNumInputFiles);

    fNumInputFiles++;
    LKParameter *parFile;
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
            if (rewriteParameter && line.size()>0)
                line = "!"+line;
            if (AddLine(line))
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

    return countParameters;
}

Int_t LKParameterContainer::AddParameterContainer(LKParameterContainer *parc)
{
    lk_info << "Adding parameter container " << parc -> GetName() << endl;

    TString parName = Form("%d", fNumInputFiles);
    fNumInputFiles++;
    auto parameter_parc = SetParCont(parc->GetName());

    Int_t countParameters = 0;
    Int_t countSameParameters = 0;

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

    bool evaluatePar = true;
    bool showLineComment = false;
    bool showParComments = true;
    bool printToScreen = true;
    bool printToFile = false;
    ofstream fileOut;

    if (printOptions.Index("!eval" )>=0) { evaluatePar = false;      printOptions.ReplaceAll("!eval", ""); }
    if (printOptions.Index("eval"  )>=0) { evaluatePar = true;       printOptions.ReplaceAll("eval",  ""); }

    if (printOptions.Index("!line#")>=0) { showLineComment = false;  printOptions.ReplaceAll("!line#",""); }
    if (printOptions.Index("line#" )>=0) { showLineComment = true;   printOptions.ReplaceAll("line#", ""); }

    if (printOptions.Index("!par#" )>=0) { showParComments = false;  printOptions.ReplaceAll("!par#", ""); }
    if (printOptions.Index("par#"  )>=0) { showParComments = true;   printOptions.ReplaceAll("par#",  ""); }

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
        fileOut << endl;
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
        bool parIsTemporary = parameter -> IsTemporary();
        bool parIsConditional = parameter -> IsConditional();
        bool parIsMultiple = parameter -> IsMultiple();

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

             if (parIsTemporary)   parName = Form("%s%s","*",parName.Data());
        else if (parIsConditional) parName = Form("%s%s","@",parName.Data());
        else if (parIsMultiple)    parName = Form("%s%s","&",parName.Data());

        int nwidth = 20;
             if (parName.Sizeof()>60) nwidth = 70;
        else if (parName.Sizeof()>50) nwidth = 60;
        else if (parName.Sizeof()>40) nwidth = 50;
        else if (parName.Sizeof()>30) nwidth = 40;
        else if (parName.Sizeof()>20) nwidth = 30;
        else                          nwidth = 20;

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

        if (isLineComment && showLineComment) {
            if (showLineComment) {
                if (printToScreen) e_cout << "# " << parComment << endl;
                if (printToFile)   fileOut << "# " << parComment << endl;
            }
        }
        else if (isParameter) {
            if (printToScreen) e_list(parNumber) << left << setw(nwidth) << parName << " " << setw(vwidth) << parValue << " " << parComment << endl;
            if (printToFile) fileOut << parNumber << ". " << left << setw(nwidth) << parName << " " << setw(vwidth) << parValue << " " << parComment << endl;
        }

        preGroup = parGroup;
        if (isParameter)
            parNumber++;
    }

    if (printToScreen)
        lk_info << "End of Parameter Container " << fName << endl;
        e_cout << endl;

    if (printToFile)
        fileOut << endl;
}

LKParameterContainer *LKParameterContainer::CloneParameterContainer(const char* name) const
{
    LKParameterContainer *new_collection = new LKParameterContainer();
    new_collection -> SetName("ParameterContainer_clone");

    TIter iterator(this);
    LKParameter *parameter;
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        if (!parameter -> IsTemporary())
            new_collection -> Add(parameter);
    }

    return new_collection;
}

Bool_t LKParameterContainer::AddLine(std::string line)
{
    if (line.empty())
        return false;

    if (line.find("#") == 0) {
        TString line2 = TString(line);
        line2 = line2(1,line2.Sizeof()-2);
        SetLineComment(line2);
        return true;
    }

    istringstream ss(line);
    TString parName;
    ss >> parName;

    TString parValues = line;
    parValues.Remove(0,parName.Sizeof()-1);
    while (parValues[0]==' ')
        parValues.Remove(0,1);

    int icomment = parValues.Index("#");
    TString parComment;

    if (icomment>0) {
        parComment = parValues(icomment+1,parValues.Sizeof()-1);
        while (parComment[0]==' ')
            parComment.Remove(0,1);

        parValues = parValues(0,icomment);
        while (parValues[0]==' ')
            parValues.Remove(0,1);
        while (parValues[parValues.Sizeof()-2]==' ')
            parValues.Remove(parValues.Sizeof()-2,1);
    }

    return AddPar(parName, parValues, parComment);
    //if (parName.Index("<<")==0) {
    //    AddFile(parValues);
    //}
    //else {
    //    return AddPar(parName, parValues, parComment);
    //}

    return false;
}

Bool_t LKParameterContainer::AddPar(TString name, TString value, TString comment)
{
    bool sendErrorIfAlreadyExist = false;
    if (FindPar(name) != nullptr)
        sendErrorIfAlreadyExist = true;

    if (name.IsNull()&&value.IsNull()&&!comment.IsNull()) {
        SetLineComment(comment);
        return true;
    }
    else
    {
        bool allowSetPar = true;
        TString groupName;

        int parameterType = kParameterIsStandard;
        bool rewriteParameter = false;

        if (name[0]=='<') {
            name = name(1, name.Sizeof()-2);
            AddFile(name, value);
            return true;
        }

        while (true)
        {
            if (name[0]=='!') {
                name = name(1, name.Sizeof()-2);
                rewriteParameter = true;
                sendErrorIfAlreadyExist = false;
            }
            else
                break;
        }

        while (true) {
            if (name[0]=='*') {
                name = name(1, name.Sizeof()-2);
                parameterType = kParameterIsTemporary;
                continue;
            }
            else if (name[0]=='@') {
                parameterType = kParameterIsConditional;
                name = name(1, name.Sizeof()-2);
                groupName = name(0,name.Index("/"));
                if (name.Index("/")<0)
                    lk_error << "Parameter name " << name << " is out of naming rule" << endl;
                if (CheckPar(groupName)==false&&CheckValue(groupName)==false)
                    allowSetPar = false; // @todo save as hidden parameter when they are not allowed to be set
                    continue;
            }
            else if (name[0]=='&') {
                name = name(1, name.Sizeof()-2);
                parameterType = kParameterIsMultiple;
                sendErrorIfAlreadyExist = false;
                continue;
            }
            break;
        }

        if (allowSetPar)
        {
            if (sendErrorIfAlreadyExist) {
                lk_error << "Parameter " << name << " already exist!" << endl;
                return false;
            }
            else {
                SetPar(name,value,value,comment,parameterType,rewriteParameter);
                return true;
            }
        }
        else
            return false;
    }

    return false;
}

LKParameter *LKParameterContainer::SetPar(TString name, TString raw, TString value, TString comment, int parameterType, bool rewriteParameter)
{
    auto parameterOld = (LKParameter*) FindPar(name);
    if (parameterOld!=nullptr) {
        if (rewriteParameter) {
            SetLineComment(TString(Form("%s was overwritten from %s",name.Data(),parameterOld->GetRaw().Data())));
            Remove(parameterOld);
        }
        else
            return parameterOld;
    }
    ReplaceVariables(value);
    auto parameter = new LKParameter(name, raw, value, comment, parameterType);
    if (parameter -> IsConditional() && rewriteParameter) {
        auto mainName = parameter -> GetMainName();
        auto parameterConditional = (LKParameter*) FindPar(mainName);
        if (parameterConditional!=nullptr)
            Remove(parameterConditional);
    }
    Add(parameter);
    return parameter;
}

LKParameter *LKParameterContainer::SetLineComment(TString comment) {
    auto parameter = new LKParameter();
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

TString LKParameterContainer::GetParString(TString name, int idx) const
{
    auto parameter = FindPar(name,true);
    TString value = parameter -> GetString(idx);
    return value;
}

Int_t LKParameterContainer::GetParN(TString name) const
{
    auto parameter = FindPar(name,true);
    auto numValues = parameter -> GetN();
    return numValues;
}

Bool_t LKParameterContainer::GetParBool(TString name, int idx) const
{
    auto parameter = FindPar(name,true);
    auto valueBool = parameter -> GetBool(idx);
    return valueBool;
}

Int_t LKParameterContainer::GetParInt(TString name, int idx) const
{
    auto parameter = FindPar(name,true);
    auto valueInt = parameter -> GetInt(idx);
    return valueInt;
}

Long64_t LKParameterContainer::GetParLong(TString name, int idx) const
{
    auto parameter = FindPar(name,true);
    auto valueLong = parameter -> GetLong(idx);
    return valueLong;
}

Double_t LKParameterContainer::GetParDouble(TString name, int idx) const
{
    auto parameter = FindPar(name,true);
    auto valueDouble = parameter -> GetDouble(idx);
    return valueDouble;
}

TVector3 LKParameterContainer::GetParV3(TString name) const
{
    auto parameter = FindPar(name,true);
    auto valueV3 = parameter -> GetV3();
    return valueV3;
}

Int_t LKParameterContainer::GetParColor(TString name, int idx) const
{
    auto parameter = FindPar(name,true);
    auto valueColor = parameter -> GetColor(idx);
    return valueColor;
}

axis_t LKParameterContainer::GetParAxis(TString name, int idx) const
{
    auto parameter = FindPar(name,true);
    auto valueAxis = parameter -> GetAxis(idx);
    return valueAxis;
}

std::vector<bool> LKParameterContainer::GetParVBool(TString name) const
{
    auto parameter = FindPar(name,true);
    auto array = parameter -> GetVBool();
    return array;
}

std::vector<int> LKParameterContainer::GetParVInt(TString name) const
{
    auto parameter = FindPar(name,true);
    auto array = parameter -> GetVInt();
    return array;
}

std::vector<double> LKParameterContainer::GetParVDouble(TString name) const
{
    auto parameter = FindPar(name,true);
    auto array = parameter -> GetVDouble();
    return array;
}

std::vector<TString> LKParameterContainer::GetParVString(TString name) const
{
    auto parameter = FindPar(name,true);
    auto array = parameter -> GetVString();
    return array;
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
            }
        }
    }

    return groupContainer;
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

LKParameter *LKParameterContainer::FindPar(TString givenName, bool terminateIfNull) const
{
    R__COLLECTION_READ_LOCKGUARD(ROOT::gCoreMutex);

    TIter iterator(this);
    LKParameter *parameter = nullptr;
    bool parameterIsFound = false;
    while ((parameter = dynamic_cast<LKParameter*>(iterator())))
    {
        if (parameter) {
            auto parName = parameter -> GetName();
            if (parName==givenName) {
                parameterIsFound = true;
                break;
            }
            if (parameter -> IsConditional()) {
                auto mainName = parameter -> GetMainName();
                if (mainName==givenName) {
                    parameterIsFound = true;
                    break;
                }
            }
        }
    }

    if (parameterIsFound) {
        //TString value = parameter -> GetRaw();
        //ReplaceVariables(value);
        //parameter -> SetValue(value);
        return parameter;
    }

    if (terminateIfNull) {
        lk_error << "parameter " << givenName << " does not exist!" << endl;
        gApplication -> Terminate();
    }

    return (LKParameter *) nullptr;
}
