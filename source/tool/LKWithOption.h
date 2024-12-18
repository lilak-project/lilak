#ifndef LKWITHOPTION_HH
#define LKWITHOPTION_HH

#include "LKMisc.h"

class LKWithOption
{
    public:
        LKWithOption() {}
        ~LKWithOption() {}

        TString GetGlobalOption() const { return fGlobalOption; }
        void    SetGlobalOption(TString option) { fGlobalOption = option; }
        bool    CheckOption(TString option)                     { return LKMisc::CheckOption     (fGlobalOption,option); }
        int     FindOptionInt   (TString option, int value)     { return LKMisc::FindOptionInt   (fGlobalOption,option,value); }
        double  FindOptionDouble(TString option, double value)  { return LKMisc::FindOptionDouble(fGlobalOption,option,value); }
        TString FindOptionString(TString option, TString value) { return LKMisc::FindOptionString(fGlobalOption,option,value); }

        void RemoveOption(TString option);
        void AddOption(TString option)                { LKMisc::AddOption(fGlobalOption,option); }
        void AddOption(TString option, double value)  { LKMisc::AddOption(fGlobalOption,option,value); }
        void AddOption(TString option, int value)     { LKMisc::AddOption(fGlobalOption,option,value); }
        void AddOption(TString option, TString value) { LKMisc::AddOption(fGlobalOption,option,value); }

    private:
        TString fGlobalOption = "stats_corner:font=132:opt_stat=1110";

    ClassDef(LKWithOption, 1)
};

#endif
