#include "LKContainer.h"

LKContainer::LKContainer()
{
}

LKContainer::~LKContainer()
{
}

void LKContainer::Clear(Option_t *option)
{
    TObject::Clear(option);
}

void LKContainer::Copy(TObject &obj) const
{
    TObject::Copy(obj);
}

TObject* LKContainer::Clone(const char *newname) const
{
    LKContainer *obj = (LKContainer*) TObject::Clone(newname);
    return obj;
}
