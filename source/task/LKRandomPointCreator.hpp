#ifndef LKRANDOMPOINTCREATOR_HH
#define LKRANDOMPOINTCREATOR_HH

#include "TClonesArray.h"

#include "LKTask.hpp"
#include "LKWPoint.hpp"

class LKRandomPointCreator : public LKTask
{ 
  public:
    LKRandomPointCreator();
    virtual ~LKRandomPointCreator() {}

    bool Init();
    void Exec(Option_t*);

  private:
    TClonesArray* fPointArray = nullptr;

    double fWidth = 1;
    int fNumPoints = 100;

  ClassDef(LKRandomPointCreator, 0)
};

#endif
