#ifndef LKGEAR_HH
#define LKGEAR_HH

#include <iostream>
#include <fstream>

#include "LKParameterContainer.h"

/**
 * LKGear is parameter container holder
 */

class LKVirtualRun;
class LKGear
{
    public:
        LKGear() {};
        virtual ~LKGear() {};

        void CreateParameterContainer();

        virtual void SetParameterContainer(LKParameterContainer *par);
        virtual void AddParameterContainer(LKParameterContainer *par, bool addEvalOnly=false);
        virtual void AddParameterContainer(TString fname);

        void SetPar(LKParameterContainer *par) { SetParameterContainer(par); }
        void AddPar(LKParameterContainer *par, bool addEvalOnly=false) { AddParameterContainer(par, addEvalOnly); }
        void AddPar(TString fname) { AddParameterContainer(fname); }

        LKParameterContainer *GetParameterContainer() const;
        LKParameterContainer *GetPar() const;

        virtual void SetRank(Int_t rank);
        Int_t GetRank() const;

        void SetRun(LKVirtualRun *run) { fRun = run; }

        void SetAllowControlLogger(bool val) { fAllowControlLogger = val; }

    protected:
        LKParameterContainer *fPar = nullptr;
        LKVirtualRun *fRun = nullptr;
        Int_t fRank = 0;
        bool fAllowControlLogger = true;

    ClassDef(LKGear, 1)
};

#endif
