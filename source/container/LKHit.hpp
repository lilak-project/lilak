#ifndef LKHIT_HH
#define LKHIT_HH

#include "LKWPoint.hpp"
#include "LKContainer.hpp"
//#include "LKHitArray.hpp"

#ifdef ACTIVATE_EVE
#include "TEveElement.h"
#endif

#include "TVector3.h"
#include "TMath.h"
#include "TF1.h"

#include <vector>
using namespace std;

class LKTracklet;

class LKHit : public LKWPoint
{
    protected:
        Int_t fHitID = -1;
        Int_t fTrackID = -1;
        Double_t fAlpha = -999; // TODO the polar angle
        Double_t fDX = -999;
        Double_t fDY = -999;
        Double_t fDZ = -999;
        Double_t fSortValue = 0; //! sort earlier if smaller, latter if larger

        //LKHitArray fHitArray; //!

        vector<Int_t> fTrackCandArray;  //!

    public :
        LKHit() { Clear(); }
        LKHit(Double_t x, Double_t y, Double_t z, Double_t q) { Clear(); Set(x,y,z,q); }
        virtual ~LKHit() {}

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "at") const;
        virtual void Copy (TObject &object) const;
        void CopyFrom(LKHit *hit);

        void SetSortValue(Double_t val);
        void SetSortByX(bool sortEarlierIfSmaller);
        void SetSortByY(bool sortEarlierIfSmaller);
        void SetSortByZ(bool sortEarlierIfSmaller);
        void SetSortByCharge(bool sortEarlierIfSmaller);
        void SetSortByDistanceTo(TVector3 point, bool sortEarlierIfCloser);
        virtual Bool_t IsSortable() const;
        virtual Int_t Compare(const TObject *obj) const;

        //virtual void PropagateMC();

        void SetHitID(Int_t id);
        void SetTrackID(Int_t id);
        void SetAlpha(Double_t a);
        void SetDPosition(TVector3 dpos);
        void SetDX(Double_t dx);
        void SetDY(Double_t dy);
        void SetDZ(Double_t dz);
        void SetX(Double_t x);
        void SetY(Double_t y);
        void SetZ(Double_t z);
        void SetCharge(Double_t charge);

        //virtual void AddHit(LKHit *hit);
        //virtual void RemoveHit(LKHit *hit);

        Double_t GetSortValue() const;

        //LKHitArray *GetHitArray() { return &fHitArray; }

        Int_t GetNumHits() const;

        Int_t GetHitID()   const;
        Int_t GetTrackID() const;
        Double_t GetAlpha()   const;
        TVector3 GetDPosition() const;
        Double_t GetDX()      const;
        Double_t GetDY()      const;
        Double_t GetDZ()      const;
        Double_t GetX()       const;
        Double_t GetY()       const;
        Double_t GetZ()       const;
        Double_t GetCharge()  const;

        //TVector3 GetMean()          const;
        //TVector3 GetVariance()      const;
        //TVector3 GetCovariance()    const;
        //TVector3 GetStdDev()        const;
        //TVector3 GetSquaredMean()   const;
        //TVector3 GetCoSquaredMean() const;

        vector<Int_t> *GetTrackCandArray();
        Int_t GetNumTrackCands();
        void AddTrackCand(Int_t id);
        void RemoveTrackCand(Int_t trackID);

#ifdef ACTIVATE_EVE
        virtual bool DrawByDefault();
        virtual bool IsEveSet();
        virtual TEveElement *CreateEveElement();
        virtual void SetEveElement(TEveElement *, Double_t scale=1);
        virtual void AddToEveSet(TEveElement *eveSet, Double_t scale=1);
#endif

        ClassDef(LKHit, 0)
};

class LKHitSortDirection {
    public:
        LKHitSortDirection(TVector3 p):fP(p) {}
        bool operator() (LKHit* h1, LKHit* h2) { return h1 -> GetPosition().Dot(fP) > h2 -> GetPosition().Dot(fP); }
    private:
        TVector3 fP;
};

class LKHitSortThetaFromP {
    public:
        LKHitSortThetaFromP(TVector3 p):fP(p) {}
        bool operator() (LKHit* h1, LKHit* h2) {
            fP1 = h1 -> GetPosition() - fP;
            fP2 = h2 -> GetPosition() - fP;
            return TMath::ATan2(fP1.Z(),fP1.X()) > TMath::ATan2(fP2.Z(),fP2.X());
        }
    private:
        TVector3 fP, fP1, fP2;
};

class LKHitSortTheta {
    public:
        bool operator() (LKHit* h1, LKHit* h2) { 
            Double_t t1 = TMath::ATan2(h1 -> GetPosition().Z(), h1 -> GetPosition().X());
            Double_t t2 = TMath::ATan2(h2 -> GetPosition().Z(), h2 -> GetPosition().X());
            return t1 > t2;
        }
};

