#ifndef LKPADPLANE
#define LKPADPLANE

#include "LKPad.hpp"
#include "LKDetectorPlane.hpp"
#include "LKHitArray.hpp"

#include "TVector2.h"
#include "TH2.h"

/**
 *  Any PadPlane class should inherit this class and implement following methods:
 *
 *  - bool Init()
 *  : Initial parameters and setting should be done in this method.
 *  This method is called when LKRun::Init() is called.
 *  If the class is used separately, the method should be called manually.
 *
 *  - bool IsInBoundary(Double_t i, Double_t j)
 *  : Check if the position (i,j) is inside the pad-plane boundary and return true if so, and return false else.
 *
 *  - TH2* GetHist(Option_t *option)
 *  : This method is not essential but returned histogram is used in EVE.
 *
 *  - Int_t FindPadID(Double_t i, Double_t j)
 *  : Find and return padID from the given position (i,j)
 *
 *  - Int_t FindPadID(Int_t section, Int_t row, Int_t layer)
 *  : Find and return padID from the given (section, row, layer)
 *
 *  - Double_t PadDisplacement() const
 *  : Should return approximate displacements between pads. Doesn't have to be exact.
 *  One example of use of this method is calculation of the track continuity in track finding.
 */
class LKPadPlane : public LKDetectorPlane
{
    public:
        LKPadPlane();
        LKPadPlane(const char *name, const char *title);
        virtual ~LKPadPlane() {};

        virtual void Print(Option_t *option = "") const;
        virtual void Clear(Option_t *option = "");

        virtual Int_t FindPadID(Double_t i, Double_t j) = 0;
        virtual Int_t FindPadID(Int_t section, Int_t row, Int_t layer) = 0;

        virtual Int_t FindChannelID(Double_t i, Double_t j);

        virtual Double_t PadDisplacement() const = 0; ///< Rough (maximum) value of displacements between pads

        //virtual bool DrawEvent(Option_t *option = ""); // options: hit, in, raw, out, section, row, layer, padid, nhit
        virtual bool SetDataFromBranch();
        virtual bool FillDataToHist(Option_t *option = "");
        virtual void DrawHist();

    public:
        LKPad *GetPadFast(Int_t padID);
        LKPad *GetPad(Int_t padID);

        LKPad *GetPad(Double_t i, Double_t j);
        LKPad *GetPad(Int_t section, Int_t row, Int_t layer);

        //LKPad *GetPadByPadID(Int_t padID);

        void SetPadArray(TClonesArray *padArray);
        void SetHitArray(TClonesArray *hitArray);
        Int_t GetNumPads();

        void FillBufferIn(Double_t i, Double_t j, Double_t tb, Double_t val, Int_t trackID = -1);

        void SetPlaneK(Double_t k);
        Double_t GetPlaneK();

        virtual void ResetHitMap();
        virtual void ResetEvent();
        void AddHit(LKTpcHit *hit);

        virtual LKTpcHit *PullOutNextFreeHit();
        void PullOutNeighborHits(vector<LKTpcHit*> *hits, vector<LKTpcHit*> *neighborHits);
        void PullOutNeighborHits(TVector2 p, Int_t range, vector<LKTpcHit*> *neighborHits);
        void PullOutNeighborHits(Double_t x, Double_t y, Int_t range, vector<LKTpcHit*> *neighborHits);

        void PullOutNeighborHits(LKHitArray *hits, LKHitArray *neighborHits);
        void PullOutNeighborHits(Double_t x, Double_t y, Int_t range, LKHitArray *neighborHits);

        void GrabNeighborPads(vector<LKPad*> *pads, vector<LKPad*> *neighborPads);
        TObjArray *GetPadArray();

        bool PadPositionChecker(bool checkCorners = true);
        bool PadNeighborChecker();

    protected:
        Int_t fEFieldAxis = -1;
        Double_t fPlaneK = -999;

        Int_t fFreePadIdx = 0;

        bool fFilledPad = false;
        bool fFilledHit = false;

    ClassDef(LKPadPlane, 1)
};

#endif
