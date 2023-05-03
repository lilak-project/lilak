#ifndef LKRANDOMGAUSCREATOR_HH
#define LKRANDOMGAUSCREATOR_HH

#include "TClonesArray.h"
#include "TH1D.h"

#include "LKTask.hpp"
#include "LKWPoint.hpp"

class LKRandomGausCreator : public LKTask
{ 
  public:
    LKRandomGausCreator();
    virtual ~LKRandomGausCreator() {}

    bool Init();
    void Exec(Option_t*);
    bool EndOfRun();

  private:
    TClonesArray* fPointArray = nullptr;

    TClonesArray* fGausArray = nullptr;
    TH1D *fHist = nullptr;

    double fSigma = 10;

  ClassDef(LKRandomGausCreator, 0)
};

#endif
