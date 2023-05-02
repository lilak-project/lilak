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

  fPointArray = run -> GetBranchA("PointArray");

  fGausArray = new TClonesArray("LKWPoint",100);
  run -> RegisterBranch("GausArray", fGausArray, true);

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

      lx_debug << point -> X() << endl;

      auto x = gRandom -> Gaus(point->X(),point->W());
      auto y = gRandom -> Gaus(point->Y(),point->W());
      auto z = gRandom -> Gaus(point->Z(),point->W());

      gaus -> Set(x,y,z);
  }

  lk_info << "Created 100 gaus points" << endl;

  return;
}
