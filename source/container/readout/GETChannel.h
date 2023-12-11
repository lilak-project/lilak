#ifndef GETCHANNEL_HH
#define GETCHANNEL_HH

#include <vector>

#include "TClonesArray.h"
#include "TH1D.h"
#include "LKChannel.h"
#include "LKContainer.h"
#include "LKLogger.h"

/*
 * Remove this comment block after reading it through
 * Or use print_example_comments=False option to omit printing
 *
 * # Example LILAK container class
 *
 * ## Must
 * - Write Clear() method
 * : Clear() method clears and intialize the data class.
 * This is "Must Write Method" because containers are not recreated each event,
 * but their memories are reused after Clear method is called.
 * and they are filled up and written to tree each event.
 * See: https://root.cern/doc/master/classTClonesArray.html, https://opentutorials.org/module/2860/19477
 *
 * - Version number (2nd par. in ClassDef of source file) should be changed if the class has been modified.
 * This notifiy users that the container has been update in the new LILAK (or side project version).
 *
 * ## Recommended
 * - Documentaion like this!
 * - Write Print() to see what is inside the container;
 *
 * ## If you have time
 * - Write Copy() for copying object
 */

/**
 * Raw event data from GET
 */
class GETChannel : public LKChannel
{
    public:
        GETChannel();
        virtual ~GETChannel() { ; }

        void Clear(Option_t *option="");
        void Print(Option_t *option="") const;
        void Copy(TObject &object) const;
        GETChannel* CloneChannel() const;

        virtual void Draw(Option_t *option="");

        Int_t GetDetType() const  { return fDetType; }
        Int_t GetFrameNo() const  { return fFrameNo; }
        Int_t GetDecayNo() const  { return fDecayNo; }
        Int_t GetCobo() const  { return fCobo; }
        Int_t GetAsad() const  { return fAsad; }
        Int_t GetAget() const  { return fAget; }
        Int_t GetChan() const  { return fChan; }
        Int_t GetChan2() const  { return fChan2; }
        Float_t GetTime() const  { return fTime; }
        Float_t GetEnergy() const  { return fEnergy; }
        Int_t* GetWaveformX() { return fWaveformX; }
        Int_t* GetWaveformY() { return fWaveformY; }

        Int_t GetCAAC() const  { return fCobo*10000+fAsad*1000+fAget*100+fChan; }

        void SetDetType(Int_t eventIdx) { fDetType = eventIdx; }
        void SetFrameNo(Int_t frameNo) { fFrameNo = frameNo; }
        void SetDecayNo(Int_t decayNo) { fDecayNo = decayNo; }
        void SetCobo(Int_t cobo) { fCobo = cobo; }
        void SetAsad(Int_t asad) { fAsad = asad; }
        void SetAget(Int_t aget) { fAget = aget; }
        void SetChan(Int_t chan) { fChan = chan; }
        void SetChan2(Int_t dChan) { fChan2 = dChan; }
        void SetTime(Float_t time) { fTime = time; }
        void SetEnergy(Float_t energy) { fEnergy = energy; }
        void SetWaveformX(const Int_t *waveform) { memcpy(fWaveformX, waveform, sizeof(Int_t)*512); }
        void SetWaveformY(const Int_t *waveform) { memcpy(fWaveformY, waveform, sizeof(Int_t)*512); }
        void SetWaveformY(const UInt_t *waveform);
        void SetWaveformY(std::vector<unsigned int> waveform);

        Int_t        fDetType = -1;
        Int_t        fFrameNo = -1;
        Int_t        fDecayNo = -1;
        Int_t        fCobo = -1;
        Int_t        fAsad = -1;
        Int_t        fAget = -1;
        Int_t        fChan = -1;
        Int_t        fChan2 = -1;
        Float_t      fTime = -1;
        Float_t      fEnergy = -1;
        Int_t        fWaveformX[512];
        Int_t        fWaveformY[512];

        Int_t GetBase() const  { return fBase; }
        void  SetBase(Int_t Base) { fBase = Base; }
        Int_t fBase = -1;

    private:
        TH1D* fHist = nullptr; //!

    ClassDef(GETChannel,2);
};

#endif
