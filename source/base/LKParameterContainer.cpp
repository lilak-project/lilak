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

#include "LKParameterContainer.hpp"

using namespace std;

ClassImp(LKParameterContainer)

LKParameterContainer::LKParameterContainer(Bool_t collect)
    :TObjArray(), fCollectionMode(collect)
{
    fName = "ParameterContainer";
}

LKParameterContainer::LKParameterContainer(const char *parName, Bool_t collect)
    :LKParameterContainer(collect)
{
    AddFile(TString(parName));
}

void LKParameterContainer::ProcessParNotFound(TString name, TString val) {
    if (fCollectionMode) {
        lk_warning << "parameter " << name << " does not exist!" << endl;
        lk_warning << "parameter will be added with default value." << endl;
        SetPar(name,val,"This parameter is collected from parameter collecting mode. Please modify the value");
    }
    else {
        lk_error << "parameter " << name << " does not exist!" << endl;
        gApplication -> Terminate();
    }
}

void LKParameterContainer::ProcessTypeError(TString name, TString value, TString type) const {
    lk_error << "parameter " << name << "=" << value << " is not convertable to " << type << endl;
    gApplication -> Terminate();
}

bool LKParameterContainer::CheckFormulaValidity(TString formula, bool isInt) const
{
    if (isInt && formula.Index(".")>=0)
        return false;

    TString formula2 = formula;
    formula2.ReplaceAll("+"," ");
    formula2.ReplaceAll("-"," ");
    formula2.ReplaceAll("/"," ");
    formula2.ReplaceAll("*"," ");
    formula2.ReplaceAll("."," ");

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

bool LKParameterContainer::IsEmpty() const
{
    if (GetEntries()>0)
        return false;
    return true;
}

void LKParameterContainer::ReplaceVariablesConst(TString &valInput, TString parName) const
{
    int ienv = valInput.Index("e{");
    while (ienv>=0) {
        int fenv = valInput.Index("}",1,ienv,TString::kExact);
        valInput.Replace(ienv,fenv-ienv+1,getenv(TString(valInput(ienv+2,fenv-ienv-2))));
        ienv = valInput.Index("e{");
    }

    int ipar = valInput.Index("{");
    while (ipar>=0) {
        int fpar = valInput.Index("}",1,ipar,TString::kExact);
        TString parName2 = valInput(ipar+1,fpar-ipar-1);
        if (parName2==parName || parName2.Index(parName+"[")>=0) {
            lk_error << "The parameter is refering to the valInput it self!" << endl;
            gApplication -> Terminate();
        }
        TString parValue2 = GetParStringConst(parName2);
        valInput.Replace(ipar,fpar-ipar+1,parValue2);
        ipar = valInput.Index("{");
    }

    if (valInput[0] == '$') {
        TString env = valInput;
        Ssiz_t nenv = env.First("/");
        env.Resize(nenv);
        valInput.Replace(0, nenv+1, getenv(env));
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

void LKParameterContainer::ReplaceVariables(TString &valInput, TString parName)
{
    int ienv = valInput.Index("e{");
    while (ienv>=0) {
        int fenv = valInput.Index("}",1,ienv,TString::kExact);
        valInput.Replace(ienv,fenv-ienv+1,getenv(TString(valInput(ienv+2,fenv-ienv-2))));
        ienv = valInput.Index("e{");
    }

    int ipar = valInput.Index("{");
    while (ipar>=0) {
        int fpar = valInput.Index("}",1,ipar,TString::kExact);
        TString parName2 = valInput(ipar+1,fpar-ipar-1);
        if (parName2==parName || parName2.Index(parName+"[")>=0) {
            lk_error << "The parameter is refering to the valInput it self!" << endl;
            gApplication -> Terminate();
        }
        TString parValue2 = GetParString(parName2);
        valInput.Replace(ipar,fpar-ipar+1,parValue2);
        ipar = valInput.Index("{");
    }

    if (valInput[0] == '$') {
        TString env = valInput;
        Ssiz_t nenv = env.First("/");
        env.Resize(nenv);
        env.Remove(0,1);
        valInput.Replace(0, nenv+1, getenv(env));
    }

}

Int_t LKParameterContainer::AddFile(TString fileName, bool addFilePar)
{
    ReplaceVariables(fileName);

    TString fileNameFull;

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

    TString parName = Form("INPUT_FILE_%d", fNumInputFiles);
    TString parName2 = Form("<<%d", fNumInputFiles);

    fNumInputFiles++;
    if (addFilePar)
        SetPar(parName, fileNameFull);
    else {
        SetComment(TString("# ") + parName2 + " " + fileNameFull);
    }

    ifstream file(fileNameFull);

    Int_t countParameters = 0;

    if (fileNameFull.EndsWith(".json")) {
        file >> fJsonValues;
        countParameters = AddJsonTree(fJsonValues);
    }
    else {
        string line;

        while (getline(file, line)) {
            if (SetPar(line))
                countParameters++;
        }

        if (countParameters == 0) {
            this -> Remove(FindObject(parName));
            fNumInputFiles--;
        }

        parName.Replace(0,11,"<<");
        SetComment(Form("%s %d parameters where added",parName.Data(),countParameters));
    }

    return countParameters;
}

Int_t LKParameterContainer::AddParameterContainer(LKParameterContainer *parc)
{
    lk_info << "Adding parameter container " << parc -> GetName() << endl;

    TString parName = Form("INPUT_PARC_%d", fNumInputFiles);
    fNumInputFiles++;
    SetPar(parName, ""); //@todo

    Int_t countParameters = 0;
    Int_t countSameParameters = 0;

    TIter iterator(parc);
    TObject *obj;
    while ((obj = dynamic_cast<TObject*>(iterator())))
    {
        TString name = obj -> GetName();

        TObject *found = FindObject(name);
        if (found != nullptr) {
            if (name.Index("INPUT_FILE_")==0)
                ((TNamed *) obj) -> SetName(name+"_");
            else {
                lk_error << "Parameter " << name << " already exist!" << endl;
                ++countSameParameters ;
                continue;
            }
        }

        Add(obj);
        ++countParameters;
    }

    if (countParameters == 0) {
        this -> Remove(FindObject(parName));
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
                        ||fileName.Index("par")==0
                        ||fileName.Index("json")==0) {
                    listDir.push_back(fileName);
                }
                listFiles -> Remove(sysFile);
            }
            else if (  fileName.EndsWith(".par")
                    || fileName.EndsWith(".conf")
                    || fileName.EndsWith(".json")
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
        lx_info << "* Select index from the below list: " << endl;
        lx_info << "  " << "0) Exit" << endl;
        while ((sysFile=(TSystemFile*)next2()))
        {
            countAll++;
            countFile++;
            fileName = sysFile -> GetName();
            lx_info << "  " << countAll << ") " << fileName << endl;
        }

        int numDir = listDir.size();
        for (auto iDir=0; iDir<numDir; ++iDir) {
            countAll++;
            countDir++;
            TString dirName2 = dirName + "/" + listDir[iDir];
            lx_info << "  " << countAll << ") " << dirName2 << endl;
        }

        TString strSelected;
        lx_info << "Enter index: ";
        cin >> strSelected;

        if (strSelected.IsDigit()) {
            int idxSelected = strSelected.Atoi();
            if (idxSelected==0) {
                lx_info << "Exit" << endl;
                return false;
            }
            else if (idxSelected>0 && idxSelected <= countAll) {
                if (idxSelected <= countFile) {
                    TString nameSelected = listFiles -> At(idxSelected-1) -> GetName();
                    lx_info << nameSelected << " selected!" << endl;
                    LKParameterContainer::AddFile(nameSelected);
                    return true;
                }
                else {
                    int idxDir = countAll - countFile - 1;
                    TString dirName2 = dirName + "/" + listDir[idxDir];
                    lk_debug << idxDir << " / " << numDir << " " << dirName2 << endl;
                    return SearchAndAddPar(dirName2);
                }
            }
            else {
                lx_error << "Invalid index selected." << endl;
                return false;
            }
        }
        else {
            lx_error << "Invalid input." << endl;
            return false;
        }
    }
    lx_warning << "No parameter files found in current directory" << endl;

    int numDir = listDir.size();
    for (auto iDir=0; iDir<numDir; ++iDir) {
        TString dirName2 = dirName + "/" + listDir[iDir];
        if (SearchAndAddPar(dirName2))
            return true;
    }

    return false;
}

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

        //nullValue
        //intValue
        //uintValue
        //realValue
        //stringValue
        //booleanValue
        //arrayValue
        //objectValue
        if (branch.type()==Json::ValueType::stringValue || branch.type()==Json::ValueType::booleanValue) {
            SetPar(parName, branch.asString());
            count_par = count_par + 1;
        }
        else if (branch.type()==Json::ValueType::intValue || branch.type()==Json::ValueType::uintValue) {
            SetPar(parName, branch. asInt());
            count_par = count_par + 1;
        }
        else if (branch.type()==Json::ValueType::realValue) {
            SetPar(parName, branch. asDouble());
            count_par = count_par + 1;
        }
        else if (branch.type()==Json::ValueType::arrayValue) {
            int nValues = branch.size();
            SetParN(parName, nValues);
            TString valueAll = "";
            for (auto iVal=0; iVal<nValues; ++iVal) {
                auto valueString = branch.get(iVal,Json::Value(-999)).asString();
                valueAll = valueAll + valueString + " ";
                SetParArray(parName, valueString, iVal);
            }
            SetParValue(parName, valueAll);
        }
        else if (branch.type()==Json::ValueType::objectValue) {
            Int_t count_par2 = AddJsonTree(*it, parName);
            count_par = count_par + count_par2;
        }
    }

    return count_par;
}

