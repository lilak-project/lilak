#include "TObjArray.h"
#include "TObjString.h"
#include "TFormula.h"
#include "TApplication.h"
#include "TColor.h"
#include <regex>
#include <ctime>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "LKMisc.h"

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

LKParameter::LKParameter(TString value)
{
    if (value.EndsWith(".txt") || value.EndsWith(".par") || value.EndsWith(".mac") || value.EndsWith(".conf"))
        ReadFile(value, 0);
    else
        SetPar("lkpar", value, value, "");
}

LKParameter::LKParameter(TString fileName, int lineNo)
{
    ReadFile(fileName, lineNo);
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

void LKParameter::ReadFile(TString fileName, int lineNo)
{
    bool comment_from_here = false;
    TString name, raw, comment;

    ifstream file(fileName);
    if (file.is_open()==false) {
        lk_info << "Cannot open " << fileName << endl;
        lk_info << "Looking for " << fileName << " in lilak/common/" << endl;
        file.clear();
        TString newFileName = TString(LILAK_PATH) + "/common/" + fileName;
        file.open(newFileName);
        if (file.is_open()==false)
            lk_info << "Cannot open " << fileName << "!!" << endl;
        return;
    }

    std::string line;
    int linesToRemove = lineNo;
    int countLines = 0;
    while (linesToRemove>0) {
        if (file.eof())
            lk_error << "File has ended at line# " << countLines << ". Cannot reach request line# " << lineNo << "!" << endl;
        std::getline(file, line);
        linesToRemove--;
        countLines++;
    }
    if (file.eof())
        lk_error << "File has ended at line# " << countLines << ". Cannot reach request line# " << lineNo << "!" << endl;
    std::getline(file, line);
    std::stringstream ss(line);
    std::string stoken;
    ss >> name;
    while (ss >> stoken)
    {
        TString token = stoken;
        token.ReplaceAll(",","");
        token.ReplaceAll(" ","");
        if (token=="#") {
            comment_from_here = true;
            continue;
        }
        else if (comment_from_here) {
            comment = comment + token + " ";
        }
        else {
            raw = raw + token + " ";
        }
    }
    if (raw.IsNull()==false) raw = raw(0,raw.Sizeof());
    if (comment.IsNull()==false) comment = comment(0,comment.Sizeof());

    SetPar(name, raw, raw, comment);
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

    //int iSlash = fName.Index("/");
    int iSlash = fName.First('/');
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
    if (name.EndsWith(")") && value.IsDec()) {
        int i1 = name.Index("(");
        int i2 = name.Index(")");
        if (i2>i1) {
            int nn = i2 - i1 - 1;
            TString unit = name(name.Index("(")+1,nn);
            TranslateUnit(value,unit);
        }
    }
}

void LKParameter::TranslateUnit(TString value, TString unit)
{
    if (unit=="t") {
        time_t start_time = value.Atoll();
        struct tm *now = localtime(&start_time);
        TString ymd = Form("%04d.%02d.%02d",now->tm_year+1900,now->tm_mon+1,now->tm_mday);
        TString hms = Form("%02d:%02d:%02d",now->tm_hour,now->tm_min,now->tm_sec);
        fValue = ymd + " " + hms;
    }
}

void LKParameter::SetValue(TString line)
{
    if (line[0]=='\"' && line[line.Length()-1]=='\"') {
        TString line1 = TString(line(1,line.Length()-2));
        if (line1.Index("\"")<0)
            line = line1;
    }

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
        } else if (c==' ' || c==',' || c=='\t') {
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
    e_cout << GetLine(option) << std::endl;
}

Int_t LKParameter::Compare(const TObject *obj) const
{
    auto parameter = (LKParameter *) obj;

    const int sortLatter = 1;
    const int sortEarlier = -1;
    const int sortSame = 0;

    TString cGroup = GetGroup(-1);
    TString iGroup = parameter -> GetGroup(-1);
    TString cName = GetLastName();
    TString iName = parameter -> GetLastName();
    int iCompare = parameter -> GetCompare();

    if (!cName.IsNull() && iName.IsNull()) { return sortEarlier; }
    if (cName.IsNull() && !iName.IsNull()) {  return sortLatter; }
    if (cName.IsNull() && iName.IsNull()) { eturn sortSame; }

    if (cGroup=="LKRun" && iGroup=="LKRun")
    {
        if      (fCompare<iCompare) { return sortEarlier; }
        else if (fCompare>iCompare) {  return sortLatter; }
        else { eturn sortSame; }
    }
    else if (cGroup=="LKRun") { return sortEarlier; }
    else if (iGroup=="LKRun") {  return sortLatter; }
    else if (cGroup<iGroup) { return sortEarlier; }
    else if (cGroup>iGroup) {  return sortLatter; }
    else {
        if      (fCompare<iCompare) { return sortEarlier; }
        else if (fCompare>iCompare) {  return sortLatter; }
        else {
            if      (cName<iName) { return sortEarlier; }
            else if (cName>iName) {  return sortLatter; }
        }
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

int LKParameter::GetColor(int idx)
{
    TString value = fValue;
    if (idx>=0) value = fValueArray[idx];
    TString value0  = value;

    value.ReplaceAll(" ","");
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
        if (!value.IsDec()) {
            if (CheckFormulaValidity(value,true)) return TFormula("formula",value).Eval(0);
            else ProcessTypeError("color", value);
        }
    }
    else if (value.Sizeof()==7)
    {
        int r, g, b;
        std::stringstream ss;

        ss << std::hex << value(0, 2);
        ss >> r; ss.clear(); ss.str("");
        ss << std::hex << value(2, 2);
        ss >> g; ss.clear(); ss.str("");
        ss << std::hex << value(4, 2);
        ss >> b;

        Double_t red   = r / 255.0;
        Double_t green = g / 255.0;
        Double_t blue  = b / 255.0;

        int colorIndex = TColor::GetFreeColorIndex();
        e_info << "Creating TColor " << colorIndex << " (" << red << ", " << green << ", " << blue << ")" << endl;
        new TColor(colorIndex, red, green, blue);
        value = Form("%d", colorIndex);
    }

    if (idx>=0)
        fValueArray[idx] = value;
    else
        fValue = value;

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

std::vector<int> LKParameter::GetIntRange() const
{
    TString value = fValue;
    auto array = value.Tokenize(",");
    TIter nextBlock(array);
    vector<int> numberList;
    while (auto block = (TObjString*) nextBlock())
    {
        TString stringb = block -> GetString();
        stringb = stringb.Strip(TString::kLeading);
        stringb = stringb.Strip(TString::kTrailing);
        int idx_c = stringb.Index(":");
        int idx_e = stringb.Index("!");
        if (idx_c>0) {
            auto string12 = stringb.Tokenize(":");
            if (string12->GetEntries()!=2) { e_error << stringb << " ?" << endl; return numberList; }
            TString string1 = ((TObjString*) string12->At(0))->GetString();
            TString string2 = ((TObjString*) string12->At(1))->GetString();
            if (string1.IsDec()==false) { e_error << string1 << " ?" << endl; return numberList; }
            if (string2.IsDec()==false) { e_error << string2 << " ?" << endl; return numberList; }
            int number1 = string1.Atoi();
            int number2 = string2.Atoi();
            for (auto number=number1; number<=number2; ++number)
                numberList.push_back(number);
        }
        else if (idx_e==0) {
            TString string1(stringb(1,stringb.Sizeof()-2));
            if (string1.IsDec()==false) { e_error << string1 << " ?" << endl; return numberList; }
            int number_to_remove = string1.Atoi();
            auto it = std::find(numberList.begin(), numberList.end(), number_to_remove);
            if (it != numberList.end()) {
                numberList.erase(it);  // Remove the first occurrence
            }
        }
        else {
            if (stringb.IsDec()==false) { e_error << stringb << " ?" << endl; return numberList; }
            numberList.push_back(stringb.Atoi());
        }
    }
    return numberList;
}

std::vector<bool> LKParameter::GetVBool(int n) const
{
    std::vector<bool> array;
    auto npar = GetN();
    if (npar==1)
        array.push_back(GetBool());
    else {
        if (n>0 && n<npar) npar = n;
        if (n<0 && (-n)<npar) npar = npar + n;
        for (auto i=0; i<npar; ++i)
            array.push_back(GetBool(i));
    }
    return array;
}

std::vector<int> LKParameter::GetVInt(int n) const
{
    std::vector<int> array;
    auto npar = GetN();
    if (npar==1)
        array.push_back(GetInt());
    else {
        if (n>0 && n<npar) npar = n;
        if (n<0 && (-n)<npar) npar = npar + n;
        for (auto i=0; i<npar; ++i)
            array.push_back(GetInt(i));
    }
    return array;
}

std::vector<int> LKParameter::GetVColor(int n)
{
    std::vector<int> array;
    auto npar = GetN();
    if (npar==1)
        array.push_back(GetColor());
    else {
        if (n>0 && n<npar) npar = n;
        if (n<0 && (-n)<npar) npar = npar + n;
        for (auto i=0; i<npar; ++i)
            array.push_back(GetColor(i));
    }
    return array;
}

std::vector<double> LKParameter::GetVDouble(int n) const
{
    std::vector<double> array;
    auto npar = GetN();
    if (npar==1)
        array.push_back(GetDouble());
    else {
        if (n>0 && n<npar) npar = n;
        if (n<0 && (-n)<npar) npar = npar + n;
        for (auto i=0; i<npar; ++i)
            array.push_back(GetDouble(i));
    }
    return array;
}

std::vector<TString> LKParameter::GetVString(int n) const
{
    std::vector<TString> array;
    auto npar = GetN();
    if (npar==1)
        array.push_back(GetString());
    else {
        if (n>0 && n<npar) npar = n;
        if (n<0 && (-n)<npar) npar = npar + n;
        for (auto i=0; i<npar; ++i)
            array.push_back(GetString(i));
    }
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

TString LKParameter::GetFirstName() const
{
    TString firstName = fName;
    int iSlash = fName.First('/');
    if (iSlash>=0) {
        firstName = fName(iSlash+1,fName.Sizeof()-iSlash-2);
    }
    return firstName;
}

TString LKParameter::GetLastName() const
{
    TString lastName = fName;
    int iSlash = fName.Last('/');
    if (iSlash>=0) {
        lastName = fName(iSlash+1,fName.Sizeof()-iSlash-2);
    }
    return lastName;
}

TString LKParameter::GetGroup(int ith) const
{
    TString name = fName.Data();
    TString group;
    int iBreak = 0;
    int countBreak = 0;

    if (ith<0)
        group = fName(0,fName.Last('/'));
    else
    {
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
    }

    return group;
}

TString LKParameter::GetLine(TString printOptions) const
{
    bool showRaw          = LKMisc::CheckOption(printOptions,"r",true);
    bool showEval         = LKMisc::CheckOption(printOptions,"e",true);
    bool showParComments  = LKMisc::CheckOption(printOptions,"c",true);
    bool nptoolFormat     = LKMisc::CheckOption(printOptions,"n",true);
    bool useMainName      = LKMisc::CheckOption(printOptions,"m",true);
    bool comIsPercent     = LKMisc::CheckOption(printOptions,"%",true);
    bool commentOutAll    = LKMisc::CheckOption(printOptions,"coa",true);
    int  ntab             =(LKMisc::CheckOption(printOptions,"1",true)?1:0);
    bool showBothRawEval = (showRaw&&showEval);
    if (!showRaw&&!showEval) showEval = true;

    if (nptoolFormat) {
        showRaw = false;
        showEval = true;
        showParComments = false;
        showBothRawEval = false;
        useMainName = true;
        comIsPercent = true;
    }

    TString com = "#";
    if (comIsPercent) com = "%";

    if (IsLineComment()) {
        TString line = com + com + " " + fComment;
        return line;
    }

    TString name = fName;
    if (useMainName)
        name = GetLastName();

    int nwidth = 30;
    if      (name.Sizeof()>60)  nwidth = 80;
    else if (name.Sizeof()>50)  nwidth = 60;
    else if (name.Sizeof()>40)  nwidth = 50;
    else if (name.Sizeof()>30)  nwidth = 40;
    else                        nwidth = 30;

    if (name.Sizeof()<nwidth) {
        auto n = nwidth - name.Sizeof();
        for (auto i=0; i<n; ++i)
            name = name  + " ";
    }

    TString comment = fComment;
    TString value = fValue;
    if (showBothRawEval) {
        value = fTitle;
        comment = TString("--> ") + fValue + " " + com + " " + comment;
    }
    else if (showEval) value = fValue;
    else if (showRaw) value = fTitle;
    else value = fValue;

    int vwidth = 30;
    if      (value.Sizeof()>60)  vwidth = 80;
    else if (value.Sizeof()>50)  vwidth = 60;
    else if (value.Sizeof()>40)  vwidth = 50;
    else if (value.Sizeof()>30)  vwidth = 40;
    else                         vwidth = 30;

    bool valueIsEmpty = false;
    if (nptoolFormat) {
        if (value.IsNull()) valueIsEmpty = true;
        value = TString("= ") + value;
        value.ReplaceAll(",","");
    }

    TString line = name + " " + value;
    if (showParComments && comment.IsNull()==false)
        line = line + "  " + com + " " + comment;

    if (ntab==1) line = TString("    ") + line;

    if (IsCommentOut() || (nptoolFormat && valueIsEmpty) || commentOutAll)
        line = com + line;

    return line;
}
