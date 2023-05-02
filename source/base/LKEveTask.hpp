#ifndef LKEVETASK_HH
#define LKEVETASK_HH

#include "TEveEventManager.h"
#include "TEveEventManager.h"
#include "LKTask.hpp"

class LKEveTask : public LKTask
{ 
  public:
    LKEveTask();
    virtual ~LKEveTask() {}

    bool Init();
    void Exec(Option_t*);

    void RunEve(Long64_t eveEventID, TString option)
    void AddEveElementToEvent(KBContainer *eveObj, bool permanent = true);
    void AddEveElementToEvent(TEveElement *element, bool permanent = true);
    void ConfigureEventDisplay();

  private:
    TObjArray *fEveEventManagerArray = nullptr;
    TEveEventManager *fEveEventManager = nullptr;
    std::vector<TEveElement *> fEveElementList;
    std::vector<TEveElement *> fPermanentEveElementList;

    Double_t fEveScale = 1;

  ClassDef(LKEveTask, 0)
};

#endif
