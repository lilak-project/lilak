#include "TRandom.h"

#include "LKLogger.hpp"
#include "LKRun.hpp"
#include "LKRandomPointCreator.hpp"

ClassImp(LKRandomPointCreator)

LKRandomPointCreator::LKRandomPointCreator()
:LKTask("LKRandomPointCreator","")
{
}

bool LKRandomPointCreator::Init()
{
  LKRun *run = LKRun::GetRun();
  auto par = run -> GetPar();

  fPointArray = new TClonesArray("LKWPoint",100);
  run -> RegisterBranch("PointArray", fPointArray, true);

  fNumPoints = par -> GetParInt("numPoints");
  lk_info << "Number of points: " << fNumPoints << endl;

  return true;
}

void LKRandomPointCreator::Exec(Option_t*)
{
  fPointArray -> Clear("C");

  for (auto iPoint=0; iPoint<fNumPoints; ++iPoint)
  {
      auto point = (LKWPoint *) fPointArray -> ConstructedAt(iPoint);

      auto x = gRandom -> Uniform(-1,1);
      auto y = gRandom -> Uniform(-1,1);
      auto z = gRandom -> Uniform(-1,1);
      auto w = gRandom -> Uniform(1,10);

      point -> Set(x,y,z,w);
  }

  lk_info << "Created 100 points" << endl;

  return;
}
