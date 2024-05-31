#ifndef LKHIT_HH
#define LKHIT_HH

#include "LKContainer.h"
#include "LKContainer.h"
#include "LKHitArray.h"

#ifdef ACTIVATE_EVE
#include "TEveElement.h"
#endif

#include "TVector3.h"
#include "TMath.h"
#include "TF1.h"

#include <vector>
using namespace std;

class LKTracklet;

class LKHit : public LKContainer
{
    protected:
        Double_t fX  = -999;
        Double_t fY  = -999;
        Double_t fZ  = -999;
        Double_t fW  = -999;
        Double_t fDX = -999;
        Double_t fDY = -999;
        Double_t fDZ = -999;
        Double_t fTb = -999;
        Int_t fHitID = -999;
        Int_t fTrackID = -999;
        Int_t fChannelID = -999;
        Int_t fPadID = -999;
        Double_t fAlpha = -999; ///< Use for polar angle
        Double_t fPedestal = -999;
        Int_t fSection = -999;
        Int_t fLayer = -999;
        Int_t fRow = -999;

        Double_t fSortValue = 0; //! Temporary parameter for sorting. Sort earlier if smaller, latter if larger
        LKHitArray fHitArray; //! Temporary hit array
        vector<Int_t> fTrackCandArray;  //! Temporary array for track ID candidates

