#ifndef LKTPC_HH
#define LKTPC_HH

#include "LKDetector.hpp"
#include "LKPadPlane.hpp"

#include "TVector3.h"
#include "LKVector3.hpp"

class LKTpc : public LKDetector
{
    public:
        LKTpc();
        LKTpc(const char *name, const char *title);
        virtual ~LKTpc() {};

        virtual LKPadPlane *GetPadPlane(Int_t idx = 0);
        virtual LKVector3::Axis GetEFieldAxis() = 0;

                TVector3 GetEField(Double_t x, Double_t y, Double_t z);
        virtual TVector3 GetEField(TVector3 pos) = 0;

                LKPadPlane *GetDriftPlane(Double_t x, Double_t y, Double_t z);
        virtual LKPadPlane *GetDriftPlane(TVector3 pos) = 0;

        ClassDef(LKTpc, 1)
};

#endif
