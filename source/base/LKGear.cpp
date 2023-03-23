#include "LKGear.hpp"

void LKGear::CreateParameterContainer(bool debug) {
  if (fPar == nullptr)
    fPar = new LKParameterContainer(debug);
}

void LKGear::AddParameterContainer(LKParameterContainer *par) {
  if (fPar == nullptr)
    fPar = par;
  else
    fPar -> AddParameterContainer(par);
}

void LKGear::AddParameterContainer(TString fname) {
  if (fPar == nullptr)
    fPar = new LKParameterContainer();
  fPar -> AddFile(fname);
}

LKParameterContainer *LKGear::GetParameterContainer() const { return fPar; }
LKParameterContainer *LKGear::GetPar() const { return fPar; }

void LKGear::SetRank(Int_t rank) { fRank = rank; }
Int_t LKGear::GetRank() const { return fRank; }
