#ifndef LKGEAR_HH
#define LKGEAR_HH

#include <iostream>
#include <fstream>
#include "LKParameterContainer.hpp"

/**
 * LKGear is basically parameter container holder
 */

class LKGear
{
    public:
        LKGear() {};
        virtual ~LKGear() {};

        void CreateParameterContainer(bool debug=false);

        virtual void SetParameterContainer(LKParameterContainer *par);
        virtual void AddParameterContainer(LKParameterContainer *par);
        virtual void AddParameterContainer(TString fname);

        void SetPar(LKParameterContainer *par) { SetParameterContainer(par); }
        void AddPar(LKParameterContainer *par) { AddParameterContainer(par); }
        void AddPar(TString fname) { AddParameterContainer(fname); }

        LKParameterContainer *GetParameterContainer() const;
        LKParameterContainer *GetPar() const;

        virtual void SetRank(Int_t rank);
        Int_t GetRank() const;

    protected:
        LKParameterContainer *fPar = nullptr;

        Int_t fRank = 0;

    ClassDef(LKGear, 1)
};

#endif
