#include "TObjArray.h"
#include "TObjString.h"
#include "TFormula.h"
#include "TApplication.h"
#include <regex>

#include "LKParameter.h"

LKParameter::LKParameter()
{
}

LKParameter::LKParameter(int parameterType)
{
    Clear();
    fType = parameterType;
}

LKParameter::LKParameter(TString name, TString raw, TString value, TString comment, int parameterType, int compare)
{
    SetPar(name, raw, value, comment, parameterType, compare);
}

LKParameter::~LKParameter()
{
}

void LKParameter::SetLineComment(TString comment)
{
    Clear();
    SetIsLineComment();
    fComment = comment;
}

void LKParameter::SetPar(TString name, TString raw, TString value, TString comment, int parameterType, int compare)
{
    Clear();
    fName = name;
    fTitle = raw;
    fComment = comment;
    fType = parameterType;
    if (compare>=0)
        fCompare = compare;

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

    SetValue(value);
}

void LKParameter::SetValue(TString line)
{
    if (line[0]=='\"' && line[line.Length()-1]=='\"')
        line = TString(line(1,line.Length()-2));

    fValue = line;
    fValueArray.clear();

    TString currentToken;
    bool insideQuotes = false;

    for (int i=0; i<line.Length(); ++i)
    {
        char c = line[i];

        if (c == '\"') {
            insideQuotes = !insideQuotes; // Toggle insideQuotes when encountering a double quote
            //if (!insideQuotes) {
            //    // If closing quote, push the current token to the list and reset it
            //    tokens.push_back(currentToken);
            //    currentToken.Clear();
            //}
        } else if (insideQuotes) {
            // If inside quotes, continue adding characters to currentToken
            currentToken += c;
        } else if (c==' ' || c==',') {
            // If encountering space or comma outside quotes, finalize the current token
            if (!currentToken.IsNull()) {
                fValueArray.push_back(currentToken);
                currentToken.Clear();
            }
        } else {
            // Continue adding characters to currentToken
            currentToken += c;
        }
    }

    // If any token remains after the loop, add it to the tokens list
    if (!currentToken.IsNull()) {
        fValueArray.push_back(currentToken);
    }
    fNumValues = fValueArray.size();
}

void LKParameter::SetValueAt(int i, TString value)
{
    auto numValues = fValueArray.size();
    if (i>numValues)
        return;

    TString valueUpdate;
    if (numValues==0 && i==0)
        valueUpdate = value;
    else if (i==numValues)
        valueUpdate = valueUpdate + " " + value;
    else
    {
        for (auto iVal=0; iVal<numValues; ++iVal)
        {
            if (iVal>0)  valueUpdate =+ " ";
            if (iVal==i) valueUpdate =+ value;
            else         valueUpdate =+ fValueArray[iVal];
        }
    }

    SetValue(valueUpdate);
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
    SetIsStandard();
}

void LKParameter::Print(Option_t *option) const
{
    e_cout << GetLine() << std::endl;
}

Int_t LKParameter::Compare(const TObject *obj) const
{
    auto parameter = (LKParameter *) obj;

    const int sortLatter = 1;
    const int sortEarlier = -1;
    const int sortSame = 0;

    TString iGroup = parameter -> GetGroup();
    TString iName = parameter -> GetName();
    int iCompare = parameter -> GetCompare();

    if (!fName.IsNull() && iName.IsNull()) { return sortEarlier; }
    if (fName.IsNull() && !iName.IsNull()) { return sortLatter; }
    if (fName.IsNull() && iName.IsNull()) { return sortSame; }

    if (fGroup=="LKRun" && iGroup=="LKRun")
    {
        if      (fCompare<iCompare) { return sortEarlier; }
        else if (fCompare>iCompare) { return sortLatter; }
        else { return sortSame; }
    }
    else if (fGroup=="LKRun") { return sortEarlier; }
    else if (iGroup=="LKRun") { return sortLatter; }
    else if (fGroup<iGroup) { return sortEarlier; }
    else if (fGroup>iGroup) { return sortLatter; }
    else {
        if      (fCompare<iCompare) { return sortEarlier; }
        else if (fCompare>iCompare) { return sortLatter; }
    }

    return sortSame;
}

bool LKParameter::CheckTypeInt(int idx) const
{
    TString value = ((idx>=0) ? fValue : fValueArray[idx]);
    if (!value.IsDec()) {
        if (CheckFormulaValidity(value,true)) return true;
        else return false;
    }
    return true;
}

bool LKParameter::CheckTypeLong(int idx) const
{
    TString value = ((idx>=0) ? fValue : fValueArray[idx]);
    if (!value.IsDec()) {
        if (CheckFormulaValidity(value,true)) return true;
        else return false;
    }
    return true;
}

bool LKParameter::CheckTypeBool(int idx) const
{
    TString value = ((idx>=0) ? fValue : fValueArray[idx]);
    value.ToLower();
    if (value=="true"||value=="1"||value=="false"||value=="0")
        return true;
    return false;
}

bool LKParameter::CheckTypeDouble(int idx) const
{
    TString value = ((idx>=0) ? fValue : fValueArray[idx]);
    if (!value.IsFloat()) {
        if (CheckFormulaValidity(value,true)) return true;
        else return false;
    }
        return false;
    return true;
}

