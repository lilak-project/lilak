#include "LKContainer.h"
#include "LKLogger.h"

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

bool LKContainer::SetValue(TString keyword, TString value) {
    e_error << "This class do not feature this method yet!" << std::endl;
    return false;
}
