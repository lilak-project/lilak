#include "TRandom.h"

#include "LKLogger.hpp"
#include "LKRun.hpp"
#include "LKRandomGausCreator.hpp"

ClassImp(LKRandomGausCreator)

LKRandomGausCreator::LKRandomGausCreator()
:LKTask("LKRandomGausCreator","")
{
}

bool LKRandomGausCreator::Init()
{
  LKRun *run = LKRun::GetRun();
  auto par = run -> GetPar();

  fPointArray = run -> GetBranchA("PointArray");

  fGausArray = new TClonesArray("LKWPoint",100);
  run -> RegisterBranch("GausArray", fGausArray, true);

  fHist = new TH1D("hist",";x",100,-10,10);

  fSigma = par -> GetParInt("sigma");
  lk_info << "gaus sigma: " << fSigma << endl;

  return true;
}

void LKRandomGausCreator::Exec(Option_t*)
{
  fGausArray -> Clear("C");

  Int_t numPoints = fPointArray -> GetEntries();

  for (auto iPoint=0; iPoint<numPoints; ++iPoint)
  {
      auto point = (LKWPoint*) fPointArray -> At(iPoint);
      auto gaus = (LKWPoint *) fGausArray -> ConstructedAt(iPoint);

      auto x = gRandom -> Gaus(point->X(),fSigma);
      auto y = gRandom -> Gaus(point->Y(),fSigma);
      auto z = gRandom -> Gaus(point->Z(),fSigma);

      fHist -> Fill(x);

      gaus -> Set(x,y,z);
  }

  lk_info << "Created 100 gaus points" << endl;

  return;
}

bool LKRandomGausCreator::EndOfRun()
{
    LKRun::GetRun() -> GetOutputFile() -> cd();
    fHist -> Write();
    return true;
}
