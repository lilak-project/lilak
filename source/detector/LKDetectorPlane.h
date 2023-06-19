#ifndef LKDETECTORPLANE_HH
#define LKDETECTORPLANE_HH

#include "LKChannel.h"
#include "LKVector3.h"
#include "LKGear.h"
#include "LKRun.h"

#include "TH2.h"
#include "TCanvas.h"
#include "TObject.h"
#include "TNamed.h"
#include "TObjArray.h"
#include "TClonesArray.h"

typedef LKVector3::Axis axis_t;

class LKRun;
class LKDetector;

class LKDetectorPlane : public TNamed, public LKGear
{
    public:
        LKDetectorPlane();
        LKDetectorPlane(const char *name, const char *title);
        virtual ~LKDetectorPlane() {};

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "") const;
        virtual bool Init() = 0;

        void SetDetector(LKDetector *detector) { fDetector = detector; }

        virtual bool IsInBoundary(Double_t i, Double_t j) = 0;

        virtual Int_t FindChannelID(Double_t i, Double_t j) { return -1; }
        virtual Int_t FindChannelID(Int_t section, Int_t row, Int_t layer) { return -1; }

        virtual TCanvas *GetCanvas(Option_t *option = "");
        virtual TH2* GetHist(Option_t *option = "-1") = 0;
        virtual bool SetDataFromBranch() { return false; }
        virtual void FillDataToHist() {};
        virtual void DrawFrame(Option_t *option = "") {}
        virtual void Draw(Option_t *option = "");

        void SetAxis(axis_t axis1, axis_t axis2);
        axis_t GetAxis1();
        axis_t GetAxis2();

    private:
        void MouseClickEvent(int iPlane);

    protected:
        virtual void ClickedAtPosition(Double_t x, Double_t y) {}

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

        axis_t fAxis1 = LKVector3::kX;
        axis_t fAxis2 = LKVector3::kY;

        LKDetector *fDetector = nullptr;

    ClassDef(LKDetectorPlane, 1)
};

#endif