bool LKParameter::CheckTypeString(int idx) const { return true; }

bool LKParameter::CheckTypeColor(int idx) const
{
    TString value = ((idx>=0) ? fValue : fValueArray[idx]);
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
    if (!value.IsDec()) {
        if (CheckFormulaValidity(value,true)) return true;
        else return false;
    }
    return true;
}

bool LKParameter::CheckTypeAxis(int idx) const
{
    TString value = ((idx>=0) ? fValue : fValueArray[idx]);
         if (value== "x") return true;
    else if (value== "y") return true;
    else if (value== "z") return true;
    else if (value=="-x") return true;
    else if (value=="-y") return true;
    else if (value=="-z") return true;
    else if (value== "i") return true;
    else if (value== "j") return true;
    else if (value== "k") return true;
    else if (value=="-i") return true;
    else if (value=="-j") return true;
    else if (value=="-k") return true;
    else if (value== "0") return true;
    return false;
}

bool LKParameter::CheckTypeV3() const
{
    if (!CheckTypeDouble(0)) return false;
    if (!CheckTypeDouble(1)) return false;
    if (!CheckTypeDouble(2)) return false;
    return true;
}

int LKParameter::GetInt(int idx) const
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];

    if (!value.IsDec()) {
        if (CheckFormulaValidity(value,true)) return TFormula("formula",value).Eval(0);
        else ProcessTypeError("int", value);
    }
    return value.Atoi();
}

Long64_t LKParameter::GetLong(int idx) const
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];

    if (!value.IsDec())
        ProcessTypeError("Long64_t", value);

    return value.Atoll();
}

bool LKParameter::GetBool(int idx) const
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];

    value.ToLower();
    if (value=="true"||value=="1") return true;
    else if (value=="false"||value=="0") return false;
    else 
        ProcessTypeError("bool", value);
    return true;
}

double LKParameter::GetDouble(int idx) const
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];

    if (!value.IsDec()) {
        if (CheckFormulaValidity(value,true)) return TFormula("formula",value).Eval(0);
        else ProcessTypeError("double", value);
    }
    return value.Atof();
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

    if (!value.IsDec()) {
        if (CheckFormulaValidity(value,true)) return TFormula("formula",value).Eval(0);
        else ProcessTypeError("color", value);
    }
    return value.Atoi();
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

void LKParameter::ProcessTypeError(TString type, TString value) const
{
    e_error << "Parameter " << fName << ", value = " << value << " is not convertable to " << type << std::endl;
    gApplication -> Terminate();
}

bool LKParameter::CheckFormulaValidity(TString formula, bool isInt) const
{
    std::regex formulaRegex("^[0-9\\(\\)\\+\\-\\*/Ee\\.]+$");
    return std::regex_match(formula.Data(), formulaRegex);
}

TString LKParameter::GetGroup(int ith) const
{
    TString name = fName.Data();
    TString group;
    int iBreak = 0;
    int countBreak = 0;
    while (iBreak>=0) {
        iBreak = name.Index("/",1,0,TString::kExact);
        if (iBreak<0)
            break;
        group = TString(name(0,iBreak));
        name = name(iBreak+1,name.Sizeof()-iBreak-2);
        if (countBreak==ith)
            break;
        countBreak++;
    }

    if (countBreak!=ith)
        return TString();

    return group;
}


TString LKParameter::GetLine(TString printOptions) const
{
    bool evaluatePar = true;
    bool showParComments = true;
    if (printOptions.Index("!eval")>=0) { evaluatePar = false; printOptions.ReplaceAll("!eval", ""); }
    if (printOptions.Index( "eval")>=0) { evaluatePar = true;  printOptions.ReplaceAll("eval", ""); }
    if (printOptions.Index("!par#")>=0) { showParComments = false; printOptions.ReplaceAll("!par#", ""); }
    if (printOptions.Index( "par#")>=0) { showParComments = true;  printOptions.ReplaceAll("par#", ""); }

    int nwidth = 30;
    if      (fName.Sizeof()>60) nwidth = 80;
    else if (fName.Sizeof()>50) nwidth = 60;
    else if (fName.Sizeof()>30) nwidth = 50;
    else                        nwidth = 30;

    int vwidth = 20;
    if      (fValue.Sizeof()>60) vwidth = 80;
    else if (fValue.Sizeof()>40) vwidth = 60;
    else if (fValue.Sizeof()>20) vwidth = 40;
    else                         vwidth = 20;

    if (IsLineComment()) {
        TString line = TString("# ") + fComment;
        return line;
    }

    TString name = fName;
    if (name.Sizeof()<nwidth) {
        auto n = nwidth - name.Sizeof();
        for (auto i=0; i<n; ++i)
            name = name  + " ";
    }

    TString value = fValue;
    if (!evaluatePar)
        value = fTitle;
    if (value.Sizeof()<vwidth) {
        auto n = vwidth - value.Sizeof();
        for (auto i=0; i<n; ++i)
            value = value + " ";
    }

    TString line = name + " " + value;
    if (showParComments)
        line = line + "  # " + fComment;

    if (IsCommentOut())
        line = TString("# ") + line;

    return line;
}
