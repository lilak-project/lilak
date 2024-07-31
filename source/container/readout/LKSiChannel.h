#ifndef LKSICHANNEL_HH
#define LKSICHANNEL_HH

#include "GETChannel.h"

/**
 * Raw event data from GET
 */

class LKSiChannel;

class LKSiChannel : public GETChannel
{
    public:
        LKSiChannel();
        virtual ~LKSiChannel() { ; }

        virtual void Clear(Option_t *option="");
        virtual void Copy(TObject &object) const;
        virtual void Print(Option_t *option="") const;

        virtual const char* GetName() const;
        virtual const char* GetTitle() const;

        virtual TObject *Clone(const char *newname="") const;

        virtual TString MakeTitle() { return TString(GetTitle()); }
        virtual TH1D *GetHist(TString name="");

        virtual double GetIntegral(double pedestal=-1., bool inverted=false);

        int GetLocalID() const { return fLocalID; }
        bool GetSide()  const { return fSide; }
        int  GetStrip() const { return fStrip; }
        bool GetDirection() const { return fDirection; }
        bool GetInverted() const { return fInverted; }
        double GetPhi1() const { return fPhi1; }
        double GetPhi2() const { return fPhi2; }
        double GetTheta1() const { return fTheta1; }
        double GetTheta2() const { return fTheta2; }
        int GetDetID() const { return GetPadID(); }
        int GetPairArrayIndex() const { return fPairArrayIndex; }
        LKSiChannel* GetPairChannel() const { return fPairChannel; }

        //void SetSiChannel(bool side, int strip, bool lr, double phi1, double ph2, double theta1, double, theta2);
        void SetLocalID(int id) { fLocalID = id; }
        void SetSide(bool side) { fSide = side; }
        void SetStrip(int strip) { fStrip = strip; }
        void SetDirection(bool direction) { fDirection = direction; }
        void SetInverted(bool inverted) { fInverted = inverted; }
        void SetPhi1(double phi) { fPhi1 = phi; }
        void SetPhi2(double phi) { fPhi2 = phi; }
        void SetTheta1(double theta) { fTheta1 = theta; }
        void SetTheta2(double theta) { fTheta2 = theta; }
        void SetDetID(int value) { SetPadID(value); }
        void SetPairArrayIndex(int pair) { fPairArrayIndex = pair; }
        void SetPairChannel(LKSiChannel* pairChannel) { fPairChannel = pairChannel; }

        bool IsPair(LKSiChannel* channel);

        void SetEnergy1(double energy) { fEnergy  = energy; }
        void SetEnergy2(double energy) { fEnergy2 = energy; }
        double GetEnergy1() const { return fEnergy;  }
        double GetEnergy2() const { return fEnergy2; }
        double GetEnergyPos() const { if (fEnergy>=0&&fEnergy2>=0) { return (fEnergy-fEnergy2)/(fEnergy +fEnergy2); } else { return -99.9; } }
        double GetEnergySum() const { if (fEnergy>=0&&fEnergy2>=0) { return (fEnergy+fEnergy2); } else { return -99.9; } }

        void SetIsStandaloneChannel() { fIsStandaloneChannel = true; }
        void SetIsPairedChannel() { fIsStandaloneChannel = false; }

        bool IsStandaloneChannel() const { return fIsStandaloneChannel; }
        bool IsPairedChannel() const { return !fIsStandaloneChannel; }

    protected:
        int  fLocalID;
        bool fSide;
        int  fStrip;
        bool fDirection; ///< 0 (left,up) or 1 (right,down)
        bool fInverted = false;

        int fPairArrayIndex = -1;
        LKSiChannel *fPairChannel; //!
        double fEnergy2 = -99.9;

        double fPhi1 = 0;
        double fPhi2 = 0;
        double fTheta1 = 0;
        double fTheta2 = 0;

        bool fIsStandaloneChannel = true;

    ClassDef(LKSiChannel,2);
};

#endif
