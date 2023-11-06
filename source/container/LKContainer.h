#ifndef LKCONTAINER_HH
#define LKCONTAINER_HH

#include "TObject.h"
#ifdef ACTIVATE_EVE
#include "TEveElement.h"
#endif

class LKContainer : public TObject
{
    public:
        LKContainer();
        virtual ~LKContainer();

        virtual void Clear(Option_t *option = "");
        virtual void Copy (TObject &object) const;

        virtual bool DrawByDefault()              { return false; } ///< return true if to be displayed on eve.
#ifdef ACTIVATE_EVE
        virtual bool IsEveSet()                   { return false; } ///< Check if this element should be a "set" of TEveElements (e.g. TEvePointSet)
        virtual TEveElement *CreateEveElement()   { return nullptr; } ///< Create TEveElement
        virtual void SetEveElement(TEveElement *, Double_t) {} ///< Set TEveElement. For when IsEveSet() is false.
        virtual void   AddToEveSet(TEveElement *, Double_t) {} ///< Add TEveElement to this eve-set
#endif

    ClassDef(LKContainer, 1)
};

#endif
