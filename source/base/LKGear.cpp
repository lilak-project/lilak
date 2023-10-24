#include "LKGear.h"

ClassImp(LKGear)

void LKGear::CreateParameterContainer() {
    if (fPar == nullptr)
        fPar = new LKParameterContainer();
}

void LKGear::SetParameterContainer(LKParameterContainer *par) {
    fPar = par;
}

void LKGear::AddParameterContainer(LKParameterContainer *par) {
    if (fPar == nullptr)
        fPar = par;
    else
        fPar -> AddParameterContainer(par);
    fPar -> Recompile();
}

void LKGear::AddParameterContainer(TString fname) {
    if (fPar == nullptr)
        fPar = new LKParameterContainer();
    if (fname.IsNull())
        fPar -> SearchAndAddPar();
    else
        fPar -> AddFile(fname);
    fPar -> Recompile();
}

LKParameterContainer *LKGear::GetParameterContainer() const { return fPar; }
LKParameterContainer *LKGear::GetPar() const { return fPar; }

void LKGear::SetRank(Int_t rank) { fRank = rank; }
Int_t LKGear::GetRank() const { return fRank; }
