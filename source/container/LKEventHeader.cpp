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
    ((LKEventHeader&)object).SetIsGoodEvent(fIsGoodEvent);
    ((LKEventHeader&)object).SetEventNumber(fEventNumber);
    ((LKEventHeader&)object).SetBufferStart(fBufferStart);
    ((LKEventHeader&)object).SetBufferSize(fBufferSize);
}
