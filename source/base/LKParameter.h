#ifndef LKPARAMETER_HH
#define LKPARAMETER_HH

#include <vector>
#include "TNamed.h"
#include "TVector3.h"
#include "LKVector3.h"
#include "LKLogger.h"

typedef LKVector3::Axis axis_t;

class LKParameter : public TNamed
{
    public:
        LKParameter();
        //LKParameter(TString name, TString raw, TString value="", TString comment="", bool temporary=false, bool conditional=false);
        LKParameter(TString name, TString raw, TString value="", TString comment="", int parameterType=0);
        virtual ~LKParameter();

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "") const;

        virtual Bool_t IsSortable() const { return true; }
        virtual Int_t Compare(const TObject *obj) const;

        void SetLineComment(TString comment);
        void SetPar(TString name, TString raw, TString value, TString comment, int parameterType);
        void SetValue(TString value);

        //const char *GetName()
        TString GetGroup()      const { return fGroup; }
        TString GetMainName()   const { return fMainName; }
        TString GetComment()    const { return fComment; }
        TString GetRaw()        const { return fTitle; }
        TString GetValue()      const { return fValue; }
        int     GetN()          const { return fNumValues; }
        int     GetType()       const { return fType; }

        TString GetGroup(int ith) const;
        TString GetLine(TString option="tc") const;

        bool    IsStandard()    const { return (fType==0); }
        bool    IsLineComment() const { return (fType==1); }
        bool    IsTemporary()   const { return (fType==2||fType==6); }
        bool    IsConditional() const { return (fType==3); }
        bool    IsMultiple()    const { return (fType==4||fType==6); }

        bool    CheckTypeInt   (int i=-1) const;
        bool    CheckTypeLong  (int i=-1) const;
        bool    CheckTypeBool  (int i=-1) const;
        bool    CheckTypeDouble(int i=-1) const;
        bool    CheckTypeString(int i=-1) const;
        bool    CheckTypeColor (int i=-1) const;
        bool    CheckTypeAxis  (int i=-1) const;
        bool    CheckTypeV3    ()         const;

        int      GetInt   (int i=-1) const;  ///< Get parameter in int
        Long64_t GetLong  (int i=-1) const;  ///< Get parameter in int
        bool     GetBool  (int i=-1) const;  ///< Get parameter in bool
        double   GetDouble(int i=-1) const;  ///< Get parameter in double
        TString  GetString(int i=-1) const;  ///< Get parameter in TString
        int      GetColor (int i=-1) const;  ///< Get parameter in Color_t
        axis_t   GetAxis  (int i=-1) const;  ///< Get parameter in LKVector3::Axis
        TVector3 GetV3    ()         const;  ///< Get parameter in TVector3

        int      GetWidth(int i=-1)  const { return GetInt(i); }
        double   GetSize (int i=-1)  const { return GetDouble(i); }
        double   GetX    ()          const { return GetDouble(0); }
        double   GetY    ()          const { return GetDouble(1); }
        double   GetZ    ()          const { return GetDouble(2); }

        std::vector<bool>    GetVBool  () const;
        std::vector<int>     GetVInt   () const;
        std::vector<double>  GetVDouble() const;
        std::vector<TString> GetVString() const;

        std::vector<int>     GetVWidth () const { return GetVInt(); }
        std::vector<int>     GetVColor () const { return GetVInt(); }
        std::vector<double>  GetVSize  () const { return GetVDouble(); }

    private:
        void ProcessTypeError(TString type, TString value) const;
        bool CheckFormulaValidity(TString formula, bool isInt=false) const;

        //TString fName; ///< parameter name defined from TNamed
        TString fGroup; ///< group of parameter
        TString fMainName; ///< group of parameter
        //TString fTitle; ///< raw value with no replacements
        TString fValue; ///< configured value
        TString fComment;
        int fNumValues = 0;
        std::vector<TString> fValueArray;
        /**
         * 0 : standard
         * 1 : comment
         * 2 : temporary
         * 3 : conditional
         * 4 : multiple
         */
        int fType = 0;

    ClassDef(LKParameter, 2)
};

#endif
