#ifndef LKTRACKLET_HH
#define LKTRACKLET_HH

#include "TGraphErrors.h"
#include "TGraph2DErrors.h"

#include "LKContainer.h"

#include "LKHitArray.h"
#include "LKHit.h"
#include "LKDetectorPlane.h"

#include <functional>

class LKTracklet : public LKContainer
{
    protected:
        Int_t fTrackID = -1;
        Int_t fParentID = -1;  ///< Vertex ID
        Int_t fPDG = -1;

        LKHitArray fHitArray; //!
        vector<Int_t> fHitIDArray;

        TGraphErrors *fTrajectoryOnPlane = nullptr; //!

    public:
        LKTracklet() {}
        virtual ~LKTracklet() {}

        virtual void Clear(Option_t *option = "");
        //virtual void Print(Option_t *option = "") const;
        //virtual void Copy (TObject &object) const;

        //virtual void PropagateMC();

        virtual void ClearHits();

        void SetTrackID(Int_t val) { fTrackID = val; }
        void SetParentID(Int_t val) { fParentID = val; }
        void SetPDG(Int_t val) { fPDG = val; }

        Int_t GetTrackID() const { return fTrackID; }
        Int_t GetParentID() const { return fParentID; }
        Int_t GetPDG() const { return fPDG; }

        Int_t GetNumHits() const { return fHitIDArray.size(); }
        LKHit *GetHit(Int_t idx) const { return fHitArray.GetHit(idx); }
        Int_t GetHitID(Int_t idx) const { return fHitIDArray[idx]; }
        LKHitArray *GetHitArray() { return &fHitArray; }
        std::vector<Int_t> *GetHitIDArray() { return &fHitIDArray; }

        virtual void AddHit(LKHit *hit);
        virtual void RemoveHit(LKHit *hit);

        bool IsHoldingHits();

        virtual bool Fit() { return true; }

        virtual TVector3 Momentum(Double_t B = 0.5) const = 0; ///< Momentum of track at head.
        virtual TVector3 PositionAtHead() const = 0; ///< Position at head of helix
        virtual TVector3 PositionAtTail() const = 0; ///< Position at tail of helix
        virtual Double_t TrackLength() const = 0; ///< Length of track calculated from head to tail.

        virtual TVector3 ExtrapolateTo(TVector3 point) const = 0; ///< Extrapolate to POCA from point, returns extrapolated position
        virtual TVector3 ExtrapolateHead(Double_t l) const = 0; ///< Extrapolate head of track about length, returns extrapolated position
        virtual TVector3 ExtrapolateTail(Double_t l) const = 0; ///< Extrapolate tail of track about length, returns extrapolated position
        virtual TVector3 ExtrapolateByRatio(Double_t r) const = 0; ///< Extrapolate by ratio (tail:0, head:1), returns extrapolated position
        virtual TVector3 ExtrapolateByLength(Double_t l) const = 0; ///< Extrapolate by length (tail:0), returns extrapolated position

        virtual Double_t LengthAt(TVector3 point) const = 0; ///< Length at POCA from point, where tail=0, head=TrackLength

        virtual bool DrawByDefault();
#ifdef ACTIVATE_EVE
        virtual bool IsEveSet();
        virtual TEveElement *CreateEveElement();
        virtual void SetEveElement(TEveElement *, Double_t scale=1);
        virtual void AddToEveSet(TEveElement *eveSet, Double_t scale=1);
#endif

        virtual bool DoDrawOnDetectorPlane();
        virtual TGraphErrors *TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos), Double_t scale=1);
        virtual TGraphErrors *TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, Double_t scale=1);
        virtual TGraphErrors *TrajectoryOnPlane(LKDetectorPlane *plane, Double_t scale=1);
        void FillTrajectory(TGraphErrors* graphTrack, LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos));
        void FillTrajectory(TGraphErrors* graphTrack, LKVector3::Axis axis1, LKVector3::Axis axis2);
        void FillTrajectory3D(TGraph2DErrors* graphTrack3D, LKVector3::Axis axis1, LKVector3::Axis axis2, LKVector3::Axis axis3, bool (*fisout)(TVector3 pos));
        void FillTrajectory3D(TGraph2DErrors* graphTrack3D, LKVector3::Axis axis1, LKVector3::Axis axis2, LKVector3::Axis axis3);
        //virtual TGraph *CrossSectionOnPlane(TVector3, TVector3, Double_t) { return (TGraph *) nullptr; }

        ClassDef(LKTracklet, 1)
};

#endif