class LKHitSortThetaInv {
    public:
        bool operator() (LKHit* h1, LKHit* h2) {
            Double_t t1 = TMath::ATan2(h1 -> GetPosition().Z(), h1 -> GetPosition().X());
            Double_t t2 = TMath::ATan2(h2 -> GetPosition().Z(), h2 -> GetPosition().X());
            return t1 < t2;
        }
};

class LKHitSortR {
    public:
        bool operator() (LKHit* h1, LKHit* h2) { return h1 -> GetPosition().Mag() > h2 -> GetPosition().Mag(); }
};

class LKHitSortRInv {
    public:
        bool operator() (LKHit* h1, LKHit* h2) { return h1 -> GetPosition().Mag() < h2 -> GetPosition().Mag(); }
};

class LKHitSortRInvFromP {
    public:
        LKHitSortRInvFromP(TVector3 p):fP(p) {}
        bool operator() (LKHit* h1, LKHit* h2){
            fP1 = h1 -> GetPosition() - fP;
            fP2 = h2 -> GetPosition() - fP;
            if (fP1.Mag() == fP2.Mag()) {
                if (fP1.X() == fP2.X()) {
                    if (fP1.Y() == fP2.Y()) {
                        return fP1.Z() > fP2.Z();
                    } return fP1.Y() > fP2.Z();
                } return fP1.X() > fP2.X();
            } return fP1.Mag() < fP2.Mag(); 
        }

    private:
        TVector3 fP;
        TVector3 fP1;
        TVector3 fP2;
};

class LKHitSortZ {
    public:
        bool operator() (LKHit* h1, LKHit* h2) { return h1 -> GetPosition().Z() > h2 -> GetPosition().Z(); }
};

class LKHitSortZInv {
    public:
        bool operator() (LKHit* h1, LKHit* h2) { return h1 -> GetPosition().Z() < h2 -> GetPosition().Z(); }
};

class LKHitSortX {
    public:
        bool operator() (LKHit* h1, LKHit* h2) { return h1 -> GetPosition().X() > h2 -> GetPosition().X(); }
};

class LKHitSortXInv {
    public:
        bool operator() (LKHit* h1, LKHit* h2) { return h1 -> GetPosition().X() < h2 -> GetPosition().X(); }
};

class LKHitSortY {
    public:
        bool operator() (LKHit* h1, LKHit* h2) { return h1 -> GetPosition().Y() > h2 -> GetPosition().Y(); }
};

class LKHitSortYInv {
    public:
        bool operator() (LKHit* h1, LKHit* h2) { return h1 -> GetPosition().Y() < h2 -> GetPosition().Y(); }
};

class LKHitSortCharge {
    public:
        bool operator() (LKHit* h1, LKHit* h2) {
            if (h1 -> GetCharge() == h2 -> GetCharge())
                return h1 -> GetY() > h2 -> GetY();
            return h1 -> GetCharge() > h2 -> GetCharge();
        }
};

class LKHitSortChargeInv {
    public:
        bool operator() (LKHit* h1, LKHit* h2) { return h1 -> GetCharge() < h2 -> GetCharge(); }
};

class LKHitSortXYZInv {
    public:
        bool operator() (LKHit* h1, LKHit* h2) {
            if (h1 -> GetPosition().Z() == h2 -> GetPosition().Z()) {
                if (h1 -> GetPosition().X() == h2 -> GetPosition().X())
                    return h1 -> GetPosition().Y() < h2 -> GetPosition().Y(); 
                else 
                    return h1 -> GetPosition().X() < h2 -> GetPosition().X(); 
            }
            return h1 -> GetPosition().Z() < h2 -> GetPosition().Z(); 
        }
};

class LKHitSortRho {
    public:
        bool operator() (LKHit* h1, LKHit* h2) {
            Double_t rho1 = (h1 -> GetPosition().X() * h1 -> GetPosition().X() + h1 -> GetPosition().Z() * h1 -> GetPosition().Z());
            Double_t rho2 = (h2 -> GetPosition().X() * h2 -> GetPosition().X() + h2 -> GetPosition().Z() * h2 -> GetPosition().Z());
            return rho1 > rho2;
        }
};

class LKHitSortRhoInv {
    public:
        bool operator() (LKHit* h1, LKHit* h2) {
            Double_t rho1 = (h1 -> GetPosition().X() * h1 -> GetPosition().X() + h1 -> GetPosition().Z() * h1 -> GetPosition().Z());
            Double_t rho2 = (h2 -> GetPosition().X() * h2 -> GetPosition().X() + h2 -> GetPosition().Z() * h2 -> GetPosition().Z());
            return rho1 < rho2;
        }
};

class LKHitSortByDistanceTo {
    public:
        LKHitSortByDistanceTo(TVector3 p):fP(p) {}
        bool operator() (LKHit* a, LKHit* b) { return (a->GetPosition()-fP).Mag() < (b->GetPosition()-fP).Mag(); }
    private:
        TVector3 fP;
};

#endif
