#include "LKChannel.h"
#include "LKLogger.h"

ClassImp(LKChannel)


LKChannel::LKChannel()
{
    Clear();
}

void LKChannel::Clear(Option_t *option)
{
    LKContainer::Clear(option);

    fChannelID = -1;
    fPadID = -1;
    fTime = -1;
    fEnergy = -1;
    fPedestal = -1;
    fNoiseScale = -1;
}

void LKChannel::Copy(TObject &obj) const
{
    LKContainer::Copy(obj);
    ((LKChannel&)obj).SetChannelID(fChannelID);
    ((LKChannel&)obj).SetPadID(fPadID);
    ((LKChannel&)obj).SetTime(fTime);
    ((LKChannel&)obj).SetEnergy(fEnergy);
    ((LKChannel&)obj).SetPedestal(fPedestal);
    ((LKChannel&)obj).SetNoiseScale(fNoiseScale);
}

TObject* LKChannel::Clone(const char *newname) const
{
    LKChannel *obj = (LKChannel*) LKContainer::Clone(newname);
    obj -> SetChannelID(fChannelID);
    obj -> SetPadID(fPadID);
    obj -> SetTime(fTime);
    obj -> SetEnergy(fEnergy);
    obj -> SetPedestal(fPedestal);
    obj -> SetNoiseScale(fNoiseScale);
    return obj;
}

void LKChannel::Print(Option_t *option) const
{
    if (TString(option).Index("!title")<0)
        e_info << "[LKChannel]" << std::endl;
    e_info << "- Ch/Pad: " << fChannelID << " " << fPadID << std::endl;
    e_info << "- TEPN: " << fTime << " " << fEnergy << " " << fPedestal << " " << fNoiseScale << std::endl;
}
