#ifndef LKGEOROTATED_HH
#define LKGEOROTATED_HH

#include "LKGeometry.h"
#include "TRotation.h"

class LKGeoRotated : public LKGeometry
{
    protected:
        TRotation fRotation;

    public:
        LKGeoRotated() {}
        virtual ~LKGeoRotated() {}

        virtual void Clear(Option_t *option = "");

        virtual TVector3 GetCenter() const { return TVector3(); }

        TVector3 Rotate(TVector3 pos) const; //< rotate due to center position
        TVector3 InvRotate(TVector3 pos) const; //< inverse rotate due to center position

        void SetRotation(TRotation rot) { fRotation = rot; }
        TRotation GetRotation() const { return fRotation; }

        ClassDef(LKGeoRotated, 0)
};

#endif
