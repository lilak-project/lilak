#ifndef LKPARAMETER_HH
#define LKPARAMETER_HH

#include <vector>
#include "TNamed.h"
#include "TVector3.h"
#include "LKVector3.hpp"

typedef LKVector3::Axis axis_t;

class LKParameter : public TNamed
{
    public:
        LKParameter();
        LKParameter(TString name, TString raw, TString value="", TString comment="");
        virtual ~LKParameter();

        virtual void Clear(Option_t *option = "");

        void SetLineComment(TString comment);
        void SetPar(TString name, TString raw, TString value, TString comment);

        //const char *GetName()
        TString GetComment()  const { return fComment; }
        TString GetRaw()      const { return fRaw; }
        TString GetValue()    const { return fValue; }
        int     GetN()        const { return fNumValues; }

        int      GetInt   (int i=-1) const;  ///< Get parameter in int
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
        void ProcessTypeError(TString type) const;
        bool CheckFormulaValidity(TString formula, bool isInt=false) const;

        TString fRaw; // raw value with no replacements
        TString fValue; // configured value
        TString fComment;
        int fNumValues = 0;
        std::vector<TString> fValueArray;

    ClassDef(LKParameter, 1)
};

#endif
