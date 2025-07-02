#ifndef LKPARAMETERCONTAINER
#define LKPARAMETERCONTAINER

//#define DEBUG_ADDPAR

#include <fstream>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <vector>

#ifdef LILAK_BUILD_JSONCPP
#include "json.h"
#endif

#include "TObjArray.h"
#include "TNamed.h"
#include "TParameter.h"
#include "TFormula.h"
#include "TVector3.h"
#include "TCut.h"
#include "TCutG.h"

#include "LKVector3.h"
#include "LKLogger.h"
#include "LKParameter.h"
#include "LKCut.h"
#include "LKBinning.h"

typedef LKVector3::Axis axis_t;

using namespace std;

/**
 * ## LKParameterContainer
 *  LKParameterContainer is a list of parameters defined by LKParameter
 *  The key features of LKParameter are [group], [name], [value], [comment].
 *  When parameter is defined from text file, line should have a form : [header] [name] [value] [comment]
 *
 * ## [comment]
 *  - Comments should start with "#". Lines starting with "#" are defined as hidden line comment.
 *
 * ## [header]
 *  [header] defines the characteristic of line.
 *  - < : Input parameter file. The parameter-value will be given as anohter input parameter file.
 *  - @ : Conditional parameter. May be used when user wants to define parameter, only when group name is defined as parameter-name or parameter-value, ex) @group/name value
 *  - [tab] : See [group] section below.
 *
 * ## [name] and group
 *  - [name] should not contain empty spaces.
 *  - [name] can set group using "/". ex) group/name
 *  - To add additional parameter file, start [name] with "<". ex) <common_parameters   path/to/additional/config.mac
 *
 * ## [value]
 *  - [value] can be a single value or a list of values.
 *  - If [value] is a list of values, they should be separated by empty spaces.
 *  - [value] can reference other parameters values using {...}. ex) {[name]}, {[name][1]}.
 *  - [value] can reference environment variable using e{...}. ex) e{ROOTSYS}, e{HOME}, e{LILAK_PATH}
 *
 * ## Folloing symbols should not be included in name / value
 *   - name  : < @ & ! = { } / *
 *   - value : { } =
 *
 * ## [group]
 *   - If a [name] ends with '/' and do not have [value], it will define [group].
 *   - To define parameter under [group], parameter should have [tab], with size of [tab] larger than the [tab] of line where group was defined.
 *   - Size of [tab] should not be different under same group.
 *   - Another [group] can be defined under [group] definition
 *   - Final name of the parameter will have [group]/[name]
 *
 * ## example parameter file
 * @code{.mac}
 *  # example parameter file
 *
 *  <input_file  path/file/to/add.par
 *  LKRun/RunName  lilak 1 sim   # this parameter will last only within the first lilak output file
 *
 *  using       option1
 *  option1/par  value1  # parameter "par" will be set as "value1" because it was defined previously
 *  option2/par  value2  # parameter "par" will not be set
 *  option3/par  value3  # parameter "par" will not be set
 *
 *  title        common parameter definitions
 *  dimension    50 60 70
 *  length       {dimension[2]}+30  # parameter value will be evaluated to 70+30 which is 100
 *  color        kRed+1  # parameter value will be evaluated to 633 when GetParColor is called.
 *
 *  persistency/  # group
 *      hit      false   # this parameter will be defined as "persistency/hit false"
 *      track    true    # this parameter will be defined as "persistency/track true"
 *
 *  color       kBlue+3 # parameter value will overwrite above parameter "color"
 *
 *  # sometimes user want to define parameter with same name for some reason...
 *  SameName   1
 *  SameName   2
 *  SameName   3
 * @endcode
 *
 * ## Get parameter
 *
 * The type of parameters are decided when Getter is called.
 * Featured types: TString, int, double, bool, TVector3, Color_t, Width_t, Size_t.
 *
 * GetParInt("size") will return parameter value 100 as int type,
 * GetParString("size") will return parameter value 100 as TString type,
 * GetParString("title") will return TString type "this is title"
 * GetParString("title",0) will return TString type "this"
 * GetParString("title",3) will return TString type "5"
 * GetParDouble("title",3) will return double type 5
 * GetParDouble("length") will return double type 80
 * GetParString("length") will return TString type "70+10"
 * GetParV3("dimension"), will return TVector3(50,60,70)
 * With GetParColor("color") will return Color_t(int) type 633
 *
 * GetParV3 use the first 3 values as the x, y, and z coordinates of the return value TVector3(x,y,z).
 * The root color keyword will be converted from kRed+1 to an integer value 633 when GetParColor is called.
 */

