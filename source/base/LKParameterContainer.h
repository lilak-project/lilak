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

#include "LKVector3.h"
#include "LKLogger.h"
#include "LKParameter.h"

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
 *  - * : Temporary parameter. May be used when user want to define parameter but do not want to transfer parameter from input file to output file through LKRun. ex) *name value
 *  - @ : Conditional parameter. May be used when user wants to define parameter, only when group name is defined as parameter-name or parameter-value, ex) @group/name value
 *  - & : Multiple parameter. May be defined many times. Can be grouped later using CreateGroupContainer() method.
 *  - ! : Overwrite parameter. Overwrite parameter if this header is used
 *  - [tab] : See [group] section below.
 *
 * ## [name] and group
 *  - [name] should not contain empty spaces.
 *  - [name] can set group using "/". ex) group/name
 *  - When adding another parameter file, start [name] with "<<". ex) <<, <<par_file, <</par_file
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
 *  - < : Input parameter file. The parameter-value will be given as anohter input parameter file.
 *  - * : Temporary parameter. May be used when user want to define parameter but do not want to transfer parameter from input file to output file through LKRun. ex) *name value
 *  - @ : Conditional parameter. May be used when user wants to define parameter, only when group name is defined as parameter-name or parameter-value, ex) @group/name value
 *  - & : Multiple parameter. May be defined many times. Can be grouped later using CreateGroupContainer() method.
 *  - ! : Overwrite parameter. Overwrite parameter if this header is used
 *
 * ## example parameter file
 * @code{.mac}
 *  # example parameter file
 *
 *  <input_file  path/file/to/add.par
 *  *LKRun/RunName  lilak 1 sim   # this parameter will last only within the first lilak output file
 *
 *  using       option1
 *  @option1/par  value1  # parameter "par" will be set as "value1" because it was defined previously
 *  @option2/par  value2  # parameter "par" will not be set 
 *  @option3/par  value3  # parameter "par" will not be set
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
 *  !color       kBlue+3 # parameter value will overwrite above parameter "color"
 *
 *  # sometimes user want to define parameter with same name for some reason...
 *  &SameName   1
 *  &SameName   2
 *  &SameName   3
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
         * - eval : evaluate and replace all unraveled variables with ({par},+,-,...)
         * - line# : show line comments
         * - par# : show parameter comments
         * - all : show all hidden parameters (not recommended to write file with this mode)
         * - raw : print from TObjArray::Print()
         */
        virtual void Print(Option_t *option = "") const;
        void SaveAs(const char *filename, Option_t *option = "") const;
        LKParameterContainer *CloneParameterContainer(TString name="") const;

        void Recompile();

        bool IsEmpty() const; ///< Return true if empty
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
        virtual Int_t AddFile(TString fileName) { return AddFile("", fileName); }
        virtual Int_t AddParameterContainer(LKParameterContainer *parc); ///< Add parameter container pointer
        Int_t GetNumInputFiles() const { return fNumInputFiles; } ///< Get number of input parameter files
        bool SearchAndAddPar(TString dirName="");

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

        Int_t    GetParN     (TString name) const             { return FindPar(name,true) -> GetN     ();    }///< Get number of parameters in array of given name.
        Bool_t   GetParBool  (TString name, int idx=-1) const { return FindPar(name,true) -> GetBool  (idx); } ///< Get parameter in Bool_t
        Int_t    GetParInt   (TString name, int idx=-1) const { return FindPar(name,true) -> GetInt   (idx); } ///< Get parameter in Int_t
        Long64_t GetParLong  (TString name, int idx=-1) const { return FindPar(name,true) -> GetLong  (idx); } ///< Get parameter in Long64_t
        Double_t GetParDouble(TString name, int idx=-1) const { return FindPar(name,true) -> GetDouble(idx); } ///< Get parameter in Double_t
        TString  GetParString(TString name, int idx=-1) const { return FindPar(name,true) -> GetString(idx); } ///< Get parameter in TString
        Int_t    GetParColor (TString name, int idx=-1) const { return FindPar(name,true) -> GetColor (idx); } ///< Get parameter in Color_t
        TVector3 GetParV3    (TString name) const             { return FindPar(name,true) -> GetV3    ();    } ///< Get parameter in TVector
        Double_t GetParX     (TString name) const { return GetParDouble(name,0); }
        Double_t GetParY     (TString name) const { return GetParDouble(name,1); }
        Double_t GetParZ     (TString name) const { return GetParDouble(name,2); }
        Int_t    GetParStyle (TString name, int idx=-1) const { return GetParInt(name,idx); }
        Int_t    GetParWidth (TString name, int idx=-1) const { return GetParInt(name,idx); }
        Double_t GetParSize  (TString name, int idx=-1) const { return GetParDouble(name,idx); }
        axis_t   GetParAxis  (TString name, int idx=-1) const { return FindPar(name,true) -> GetAxis(); }

        /// UpdatePar() will update given value, if parameter with name and idx exist. If not, value will not be changed.
        void UpdatePar(Bool_t   &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParBool  (name,idx); }
        void UpdatePar(Int_t    &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParInt   (name,idx); } ///< See UpdatePar(Bool_t, TString, int)
        void UpdatePar(Long64_t &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParLong  (name,idx); } ///< See UpdatePar(Bool_t, TString, int)
        void UpdatePar(Double_t &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParDouble(name,idx); } ///< See UpdatePar(Bool_t, TString, int)
        void UpdatePar(TString  &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParString(name,idx); } ///< See UpdatePar(Bool_t, TString, int)
        void UpdatePar(axis_t   &value, TString name, int idx=-1) const { if (CheckPar(name)) value = GetParAxis  (name,idx); } ///< See UpdatePar(Bool_t, TString, int)

        void UpdateBinning(TString name, Int_t &n, Double_t &x1, Double_t &x2) const;
        void UpdateV3(TString name, Double_t &x, Double_t &y, Double_t &z) const;

        std::vector<bool>    GetParVBool  (TString name) const { return FindPar(name,true) -> GetVBool  (); }
        std::vector<int>     GetParVInt   (TString name) const { return FindPar(name,true) -> GetVInt   (); }
        std::vector<double>  GetParVDouble(TString name) const { return FindPar(name,true) -> GetVDouble(); }
        std::vector<TString> GetParVString(TString name) const { return FindPar(name,true) -> GetVString(); }
        std::vector<int>     GetParVStyle (TString name) const { return GetParVInt(name); }
        std::vector<int>     GetParVWidth (TString name) const { return GetParVInt(name); }
        std::vector<int>     GetParVColor (TString name) const { return GetParVInt(name); }
        std::vector<double>  GetParVSize  (TString name) const { return GetParVDouble(name); }

        LKParameterContainer* CreateGroupContainer(TString nameGroup);

    protected:
        LKParameter *SetPar    (TString name, TString  raw, TString val, TString comment, int parameterType, bool rewriteParameter=false);
        LKParameter *SetPar    (TString name, TString  val, TString comment="") { return SetPar(name,val,val,comment,0); } ///< Set parameter string
        LKParameter *SetPar    (TString name, Int_t    val, TString comment="") { return SetPar(name,Form("%d",val),Form("%d",val),comment,0); } ///< Set parameter Int_t
        LKParameter *SetPar    (TString name, Double_t val, TString comment="") { return SetPar(name,Form("%f",val),Form("%f",val),comment,0); } ///< Set parameter Double_t
        LKParameter *SetParCont(TString name);
        LKParameter *SetParFile(TString name);
        LKParameter *SetLineComment(TString comment);

        LKParameter *FindPar(TString givenName, bool terminateIfNull=false) const;

    private:
        void ProcessTypeError(TString name, TString val, TString type) const;
        bool CheckFormulaValidity(TString formula, bool isInt=false) const;
        double Eval(TString formula) const;

        Int_t fNumInputFiles = 0;
        Int_t fRank = 0;

        const int kParameterIsStandard = 0;
        const int kParameterIsLineComment = 1;
        const int kParameterIsTemporary = 2;
        const int kParameterIsConditional = 3;
        const int kParameterIsMultiple = 4;
        //const int kRewriteParameter = 5;

    private:
        TString fCurrentGroupName;
        Int_t fPreviousTabSize = 0;
        vector<Int_t> fTabSizeArray;
        vector<TString> fGroupNameArray;


#ifdef LILAK_BUILD_JSONCPP
        Json::Value fJsonValues;
#endif

    ClassDef(LKParameterContainer, 2)
};

#endif
