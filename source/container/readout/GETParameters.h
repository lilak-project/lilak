#ifndef GETPARAMETERS_HH
#define GETPARAMETERS_HH

#include "TString.h"

class GETParameters
{
    public:
        GETParameters();
        virtual ~GETParameters();
        virtual void ClearParameters();
        virtual void PrintParameters(TString option="") const;

        int GetFrameNo() const  { return fFrameNo; }
        int GetDecayNo() const  { return fDecayNo; }
        int GetDetType() const  { return fDetType; }
        int GetCobo() const  { return fCobo; }
        int GetAsad() const  { return fAsad; }
        int GetAget() const  { return fAget; }
        int GetChan() const  { return fChan; }

        int GetCAAC() const  { return fCobo*10000+fAsad*1000+fAget*100+fChan; }

        void SetFrameNo(int id) { fFrameNo = id; }
        void SetDecayNo(int id) { fDecayNo = id; }
        void SetDetType(int id) { fDetType = id; }
        void SetCobo(int cobo) { fCobo = cobo; }
        void SetAsad(int asad) { fAsad = asad; }
        void SetAget(int aget) { fAget = aget; }
        void SetChan(int chan) { fChan = chan; }

    protected:
        int  fFrameNo = -1;
        int  fDecayNo = -1;
        int  fDetType = -1;
        int  fCobo = -1;
        int  fAsad = -1;
        int  fAget = -1;
        int  fChan = -1;
};

#endif
