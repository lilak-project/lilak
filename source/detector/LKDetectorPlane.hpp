#ifndef LKDETECTORPLANE_HH
#define LKDETECTORPLANE_HH

#include "LKChannel.hpp"
#include "LKVector3.hpp"
#include "LKGear.hpp"

#include "TH2.h"
#include "TCanvas.h"
#include "TObject.h"
#include "TNamed.h"
#include "TObjArray.h"
#include "TClonesArray.h"

class LKDetectorPlane : public TNamed, public LKGear
{
    public:
        LKDetectorPlane();
        LKDetectorPlane(const char *name, const char *title);
        virtual ~LKDetectorPlane() {};

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "") const;
        virtual bool Init() = 0;

        virtual bool IsInBoundary(Double_t i, Double_t j) = 0;

        virtual Int_t FindChannelID(Double_t i, Double_t j) = 0;

        virtual TCanvas *GetCanvas(Option_t *option = "");
        virtual void DrawFrame(Option_t *option = "");
        virtual TH2* GetHist(Option_t *option = "-1") = 0;

        virtual LKVector3::Axis GetAxis1();// { return TVector3(1,0,0); }
        virtual LKVector3::Axis GetAxis2();// { return TVector3(0,1,0); }

    public:
        void SetPlaneID(Int_t id);
        Int_t GetPlaneID() const;

        void AddChannel(LKChannel *channel);

        LKChannel *GetChannelFast(Int_t idx);
        LKChannel *GetChannel(Int_t idx);

        Int_t GetNChannels();
        TObjArray *GetChannelArray();

    protected:
        TObjArray *fChannelArray = nullptr;

        Int_t fPlaneID = -1;

        TCanvas *fCanvas = nullptr;
        TH2 *fH2Plane = nullptr;

        ClassDef(LKDetectorPlane, 0)
};

#endif
