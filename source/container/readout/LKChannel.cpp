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
    auto channel = (LKChannel &) obj;
    channel.SetChannelID(fChannelID);
    channel.SetPadID(fPadID);
    channel.SetTime(fTime);
    channel.SetEnergy(fEnergy);
    channel.SetPedestal(fPedestal);
    channel.SetNoiseScale(fNoiseScale);
}

void LKChannel::Print(Option_t *option) const
{
    if (TString(option).Index("!title")<0)
        e_info << "[LKChannel]" << std::endl;
    e_info << "- Ch/Pad: " << fChannelID << " " << fPadID << std::endl;
    e_info << "- TEPN: " << fTime << " " << fEnergy << " " << fPedestal << " " << fNoiseScale << std::endl;
}