void LKParameterContainer::Print(Option_t *option) const
{
    TString printOptions(option);

    if (printOptions.Index("raw")>=0) {
        TObjArray::Print();
        return;
    }

    bool evalulatePar = false;
    bool showHiddenPar = false;
    bool showLineComments = false;
    bool showParComments = false;
    bool printToScreen = true;
    bool printToFile = false;
    ofstream fileOut;

    printOptions.ReplaceAll(":"," ");
    if (printOptions.Index("eval" )>=0) { evalulatePar = true;     printOptions.ReplaceAll("eval", ""); }
    if (printOptions.Index("line#")>=0) { showLineComments = true; printOptions.ReplaceAll("line#",""); }
    if (printOptions.Index("par#" )>=0) { showParComments = true;  printOptions.ReplaceAll("par#", ""); }
    if (printOptions.Index("all"  )>=0) { showHiddenPar = true;    printOptions.ReplaceAll("all",  ""); }
    printOptions.ReplaceAll(" ","");

    TString fileName = printOptions;
    if (fileName.IsNull()) {
        printToScreen = true;
    }
    else if (fileName.Index(".")>0) {
        printToFile = true;
        printToScreen = false;
    }

    if (printToScreen) {
        lx_cout << endl;
        lk_info << "Parameter Container" << endl;
    }

    if (printToFile)
    {
        lk_info << "Writting " << fileName << endl;
        fileOut.open(fileName);
        fileOut << "# " << fileName << " created from LKParameterContainer::Print" << endl;
        fileOut << endl;
    }

    TIter iterator(this);
    TNamed *obj;
    while ((obj = dynamic_cast<TNamed*>(iterator())))
    {
        TString parName = obj -> GetName();

        TString parValues = obj -> GetTitle();
        if (evalulatePar) {
            auto parN = GetParN(parName);
            if (parN==1)
                ReplaceVariablesConst(parValues,parName);
            else {
                parValues = "";
                for (auto iPar=0; iPar<parN; ++iPar) {
                    TString parValue = GetParStringConst(parName,iPar);
                    if (iPar==0) parValues = parValue;
                    else parValues = parValues + " " + parValue;
                }
            }
        }

        TString parComment;
        if (!showHiddenPar)
            if (parName.Index("NUM_VALUES_")==0||parName.Index("COMMENT_PAR_")==0||parName.EndsWith("]"))
                continue;

        if (parName.Index("COMMENT_LINE_")>=0) {
            if (!showLineComments)
                continue;
            parName = parValues;
            parValues = "";
            if (parName[0]!='#')
                parName = TString("# ") + parName;
        }
        else if (parName.Index("INPUT_FILE_")>=0)
            parName.Replace(0,11,"<<");
        else if (parName.Index("INPUT_PARC_")>=0) {
            if (!showLineComments)
                continue;
            parName.Replace(0,11,"# Parameter container was added here; ");
        }
        else if (showParComments) {
            TNamed* objc = (TNamed *) FindObject(Form("COMMENT_PAR_%s",parName.Data()));
            if (objc!=nullptr)
                parComment = objc -> GetTitle();
            if (!parComment.IsNull()&&parComment[0]!='#')
                parComment = TString("# ") + parComment;
        }

        ostringstream ssLine;
             if (parName.Sizeof()>60) ssLine << left << setw(70) << parName << " " << parValues << " " << parComment << endl;
        else if (parName.Sizeof()>50) ssLine << left << setw(60) << parName << " " << parValues << " " << parComment << endl;
        else if (parName.Sizeof()>40) ssLine << left << setw(50) << parName << " " << parValues << " " << parComment << endl;
        else if (parName.Sizeof()>30) ssLine << left << setw(40) << parName << " " << parValues << " " << parComment << endl;
        else if (parName.Sizeof()>20) ssLine << left << setw(30) << parName << " " << parValues << " " << parComment << endl;
        else                          ssLine << left << setw(20) << parName << " " << parValues << " " << parComment << endl;

        if (printToScreen)
            lk_cout << ssLine.str();
        if (printToFile)
            fileOut << ssLine.str();
    }

    if (printToFile)
        fileOut << endl;
}