class LKParameterContainer : public TObjArray
{
    public:
        LKParameterContainer();
        LKParameterContainer(const char *parName); ///< Constructor with input parameter file name
        virtual ~LKParameterContainer() {}

        /**
         * Print to screen or file
         *
         * ## How to give option
         *
         * If file name is not given, parameter container will be printed on the screen. ex) Print()
         * If file name is given with an extension (".par") parameter container will be written to the file. ex) Print("file.par")
         * Options may be added with and addition to ":" or space. ex) Print("file.par:line#:par#"), Print("eval:par#")
         *
         * ## options
         *
         * - i : show line index
         * - l : show line comments
         * - t : print from TObjArray::Print()
         * - r : show raw parameter value
         * - e : evaluate and replace all unraveled variables with ({par},+,-,...)
         * - c : show parameter comments
         */
        virtual void Print(Option_t *option="i:l:e:c") const;
        void SaveAs(const char *filename, Option_t *option = "") const;
        LKParameterContainer *CloneParameterContainer(TString name="", bool addTemporary=false) const;

        void Recompile();

        bool IsEmpty() const; ///< Return true if empty
        int  FindAndRetrieveColumnValue(TString fileName, int searchColumn, TString searchValue, int getColumn, TString &getValue);
        void ReplaceEnvVariables(TString &val); ///< evaluate and replace all unraveled variables with ({par},+,-,...)
        void ReplaceVariables(TString &val); ///< evaluate and replace all unraveled variables with ({par},+,-,...)

        /**
         * Add parameter by given fileName.
         * If fileName does not include path, file will be searched in path/to/LILAK/input.
         *
         * fileName will also be registered as parameter. 
         * parameter name will be set as INPUT_PARAMETER_FILE[fNumInputFiles]
         */
        virtual Int_t AddFile(TString parName, TString fileName); ///< Add parameter file
        virtual Int_t AddFile(TString fileName, bool addEvalOnly=false) { return AddFile("", fileName); }
        virtual Int_t AddParameterContainer(LKParameterContainer *parc, bool addEvalOnly=false); ///< Add parameter container pointer
        Int_t GetNumInputFiles() const { return fNumInputFiles; } ///< Get number of input parameter files
        bool SearchAndAddPar(TString dirName="");

        LKParameter* GetParameter(int idx) { return (LKParameter*) At(idx); }

#ifdef LILAK_BUILD_JSONCPP
        Int_t  AddJsonTree(const Json::Value &value, TString treeName="");
#endif

        Int_t  AddLine(std::string line); ///< Set parameter by line
        Bool_t AddPar(TString name, TString val, TString comment=""); ///< Set parameter TString
        Bool_t AddPar(TString name, Int_t val, TString comment="")    { return AddPar(name,Form("%d",val),comment); } ///< Set parameter Int_t
        Bool_t AddPar(TString name, Long64_t val, TString comment="") { return AddPar(name,Form("%lld",val),comment); } ///< Set parameter Double_t
        Bool_t AddPar(TString name, Double_t val, TString comment="") { return AddPar(name,Form("%f",val),comment); } ///< Set parameter Double_t

        Bool_t CheckPar(TString name) const;
        Bool_t CheckValue(TString name) const;
        Bool_t CheckParTypeBool  (TString name, int idx=-1) const { return FindPar(name,true) -> CheckTypeBool  (idx); }
        Bool_t CheckParTypeInt   (TString name, int idx=-1) const { return FindPar(name,true) -> CheckTypeInt   (idx); }
        Bool_t CheckParTypeLong  (TString name, int idx=-1) const { return FindPar(name,true) -> CheckTypeLong  (idx); }
        Bool_t CheckParTypeDouble(TString name, int idx=-1) const { return FindPar(name,true) -> CheckTypeDouble(idx); }
        Bool_t CheckParTypeString(TString name, int idx=-1) const { return FindPar(name,true) -> CheckTypeString(idx); }
        Bool_t CheckParTypeColor (TString name, int idx=-1) const { return FindPar(name,true) -> CheckTypeColor (idx); }
        Bool_t CheckParTypeV3    (TString name) const             { return FindPar(name,true) -> CheckTypeV3    ();    }
        Bool_t CheckParTypeX     (TString name) const { return CheckParTypeDouble(name,0); }
        Bool_t CheckParTypeY     (TString name) const { return CheckParTypeDouble(name,1); }
        Bool_t CheckParTypeZ     (TString name) const { return CheckParTypeDouble(name,2); }
        Bool_t CheckParTypeStyle (TString name, int idx=-1) const { return CheckParTypeInt(name,idx); }
        Bool_t CheckParTypeWidth (TString name, int idx=-1) const { return CheckParTypeInt(name,idx); }
        Bool_t CheckParTypeSize  (TString name, int idx=-1) const { return CheckParTypeDouble(name,idx); }
        Bool_t CheckParTypeAxis  (TString name, int idx=-1) const { return FindPar(name,true) -> CheckTypeAxis(idx); }

