#ifndef LKCHANNEL_HH
#define LKCHANNEL_HH

#include "LKContainer.h"

#include "TObject.h"
#include "TObjArray.h"
#include "TVector3.h"

/**
 * General channel
 * Note that PadID is shared with DetID
 * Use PadID for TPC, and DetID for Other detectors
 */
class LKChannel : public LKContainer
{
    public:
        LKChannel();
        virtual ~LKChannel() {}

        virtual void Clear(Option_t *option = "");
        virtual void Copy(TObject &obj) const;
        virtual void Print(Option_t *option = "") const;

        virtual TObject *Clone(const char *newname="") const;

        void SetChannelID(int value) { fChannelID = value; }
        void SetPadID(int value) { fPadID = value; }
        void SetDetID(int value) { fPadID = value; }
        void SetTime(double value) { fTime = value; }
        void SetEnergy(double value) { fEnergy = value; }
        void SetPedestal(double value) { fPedestal = value; }
        void SetNoiseScale(double value) { fNoiseScale = value; }

        int GetChannelID() const { return fChannelID; }
        int GetDetID() const { return fPadID; }
        int GetPadID() const { return fPadID; }
        double GetTime() const { return fTime; }
        double GetEnergy() const { return fEnergy; }
        double GetPedestal() const { return fPedestal; }
        double GetNoiseScale() const { return fNoiseScale; }

    protected:
        int fChannelID = -1;
        int fPadID = -1;
        double fTime = -1;
        double fEnergy = -1;
        double fPedestal = -1;
        double fNoiseScale = -1;

    ClassDef(LKChannel, 2)
};

#endif
