#include "TObjArray.h"
#include "TObjString.h"
#include "TFormula.h"
#include "TApplication.h"

#include "LKParameter.h"

LKParameter::LKParameter()
{
}

LKParameter::LKParameter(TString name, TString raw, TString value, TString comment, bool temporary, bool conditional)
{
    SetPar(name, raw, value, comment, temporary, conditional);
}

LKParameter::~LKParameter()
{
}

void LKParameter::SetLineComment(TString comment)
{
    fGroup = "";
    fMainName = "";
    fTitle = "";
    fValue = "";
    fComment = comment;
    fNumValues = 0;
    fValueArray.clear();
    fTemporary = false;
    fConditional = false;
}

void LKParameter::SetPar(TString name, TString raw, TString value, TString comment, bool temporary, bool conditional)
{
    fName = name;
    fTitle = raw;
    fValue = value;
    fComment = comment;
    fTemporary = temporary;
    fConditional = conditional;

    int iSlash = fName.Index("/");
    if (iSlash>=0) {
        fGroup    = fName(0,iSlash);
        fMainName = fName(iSlash+1,fName.Sizeof()-iSlash-2);
    }
    else {
        fGroup = "";
        fMainName = fName;
    }

    if (fTitle.IsNull() && !fValue.IsNull()) fTitle = fValue;
    if (!fTitle.IsNull() && fValue.IsNull()) fValue = fTitle;

    auto listOfTokens = value.Tokenize(" ");
    fNumValues = listOfTokens -> GetEntries();
    if (fNumValues>1) {
        for (auto iVal=0; iVal<fNumValues; ++iVal) {
            TString parValue(((TObjString *) listOfTokens->At(iVal))->GetString());
            fValueArray.push_back(parValue);
        }
    }
}

void LKParameter::Clear(Option_t *option)
{
    fGroup = "";
    fMainName = "";
    fTitle = "";
    fValue = "";
    fComment = "";
    fNumValues = 0;
    fValueArray.clear();
    fTemporary = false;
    fConditional = false;
}

void LKParameter::Print(Option_t *option) const
{
    lx_info << fName << " " << fValue << " # " << fComment << std::endl;
}

int LKParameter::GetInt(int idx) const
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];

    if (!CheckFormulaValidity(value,true))
        ProcessTypeError("int");

    return TFormula("formula",value).Eval(0);
}

bool LKParameter::GetBool(int idx) const
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];

    value.ToLower();
    if (value=="true"||value=="1") return true;
    else if (value=="false"||value=="0") return false;
    else 
        ProcessTypeError("bool");
    return true;
}

double LKParameter::GetDouble(int idx) const
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];

    if (!CheckFormulaValidity(value))
        ProcessTypeError("double");

    return TFormula("formula",value).Eval(0);
}

TString LKParameter::GetString(int idx) const
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];

    return value;
}

int LKParameter::GetColor(int idx) const
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];

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

    if (!CheckFormulaValidity(value,true))
        ProcessTypeError("color");

    return TFormula("formula",value).Eval(0);
}

axis_t LKParameter::GetAxis(int idx) const
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];

         if (value== "x") return LKVector3::kX;
    else if (value== "y") return LKVector3::kY;
    else if (value== "z") return LKVector3::kZ;
    else if (value=="-x") return LKVector3::kMX;
    else if (value=="-y") return LKVector3::kMY;
    else if (value=="-z") return LKVector3::kMZ;
    else if (value== "i") return LKVector3::kI;
    else if (value== "j") return LKVector3::kJ;
    else if (value== "k") return LKVector3::kK;
    else if (value=="-i") return LKVector3::kMI;
    else if (value=="-j") return LKVector3::kMJ;
    else if (value=="-k") return LKVector3::kMK;

    return LKVector3::kNon;
}

TVector3 LKParameter::GetV3() const
{
    return TVector3(GetDouble(0), GetDouble(1), GetDouble(2));
}

std::vector<bool> LKParameter::GetVBool() const
{
    std::vector<bool> array;
    auto npar = GetN();
    if (npar==1)
        array.push_back(GetBool());
    else
        for (auto i=0; i<npar; ++i)
            array.push_back(GetBool(i));
    return array;
}

std::vector<int> LKParameter::GetVInt() const
{
    std::vector<int> array;
    auto npar = GetN();
    if (npar==1)
        array.push_back(GetInt());
    else
        for (auto i=0; i<npar; ++i)
            array.push_back(GetInt(i));
    return array;
}

std::vector<double> LKParameter::GetVDouble() const
{
    std::vector<double> array;
    auto npar = GetN();
    if (npar==1)
        array.push_back(GetDouble());
    else
        for (auto i=0; i<npar; ++i)
            array.push_back(GetDouble(i));
    return array;
}

std::vector<TString> LKParameter::GetVString() const
{
    std::vector<TString> array;
    auto npar = GetN();
    if (npar==1)
        array.push_back(GetString());
    else
        for (auto i=0; i<npar; ++i)
            array.push_back(GetString(i));
    return array;
}

void LKParameter::ProcessTypeError(TString type) const
{
    lx_error << "Parameter " << fName << " = " << fValue << " is not convertable to " << type << std::endl;
    gApplication -> Terminate();
}

bool LKParameter::CheckFormulaValidity(TString formula, bool isInt) const
{
    if (isInt && formula.Index(".")>=0)
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
