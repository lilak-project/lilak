#ifndef LKWPOINT_HH
#define LKWPOINT_HH

#include "TError.h"
#include "TVector3.h"

#include "LKContainer.h"
#include "LKVector3.h"

#ifdef ACTIVATE_EVE
#include "TEveElement.h"
#include "TEvePointSet.h"
#endif

/// position data with weight

class LKWPoint : public LKContainer
{
    public:
        LKWPoint();
        LKWPoint(Double_t x, Double_t y, Double_t z, Double_t w = 1);

        virtual void Print(Option_t *option = "") const;
        virtual void Clear(Option_t *option = "");
        virtual void Copy (TObject &object) const;

        void Set(Double_t x, Double_t y, Double_t z, Double_t w = 1);
        void SetWeight(Double_t w);
        void SetPosition(Double_t x, Double_t y, Double_t z);
        void SetPosition(TVector3 pos);

        TVector3 GetPosition()                    const { return  TVector3(fX,fY,fZ); }
        LKVector3 GetPosition(LKVector3::Axis ref) const { return LKVector3(fX,fY,fZ,ref); }

        Double_t x() const { return fX; }
        Double_t y() const { return fY; }
        Double_t z() const { return fZ; }
        Double_t w() const { return fW; }

        Double_t X() const { return fX; }
        Double_t Y() const { return fY; }
        Double_t Z() const { return fZ; }
        Double_t W() const { return fW; }

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

#ifdef ACTIVATE_EVE
        virtual bool DrawByDefault();
        virtual bool IsEveSet();
        virtual TEveElement *CreateEveElement();
        virtual void SetEveElement(TEveElement *element, Double_t scale = 1);
        virtual void AddToEveSet(TEveElement *eveSet, Double_t scale = 1);
#endif

    protected:
        Double_t fX;
        Double_t fY;
        Double_t fZ;
        Double_t fW;

    ClassDef(LKWPoint, 1)
};

#endif
