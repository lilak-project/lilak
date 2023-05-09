#ifndef LKPARAMETERCONTAINER
#define LKPARAMETERCONTAINER

#include <fstream>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <vector>

#include "json.h"

#include "TObjArray.h"
#include "TNamed.h"
#include "TParameter.h"
#include "TFormula.h"
#include "TVector3.h"

#include "LKVector3.hpp"
#include "LKLogger.hpp"
#include "LKParameter.hpp"

typedef LKVector3::Axis axis_t;

using namespace std;

/**
 * # LKParameterContainer is a list of parameters([par-name] [par-value])
 *
 * ## Set parameter from the file
 *
 * Comments should start with "#". Lines starting with "#" are ignored.
 * [par-name] should not contain empty spaces.
 * For adding file use "<<" for [par-name]
 * [par-value] comes after [par-name], before "#".
 * [par-value] can be a single value or a list of values.
 * If [par-value] is a list of values, the values should be separated by empty spaces.
 * [par-value] can reference other parameters using {}: {[par-name]}, {[par-name][1]}.
 * [par-value] can reference environment variable using e{}: e{ROOTSYS}
 *
 * ## exmpale parameter file
 *
 * <<         /file/to/add.par
 * size       100                       # this is comment
 * dimension  50 60 70
 * title      this is title 5
 * length     {dimension[2]}+10
 * color      kRed+1
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
 *
 * ## Keywords used internally
 *
 * - COMMENT_PAR_: COMMENT_PAR_[par-name] is used to save comment of [par-name]
 * - NUM_VALUES_ : NUM_VALUES_[par-name] is saved to keep the number of values for parameter
 * - INPUT_FILE_ : INPUT_FILE_[i] is saved to keep the input file name
 * - INPUT_PARC_ : INPUT_PARC_[i] is saved to keep the existance of input parameter container
 * - [i]         : [par-name][i] is saved to keep the individual components of parameters
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
        virtual Int_t AddFile(TString fileName, bool addFilePar=false); ///< Add parameter file
        virtual Int_t AddParameterContainer(LKParameterContainer *parc); ///< Add parameter container pointer
        Int_t GetNumInputFiles() const { return fNumInputFiles; } ///< Get number of input parameter files
        bool SearchAndAddPar(TString dirName="");

        Int_t  AddJsonTree(const Json::Value &value, TString treeName="");

        Bool_t AddLine(std::string line); ///< Set parameter by line
        Bool_t AddPar(TString name, TString val, TString comment=""); ///< Set parameter TString
        Bool_t AddPar(TString name, Int_t val, TString comment="")    { return AddPar(name,Form("%d",val),comment); } ///< Set parameter Int_t
        Bool_t AddPar(TString name, Double_t val, TString comment="") { return AddPar(name,Form("%f",val),comment); } ///< Set parameter Double_t

        LKParameter *SetPar      (TString name, TString  raw, TString val, TString comment);
        LKParameter *SetPar      (TString name, TString  val, TString comment="");
        LKParameter *SetPar      (TString name, Int_t    val, TString comment="") { return SetPar(name,Form("%d",val),comment); } ///< Set parameter Int_t
        LKParameter *SetPar      (TString name, Double_t val, TString comment="") { return SetPar(name,Form("%f",val),comment); } ///< Set parameter Double_t
        LKParameter *SetParCont  (TString name);
        LKParameter *SetParFile  (TString name);
        LKParameter *SetLineComment(TString comment);

        LKParameter *FindPar(TString givenName, bool terminateIfNull=false) const;

        Bool_t CheckPar(TString name) const;
        Bool_t CheckValue(TString name) const;

        Int_t    GetParN     (TString name) const;              ///< Get number of parameters in array of given name.
        Bool_t   GetParBool  (TString name, int idx=-1) const;  ///< Get parameter in Bool_t
        Int_t    GetParInt   (TString name, int idx=-1) const;  ///< Get parameter in Int_t
        Double_t GetParDouble(TString name, int idx=-1) const;  ///< Get parameter in Double_t
        TString  GetParString(TString name, int idx=-1) const;  ///< Get parameter in TString
        Int_t    GetParColor (TString name, int idx=-1) const;  ///< Get parameter in Color_t
        TVector3 GetParV3    (TString name) const;              ///< Get parameter in TVector
        Double_t GetParX     (TString name) const { return GetParDouble(name,0); }
        Double_t GetParY     (TString name) const { return GetParDouble(name,1); }
        Double_t GetParZ     (TString name) const { return GetParDouble(name,2); }
        Int_t    GetParStyle (TString name, int idx=-1) const { return GetParInt(name,idx); }
        Int_t    GetParWidth (TString name, int idx=-1) const { return GetParInt(name,idx); }
        Double_t GetParSize  (TString name, int idx=-1) const { return GetParDouble(name,idx); }
        axis_t   GetParAxis  (TString name, int idx=-1) const;

        std::vector<bool>    GetParVBool  (TString name) const;
        std::vector<int>     GetParVInt   (TString name) const;
        std::vector<double>  GetParVDouble(TString name) const;
        std::vector<TString> GetParVString(TString name) const;
        std::vector<int>     GetParVStyle (TString name) const { return GetParVInt(name); }
        std::vector<int>     GetParVWidth (TString name) const { return GetParVInt(name); }
        std::vector<int>     GetParVColor (TString name) const { return GetParVInt(name); }
        std::vector<double>  GetParVSize  (TString name) const { return GetParVDouble(name); }

    private:
        void ProcessTypeError(TString name, TString val, TString type) const;
        bool CheckFormulaValidity(TString formula, bool isInt=false) const;
        double Eval(TString formula) const;

        Int_t fNumInputFiles = 0;
        Int_t fRank = 0;

        Json::Value fJsonValues;

    ClassDef(LKParameterContainer, 1)
};

#endif