        Int_t     GetParIndex (TString name) const;
        TString   GetParRaw   (TString name) const             { return FindPar(name,true) -> GetRaw   ();    }
        Int_t     GetParN     (TString name) const             { return FindPar(name,true) -> GetN     ();    } ///< Get number of parameters in array of given name.
        Bool_t    GetParBool  (TString name, int idx=-1) const { return FindPar(name,true) -> GetBool  (idx); } ///< Get parameter in Bool_t
        Int_t     GetParInt   (TString name, int idx=-1) const { return FindPar(name,true) -> GetInt   (idx); } ///< Get parameter in Int_t
        Long64_t  GetParLong  (TString name, int idx=-1) const { return FindPar(name,true) -> GetLong  (idx); } ///< Get parameter in Long64_t
        Double_t  GetParDouble(TString name, int idx=-1) const { return FindPar(name,true) -> GetDouble(idx); } ///< Get parameter in Double_t
        TString   GetParString(TString name, int idx=-1) const { return FindPar(name,true) -> GetString(idx); } ///< Get parameter in TString
        Int_t     GetParColor (TString name, int idx=-1)       { return FindPar(name,true) -> GetColor (idx); } ///< Get parameter in Color_t
        TVector3  GetParV3    (TString name) const             { return FindPar(name,true) -> GetV3    ();    } ///< Get parameter in TVector
        Double_t  GetParX     (TString name) const             { return GetParDouble(name,0); }
        Double_t  GetParY     (TString name) const             { return GetParDouble(name,1); }
        Double_t  GetParZ     (TString name) const             { return GetParDouble(name,2); }
        Int_t     GetParStyle (TString name, int idx=-1) const { return GetParInt(name,idx); }
        Int_t     GetParWidth (TString name, int idx=-1) const { return GetParInt(name,idx); }
        Double_t  GetParSize  (TString name, int idx=-1) const { return GetParDouble(name,idx); }
        axis_t    GetParAxis  (TString name, int idx=-1) const { return FindPar(name,true) -> GetAxis(); }
        LKCut*    GetParCut   (TString name);
        LKBinning GetBinning  (TString name);
        std::vector<int>     GetParIntRange(TString name         ) const { return FindPar(name,true) -> GetIntRange(); } 
        std::vector<bool>    GetParVBool   (TString name, int n=0) const { return FindPar(name,true) -> GetVBool  (n);  } ///< Return vector of comma separated values in bool type. n=0: all, n>0: upto n-elements, n<0: remove n elements from the back.
        std::vector<int>     GetParVInt    (TString name, int n=0) const { return FindPar(name,true) -> GetVInt   (n);  } ///< Return vector of comma separated values in int type. n=0: all, n>0: upto n-elements, n<0: remove n elements from the back.
        std::vector<double>  GetParVDouble (TString name, int n=0) const { return FindPar(name,true) -> GetVDouble(n);  } ///< Return vector of comma separated values in double type. n=0: all, n>0: upto n-elements, n<0: remove n elements from the back.
        std::vector<TString> GetParVString (TString name, int n=0) const { return FindPar(name,true) -> GetVString(n);  } ///< Return vector of comma separated values in TString type. n=0: all, n>0: upto n-elements, n<0: remove n elements from the back.
        std::vector<int>     GetParVStyle  (TString name, int n=0) const { return GetParVInt(name, n); } ///< Return vector of comma separated values in Style_t type. n=0: all, n>0: upto n-elements, n<0: remove n elements from the back.
        std::vector<int>     GetParVWidth  (TString name, int n=0) const { return GetParVInt(name, n); } ///< Return vector of comma separated values in Width_t type. n=0: all, n>0: upto n-elements, n<0: remove n elements from the back.
        std::vector<int>     GetParVColor  (TString name, int n=0) const { return GetParVInt(name, n); } ///< Return vector of comma separated values in Colot_t type. n=0: all, n>0: upto n-elements, n<0: remove n elements from the back.
        std::vector<double>  GetParVSize   (TString name, int n=0) const { return GetParVDouble(name, n); } ///< Return vector of comma separated values in Size_t type. n=0: all, n>0: upto n-elements, n<0: remove n elements from the back.

