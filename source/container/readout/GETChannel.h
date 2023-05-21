#ifndef GETCHANNEL_HH
#define GETCHANNEL_HH

#include "LKContainer.h"
#include "TArrayD.h"
#include "TH1D.h"

class GETChannel : public LKContainer
{
    public:
        GETChannel() { Clear(); }
        virtual ~GETChannel() {}

        virtual void Clear(Option_t *option = "");


        TH1D *GetHist(TString name);

        void SetCobo(Int_t cobo)       { fCobo = cobo; }
        void SetAsad(Int_t asad)       { fAsad = asad; }
        void SetAget(Int_t aget)       { fAget = aget; }
        void SetChan(Int_t chan)       { fChan = chan; }
        void SetTime(Float_t time)     { fTime = time; }
        void SetEnergy(Float_t energy) { fEnergy = energy; }

        Int_t GetCobo()     const { return fCobo; }
        Int_t GetAsad()     const { return fAsad; }
        Int_t GetAget()     const { return fAget; }
        Int_t GetChan()     const { return fChan; }
        Float_t GetTime()   const { return fTime; }
        Float_t GetEnergy() const { return fEnergy; }

        void SetWaveform(Int_t *buffer);
        Int_t *GetWaveform() { return fWaveform; };
        const Int_t *GetWaveform() const { return fWaveform; };

    private:
        Int_t fCobo;
        Int_t fAsad;
        Int_t fAget;
        Int_t fChan;
        Float_t fTime;
        Float_t fEnergy;
        Int_t fWaveform[512];

    ClassDef(GETChannel, 1)
};

#endif
