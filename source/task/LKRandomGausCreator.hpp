#ifndef LKRANDOMGAUSCREATOR_HH
#define LKRANDOMGAUSCREATOR_HH

#include "TClonesArray.h"

#include "LKTask.hpp"
#include "LKWPoint.hpp"

class LKRandomGausCreator : public LKTask
{ 
  public:
    LKRandomGausCreator();
    virtual ~LKRandomGausCreator() {}

    bool Init();
    void Exec(Option_t*);

  private:
    TClonesArray* fPointArray = nullptr;
    TClonesArray* fGausArray = nullptr;

  ClassDef(LKRandomGausCreator, 0)
};

#endif