        /// UpdatePar() will update given value, if parameter with name and idx exist. If not, value will not be changed.
        void UpdatePar(Bool_t    &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParBool  (name,idx); }
        void UpdatePar(Int_t     &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParInt   (name,idx); } ///< See UpdatePar(Bool_t, TString, int)
        void UpdatePar(Long64_t  &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParLong  (name,idx); } ///< See UpdatePar(Bool_t, TString, int)
        void UpdatePar(Double_t  &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParDouble(name,idx); } ///< See UpdatePar(Bool_t, TString, int)
        void UpdatePar(TString   &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParString(name,idx); } ///< See UpdatePar(Bool_t, TString, int)
        void UpdatePar(axis_t    &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParAxis  (name,idx); } ///< See UpdatePar(Bool_t, TString, int)
        void UpdatePar(TVector3  &value, TString name)             const { if (CheckPar(name)) value = GetParV3    (name);     } ///< See UpdatePar(Bool_t, TString, int)
        void UpdatePar(LKBinning &value, TString name)             const { int n; double x1, x2; if (UpdateBinning(name, n, x1, x2)) value.SetXNMM(n, x1, x2); }
        bool UpdateBinning(TString name, Int_t &n, Double_t &x1, Double_t &x2) const;
        void UpdateV3(TString name, Double_t &x, Double_t &y, Double_t &z) const;

        Bool_t               InitPar(Bool_t    dfValue, TString name, int idx=-1) const { if (CheckParWithValue(name,dfValue)) return GetParBool   (name, idx); return dfValue; }
        Int_t                InitPar(Int_t     dfValue, TString name, int idx=-1) const { if (CheckParWithValue(name,dfValue)) return GetParInt    (name, idx); return dfValue; }
        Long64_t             InitPar(Long64_t  dfValue, TString name, int idx=-1) const { if (CheckParWithValue(name,dfValue)) return GetParLong   (name, idx); return dfValue; }
        Double_t             InitPar(Double_t  dfValue, TString name, int idx=-1) const { if (CheckParWithValue(name,dfValue)) return GetParDouble (name, idx); return dfValue; }
        TString              InitPar(TString   dfValue, TString name, int idx=-1) const { if (CheckParWithValue(name,dfValue)) return GetParString (name, idx); return dfValue; }
        axis_t               InitPar(axis_t    dfValue, TString name, int idx=-1) const { if (CheckParWithValue(name,dfValue)) return GetParAxis   (name, idx); return dfValue; }
        TVector3             InitPar(TVector3  dfValue, TString name) const             { if (CheckParWithValue(name,dfValue)) return GetParV3     (name);      return dfValue; }
        LKBinning            InitPar(LKBinning dfValue, TString name)                   { if (CheckParWithValue(name,dfValue)) return GetBinning   (name);      return dfValue; }
        std::vector<bool>    InitPar(std::vector<bool>    dfValue, TString name)  const { if (CheckParWithValue(name,dfValue)) return GetParVBool  (name);      return dfValue; }
        std::vector<int>     InitPar(std::vector<int>     dfValue, TString name)  const { if (CheckParWithValue(name,dfValue)) return GetParVInt   (name);      return dfValue; }
        std::vector<double>  InitPar(std::vector<double>  dfValue, TString name)  const { if (CheckParWithValue(name,dfValue)) return GetParVDouble(name);      return dfValue; }
        std::vector<TString> InitPar(std::vector<TString> dfValue, TString name)  const { if (CheckParWithValue(name,dfValue)) return GetParVString(name);      return dfValue; }
        TString              InitPar(const char* dfValue, TString name, int idx=-1) const { return InitPar(TString(dfValue), name, idx); }

