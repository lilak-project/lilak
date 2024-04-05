#include "LKLogger.h"
#include "LKEventHeader.h"

ClassImp(LKEventHeader);

LKEventHeader::LKEventHeader()
{
    Clear();
}

void LKEventHeader::Clear(Option_t *option)
{
    LKContainer::Clear(option);
    fIsGoodEvent = false;
    fEventNumber = -1;
    fBufferStart = -1;
    fBufferSize = -1;
}

void LKEventHeader::Print(Option_t *option) const
{
    e_info << "[EventHeader] " << fEventNumber << " " << (fIsGoodEvent?"(good)":"(bad)") << std::endl;
}

void LKEventHeader::Copy(TObject &object) const
{
    LKContainer::Copy(object);
    auto objCopy = (LKEventHeader &) object;
    objCopy.SetIsGoodEvent(fIsGoodEvent);
    objCopy.SetEventNumber(fEventNumber);
    objCopy.SetBufferStart(fBufferStart);
    objCopy.SetBufferSize(fBufferSize);
}
