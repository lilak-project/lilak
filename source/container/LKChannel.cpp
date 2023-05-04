#include "LKChannel.hpp"

ClassImp(LKChannel)


LKChannel::LKChannel()
{
    Clear();
}

void LKChannel::Clear(Option_t *option)
{
    LKContainer::Clear(option);

    //TString opt(option); opt.ToLower();
    //if (opt.Index("ch")>=0) fID = -1;
}

void LKChannel::Copy(TObject &obj) const
{
    LKContainer::Copy(obj);
    auto channel = (LKChannel &) obj;
    channel.SetID(fID);
}

void LKChannel::SetID(Int_t id) { fID = id; }
Int_t LKChannel::GetID() const { return fID; }
