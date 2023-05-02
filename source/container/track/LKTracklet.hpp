#ifndef LKTRACKLET_HH
#define LKTRACKLET_HH

#include "TGraph.h"

#include "LKContainer.hpp"

#include "LKHitArray.hpp"
#include "LKHit.hpp"
#include "LKDetectorPlane.hpp"

#include <functional>

class LKTracklet : public LKContainer
{
    protected:
        Int_t fTrackID = -1;
        Int_t fParentID = -1;  ///< Vertex ID
        Int_t fPDG = -1;

        LKHitArray fHitArray; //!

        TGraph *fTrajectoryOnPlane = nullptr; //!

    public:
        LKTracklet() {}
        virtual ~LKTracklet() {}

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "") const;
        virtual void Copy (TObject &object) const;

        //virtual void PropagateMC();

        void SetTrackID(Int_t val) { fTrackID = val; }
        void SetParentID(Int_t val) { fParentID = val; }
        void SetPDG(Int_t val) { fPDG = val; }

        Int_t GetTrackID() const { return fTrackID; }
        Int_t GetParentID() const { return fParentID; }
        Int_t GetPDG() const { return fPDG; }

        LKHitArray *GetHitArray() { return &fHitArray; }

        virtual void AddHit(LKHit *hit);
        virtual void RemoveHit(LKHit *hit);

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

#ifdef ACTIVATE_EVE
        virtual bool DrawByDefault();
        virtual bool IsEveSet();
        virtual TEveElement *CreateEveElement();
        virtual void SetEveElement(TEveElement *, Double_t scale=1);
        virtual void AddToEveSet(TEveElement *eveSet, Double_t scale=1);
#endif

        virtual bool DoDrawOnDetectorPlane();
        virtual TGraph *TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos), Double_t scale=1);
        virtual TGraph *TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, Double_t scale=1);
        virtual TGraph *TrajectoryOnPlane(LKDetectorPlane *plane, Double_t scale=1);
        //virtual TGraph *CrossSectionOnPlane(TVector3, TVector3, Double_t) { return (TGraph *) nullptr; }

        ClassDef(LKTracklet, 0)
};

#endif
