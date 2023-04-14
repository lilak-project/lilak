#ifndef LKCONTAINER_HH
#define LKCONTAINER_HH

#include "TObject.h"

class LKContainer : public TObject
{
    public:
        LKContainer();
        virtual ~LKContainer();

        virtual void Clear(Option_t *option = "");
        virtual void Copy (TObject &object) const;

    ClassDef(LKContainer, 0)
};

#endif