    public :
        LKHit() { Clear(); }
        LKHit(Double_t x, Double_t y, Double_t z, Double_t q) { Clear(); SetPosition(x,y,z); SetW(q); }
        virtual ~LKHit() {}

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "at") const;
        virtual void Copy (TObject &object) const;
        void CopyFrom(LKHit *hit);

        void SetSortValue(Double_t val) { fSortValue = val; }
        void SetSortByX(bool sortEarlierIfSmaller);
        void SetSortByY(bool sortEarlierIfSmaller);
        void SetSortByZ(bool sortEarlierIfSmaller);
        void SetSortByCharge(bool sortEarlierIfSmaller);
        void SetSortByDistanceTo(TVector3 point, bool sortEarlierIfCloser);
        void SetSortByLayer(bool sortEarlierIfSmaller);
        virtual Bool_t IsSortable() const;
        virtual Int_t Compare(const TObject *obj) const;

        //virtual void PropagateMC();

        void SetX(Double_t x) { fX = x; }
        void SetY(Double_t y) { fY = y; }
        void SetZ(Double_t z) { fZ = z; }
        void SetW(Double_t w) { fW = w; } ///< Same as SetCharge
        void SetCharge(Double_t charge) { fW = charge; }
        void SetWeight(Double_t w) { fW = w; }
        void SetPosition(Double_t x, Double_t y, Double_t z) { fX = x; fY = y; fZ = z; }
        void SetPosition(TVector3 pos) { fX = pos.X(); fY = pos.Y(); fZ = pos.Z(); }
        void SetHitID(Int_t id) { fHitID = id; }
        void SetTrackID(Int_t id) { fTrackID = id; }
        void SetChannelID(Int_t id) { fChannelID = id; }
        void SetPadID(Int_t id) { fPadID = id; }
        void SetPedestal(Double_t a) { fPedestal = a; }
        void SetAlpha(Double_t a) { fAlpha = a; }
        void SetPositionError(TVector3 dpos) { fDX = dpos.X(); fDY = dpos.Y(); fDZ = dpos.Z(); }
        void SetPositionError(Double_t dx, Double_t dy, Double_t dz) { fDX = dx; fDY = dy; fDZ = dz; }
        void SetPosError(TVector3 dpos) { SetPositionError(dpos); }
        void SetPosError(Double_t dx, Double_t dy, Double_t dz) { SetPositionError(dx, dy, dz); }
        void SetXError(Double_t dx) { fDX = dx; }
        void SetYError(Double_t dy) { fDY = dy; }
        void SetZError(Double_t dz) { fDZ = dz; }
        void SetTb(Double_t tb) { fTb = tb; }
        void SetSection(Int_t section) { fSection = section; }
        void SetLayer(Int_t layer) { fLayer = layer; }
        void SetRow(Int_t row) { fRow = row; }

        virtual void AddHit(LKHit *hit);
        virtual void RemoveHit(LKHit *hit);

        LKHitArray *GetHitArray() { return &fHitArray; }

        Double_t x() const { return fX; }
        Double_t y() const { return fY; }
        Double_t z() const { return fZ; }
        Double_t w() const { return fW; }
        Double_t X() const { return fX; }
        Double_t Y() const { return fY; }
        Double_t Z() const { return fZ; }
        Double_t W() const { return fW; }
        Double_t GetX() const { return fX; }
        Double_t GetY() const { return fY; }
        Double_t GetZ() const { return fZ; }
        Double_t GetCharge() const { return fW; }
        TVector3 GetPosition() const { return TVector3(fX,fY,fZ); }
        LKVector3 GetPosition(LKVector3::Axis ref) const { return LKVector3(fX,fY,fZ,ref); }
        TVector3 GetDPosition() const { return TVector3(fDX,fDY,fDZ); }
        TVector3 GetPositionError() const { return TVector3(fDX,fDY,fDZ); }
        Double_t GetDX() const { return fDX; }
        Double_t GetDY() const { return fDY; }
        Double_t GetDZ() const { return fDZ; }
        Double_t GetTb() const { return fTb; }
        Int_t GetHitID() const { return fHitID; }
        Int_t GetTrackID() const { return fTrackID; }
        Int_t GetChannelID() const { return fChannelID; }
        Int_t GetPadID() const { return fPadID; }
        Double_t GetPedestal() const { return fPedestal; }
        Double_t GetAlpha() const { return fAlpha; }
        Int_t GetSection() const { return fSection; }
        Int_t GetLayer() const { return fLayer; }
        Int_t GetRow() const { return fRow; }
        Double_t GetSortValue() const { return fSortValue; }
        Double_t WeightPositionError() const;

        TVector3 GetMean()          const { return fHitArray.GetMean();          }
        TVector3 GetVariance()      const { return fHitArray.GetVariance();      }
        TVector3 GetCovariance()    const { return fHitArray.GetCovariance();    }
        TVector3 GetStdDev()        const { return fHitArray.GetStdDev();        }
        TVector3 GetSquaredMean()   const { return fHitArray.GetSquaredMean();   }
        TVector3 GetCoSquaredMean() const { return fHitArray.GetCoSquaredMean(); }

        inline Double_t & operator[](int i) {
            switch(i) {
                case 0:
                    return fX;
                case 1:
                    return fY;
                case 2:
                    return fZ;
                default:
                    Error("operator[](i)", "bad index (%d) returning 0",i);
            }
            return fX;
        }

        inline Double_t operator[](int i) const {
            switch(i) {
                case 0:
                    return fX;
                case 1:
                    return fY;
                case 2:
                    return fZ;
                default:
                    Error("operator[](i)", "bad index (%d) returning 0",i);
            }
            return 0.;
        }


        vector<Int_t> *GetTrackCandArray();
        Int_t GetNumTrackCands();
        Int_t GetTrackCand(Int_t id);
        void AddTrackCand(Int_t id);
        void RemoveTrackCand(Int_t trackID);
        bool FindTrackCand(Int_t id);
        bool PropagateTrackCand();

        virtual bool DrawByDefault();
        virtual bool IsEveSet();
#ifdef ACTIVATE_EVE
        virtual TEveElement *CreateEveElement();
        virtual void SetEveElement(TEveElement *, Double_t scale=1);
        virtual void AddToEveSet(TEveElement *eveSet, Double_t scale=1);
#endif

        ClassDef(LKHit, 4)
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

class LKHitSortSectionRow {
    public:
        LKHitSortSectionRow();
        bool operator() (LKHit* h1, LKHit* h2) {
            if (h1 -> GetSection() == h2 -> GetSection())
                return h1 -> GetRow() < h2 -> GetRow();
            return h1 -> GetSection() < h2 -> GetSection();
        }
};

class LKHitSortSectionLayer {
    public:
        LKHitSortSectionLayer();
        bool operator() (LKHit* h1, LKHit* h2) {
            if (h1 -> GetSection() == h2 -> GetSection())
                return h1 -> GetLayer() < h2 -> GetLayer();
            return h1 -> GetSection() < h2 -> GetSection();
        }
};

#endif