Bool_t LKParameterContainer::SetPar(std::string line)
{
    if (line.empty())
        return false;

    if (line.find("#") == 0) {
        SetComment(TString(line));
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
    TString parComment = parValues(icomment,parValues.Sizeof());

    if (icomment>0) {
        parValues = parValues(0,icomment);
        while (parValues[parValues.Sizeof()-2]==' ')
            parValues.Remove(parValues.Sizeof()-2,1);
    }

    if (parName.Index("<<")==0) {
        AddFile(parValues);
    }
    else {
        SetPar(parName, parValues, parComment);
    }

    return true;
}

Bool_t LKParameterContainer::SetPar(TString name, TString val, TString comment)
{
    if (FindObject(name) != nullptr) {
        lk_error << "Parameter " << name << " already exist!" << endl;
        return false;
    }

    if (name.IsNull()&&val.IsNull()&&!comment.IsNull())
        SetComment(comment);
    else {
        SetParValue(name,val);
        if (!comment.IsNull())
            SetParComment(name,comment);
    }

    auto valueTokens = val.Tokenize(" ");
    Int_t numValues = valueTokens -> GetEntries();
    SetParN(name,numValues);
    if (numValues>1) {
        for (auto iVal=0; iVal<numValues; ++iVal) {
            TString parValue(((TObjString *) valueTokens->At(iVal))->GetString());
            SetParArray(name,parValue,iVal);
        }
    }

    return true;
}

void LKParameterContainer::SetParValue(TString name, TString val) {
    Add(new TNamed(name, val));
}
void LKParameterContainer::SetParComment(TString name, TString val) {
    SetParValue(TString(Form("COMMENT_PAR_%s",name.Data())), val);
}
void LKParameterContainer::SetParN(TString name, Int_t val) {
    SetParValue(TString("NUM_VALUES_")+name, Form("%d",val));
}
void LKParameterContainer::SetParArray(TString name, TString val, Int_t idx) {
    SetParValue(name+"["+idx+"]", val);
}
void LKParameterContainer::SetComment(TString comment) {
    SetParValue(TString(Form("COMMENT_LINE_%d",GetEntries())), comment);
}

TString LKParameterContainer::GetParStringConst(TString name, Int_t idx) const
{
    if (idx>=0) return GetParStringConst(name+"["+idx+"]");

    TObject *obj = FindObject(name);
    TString value = ((TNamed *) obj) -> GetTitle();
    ReplaceVariablesConst(value,name);

    return value;
}

TString LKParameterContainer::GetParString(TString name, Int_t idx)
{
    if (idx>=0) return GetParString(name+"["+idx+"]");

    TObject *obj = FindObject(name);
    if (obj == nullptr) {
        ProcessParNotFound(name,"default_parameter_value");
        return "default_parameter_value";
    }

    TString value = ((TNamed *) obj) -> GetTitle();
    ReplaceVariables(value,name);

    return value;
}

Int_t LKParameterContainer::GetParN(TString name) const
{
    TString name_n = TString("NUM_VALUES_") + name;

    TObject *obj = FindObject(name_n);
    if (obj == nullptr) {
        if (FindObject(name)==nullptr)
            return 0;
        else
            return 1;
    }

    TString value = ((TNamed *) obj) -> GetTitle();

    return value.Atoi();
}

Bool_t LKParameterContainer::GetParBool(TString name, Int_t idx)
{
    if (idx>=0) return GetParBool(name+"["+idx+"]");

    TObject *obj = FindObject(name);
    if (obj == nullptr) {
        ProcessParNotFound(name,"false");
        return false;
    }

    TString value = ((TNamed *) obj) -> GetTitle();
    ReplaceVariables(value,name);

    value.ToLower();
    if (value=="true "||value=="1") return true;
    else if (value=="false"||value=="0") return false;
    else 
        ProcessTypeError(name,value,"bool");

    return true;
}

Int_t LKParameterContainer::GetParInt(TString name, Int_t idx)
{
    if (idx>=0) return GetParInt(name+"["+idx+"]");

    TObject *obj = FindObject(name);
    if (obj == nullptr) {
        ProcessParNotFound(name,"0");
        return 0;
    }

    TString value = ((TNamed *) obj) -> GetTitle();
    ReplaceVariables(value,name);

    if (!CheckFormulaValidity(value,1))
        ProcessTypeError(name, value, "int");

    return Int_t(Eval(value));
}

Double_t LKParameterContainer::GetParDouble(TString name, Int_t idx)
{
    if (idx>=0) return GetParDouble(name+"["+idx+"]");

    TObject *obj = FindObject(name);
    if (obj == nullptr) {
        ProcessParNotFound(name,"0.1");
        return 0.1;
    }

    TString value = ((TNamed *) obj) -> GetTitle();
    ReplaceVariables(value,name);

    if (!CheckFormulaValidity(value))
        ProcessTypeError(name, value, "double");

    return Eval(value);
}

TVector3 LKParameterContainer::GetParV3(TString name)
{
    TString xname = name + "[0]";
    TString yname = name + "[1]";
    TString zname = name + "[2]";

    TObject *xobj = FindObject(xname);
    if (xobj == nullptr) {
        ProcessParNotFound(name,".9,.9,.9");
        return TVector3(.9,.9,.9);
    }

    double x = GetParDouble(xname);
    double y = GetParDouble(yname);
    double z = GetParDouble(zname);

    return TVector3(x,y,z);
}

Int_t LKParameterContainer::GetParColor(TString name, Int_t idx)
{
    if (idx>=0) return GetParColor(name+"["+idx+"]");

    TObject *obj = FindObject(name);
    if (obj == nullptr) {
        ProcessParNotFound(name,"kBlack");
        return 1;//"kBlack";
    }

    TString value = ((TNamed *) obj) -> GetTitle();
    ReplaceVariables(value,name);

    if (value.Index("k")==0)
    {
        value.ReplaceAll("kWhite"  ,"0");
        value.ReplaceAll("kBlack"  ,"1");
        value.ReplaceAll("kGray"   ,"920");
        value.ReplaceAll("kRed"    ,"632");
        value.ReplaceAll("kGreen"  ,"416");
        value.ReplaceAll("kBlue"   ,"600");
        value.ReplaceAll("kYellow" ,"400");
        value.ReplaceAll("kMagenta","616");
        value.ReplaceAll("kCyan"   ,"432");
        value.ReplaceAll("kOrange" ,"800");
        value.ReplaceAll("kSpring" ,"820");
        value.ReplaceAll("kTeal"   ,"840");
        value.ReplaceAll("kAzure"  ,"860");
        value.ReplaceAll("kViolet" ,"880");
        value.ReplaceAll("kPink"   ,"900");
    }

    if (!CheckFormulaValidity(value,1))
        ProcessTypeError(name,value,"color");

    return Int_t(Eval(value));
}

axis_t LKParameterContainer::GetParAxis(TString name, int idx)
{
    if (idx>=0) return GetParAxis(name+"["+idx+"]");

    TObject *obj = FindObject(name);
    if (obj == nullptr) {
        ProcessParNotFound(name, LKVector3::kNon);
        return LKVector3::kNon;
    }

    TString value = ((TNamed *) obj) -> GetTitle();

         if (value=="x")  return LKVector3::kX;
    else if (value=="y")  return LKVector3::kY;
    else if (value=="z")  return LKVector3::kZ;
    else if (value=="-x") return LKVector3::kMX;
    else if (value=="-y") return LKVector3::kMY;
    else if (value=="-z") return LKVector3::kMZ;
    else if (value=="i")  return LKVector3::kI;
    else if (value=="j")  return LKVector3::kJ;
    else if (value=="k")  return LKVector3::kK;
    else if (value=="-i") return LKVector3::kMI;
    else if (value=="-j") return LKVector3::kMJ;
    else if (value=="-k") return LKVector3::kMK;
    else                  return LKVector3::kNon;
}

std::vector<bool> LKParameterContainer::GetParVBool(TString name)
{
    std::vector<bool> array;
    auto npar = GetParN(name);
    if (npar==1)
        array.push_back(GetParBool(name));
    else
        for (auto i=0; i<npar; ++i)
            array.push_back(GetParBool(name,i));
    return array;
}

std::vector<int> LKParameterContainer::GetParVInt(TString name)
{
    std::vector<int> array;
    auto npar = GetParN(name);
    if (npar==1)
        array.push_back(GetParInt(name));
    else
        for (auto i=0; i<npar; ++i)
            array.push_back(GetParInt(name,i));
    return array;
}

std::vector<double> LKParameterContainer::GetParVDouble(TString name)
{
    std::vector<double> array;
    auto npar = GetParN(name);
    if (npar==1)
        array.push_back(GetParDouble(name));
    else
        for (auto i=0; i<npar; ++i)
            array.push_back(GetParDouble(name,i));
    return array;
}

std::vector<TString> LKParameterContainer::GetParVString(TString name)
{
    std::vector<TString> array;
    auto npar = GetParN(name);
    if (npar==1)
        array.push_back(GetParString(name));
    else
        for (auto i=0; i<npar; ++i)
            array.push_back(GetParString(name,i));
    return array;
}

Bool_t LKParameterContainer::CheckPar(TString name) const
{
    if (FindObject(name) != nullptr) return true;
    return false;
}