        LKParameterContainer* CreateGroupContainer(TString nameGroup); ///< Create new LKParameterContainer that contains all parameters which has group name [name group]
        LKParameterContainer* CreateMultiParContainer(TString parNameGiven); ///< Create new LKParameterContainer that contains all parameters which has name [parNameGiven]
        LKParameterContainer* CreateAnyContainer(TString any); ///< Create new LKParameterContainer that contains all parameters which has [group name]==[any] or [full name]==[any].

    public:
        /**
         * - type:
         * s: Standard
         * l: Legacy
         * r: Rewrite
         * m: Multiple
         * t: Temporary
         * i: InputFile
         * c: Conditional
         * #: LineComment
         * /: CommentOut
         */
        void Require(TString name, TString value, TString comment, TString type="", int compare=-1);
        void Require(TString name, int value, TString comment, TString type="", int compare=-1) { Require(name, Form("%d",value), comment, type, compare); }
        void Require(TString name, double value, TString comment, TString type="", int compare=-1) { Require(name, Form("%f",value), comment, type, compare); }

        void SetParType(TString name, TString type) { auto par = FindParFree(name, false); if (par!=nullptr) par -> SetType(type); }
        LKParameter *FindPar(TString givenName, bool terminateIfNull=false) const;

    protected:
        LKParameter *SetPar    (TString name, TString  raw, TString val, TString comment, int parameterType=1);
        LKParameter *SetPar    (TString name, TString  val, TString comment="") { return SetPar(name,val,val,comment); } ///< Set parameter string
        LKParameter *SetPar    (TString name, Int_t    val, TString comment="") { return SetPar(name,Form("%d",val),Form("%d",val),comment); } ///< Set parameter Int_t
        LKParameter *SetPar    (TString name, Double_t val, TString comment="") { return SetPar(name,Form("%f",val),Form("%f",val),comment); } ///< Set parameter Double_t
        LKParameter *SetParCont(TString name);
        LKParameter *SetParFile(TString name);
        LKParameter *SetInputFile(TString name, TString value);
        LKParameter *SetLineComment(TString comment);

        LKParameter *FindParFree(TString givenName, bool terminateIfNull=false);

        TString ToString(Bool_t               dfValue) const;
        TString ToString(Int_t                dfValue) const;
        TString ToString(Long64_t             dfValue) const;
        TString ToString(Double_t             dfValue) const;
        TString ToString(TString              dfValue) const;
        TString ToString(axis_t               dfValue) const;
        TString ToString(TVector3             dfValue) const;
        TString ToString(LKBinning            dfValue) const;
        TString ToString(std::vector<bool>    dfValue) const;
        TString ToString(std::vector<int>     dfValue) const;
        TString ToString(std::vector<double>  dfValue) const;
        TString ToString(std::vector<TString> dfValue) const;

        TString InsertValueInName(TString name, TString value) const;

        bool CheckParWithValue(TString name, Bool_t               dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, Int_t                dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, Long64_t             dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, Double_t             dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, TString              dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, axis_t               dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, TVector3             dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, LKBinning            dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, std::vector<bool>    dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, std::vector<int>     dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, std::vector<double>  dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }
        bool CheckParWithValue(TString name, std::vector<TString> dfValue) const { return CheckPar(InsertValueInName(name,ToString(dfValue))); }

    private:
        void ProcessTypeError(TString name, TString val, TString type) const;
        bool CheckFormulaValidity(TString formula, bool isInt=false) const;
        double Eval(TString formula) const;

        Int_t fNumInputFiles = 0;
        Int_t fRank = 0;

    private:
        TString fCurrentGroupName;
        Int_t fPreviousTabSize = 0;
        vector<Int_t> fTabSizeArray;
        vector<TString> fGroupNameArray;

    public:
        void SetCollectParameters(bool collect);
        void PrintCollection(TString fileName="");
        void SetIsNewCollection() { fThisIsNewCollection = true; }
        LKParameterContainer *GetCollectedParameterContainer() { return fCollectedParameterContainer; }

    private:
        void CollectParameter();
        bool fParameterCollectionMode = false;
        bool fThisIsNewCollection = false;
        LKParameterContainer *fCollectedParameterContainer = nullptr;


#ifdef LILAK_BUILD_JSONCPP
        Json::Value fJsonValues;
#endif

    ClassDef(LKParameterContainer, 4)
};

#endif
