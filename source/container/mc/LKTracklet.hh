#ifndef LKTRACKLET_HH
#define LKTRACKLET_HH

#include "TGraph.h"

#include "LKMCTagged.hpp"

#include "LKHitArray.hh"
#include "LKHit.hh"
#include "LKVector3.hh"
#include "LKDetectorPlane.hh"

#include <functional>

class LKTracklet : public LKMCTagged
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

    virtual void PropagateMC();

    void SetTrackID(Int_t val) { fTrackID = val; }
    Int_t GetTrackID() const { return fTrackID; }

    void SetParentID(Int_t val) { fParentID = val; }
    Int_t GetParentID() const { return fParentID; }

    void SetPDG(Int_t val) { fPDG = val; }
    Int_t GetPDG() const { return fPDG; }

    LKHitArray *GetHitArray() { return &fHitArray; }

    virtual void AddHit(LKHit *hit);
    virtual void RemoveHit(LKHit *hit);

    virtual bool Fit() { return true; }

    //virtual TVector3 Momentum(Double_t B = 0.5) const = 0; ///< Momentum of track at head.
    //virtual TVector3 PositionAtHead() const = 0; ///< Position at head of helix
    //virtual TVector3 PositionAtTail() const = 0; ///< Position at tail of helix
    //virtual Double_t TrackLength() const = 0; ///< Length of track calculated from head to tail.
    //virtual TVector3 ExtrapolateTo(TVector3 point) const = 0; ///< Extrapolate to POCA from point, returns extrapolated position
    //virtual TVector3 ExtrapolateHead(Double_t l) const = 0; ///< Extrapolate head of track about length, returns extrapolated position
    //virtual TVector3 ExtrapolateTail(Double_t l) const = 0; ///< Extrapolate tail of track about length, returns extrapolated position
    //virtual TVector3 ExtrapolateByRatio(Double_t r) const = 0; ///< Extrapolate by ratio (tail:0, head:1), returns extrapolated position
    //virtual TVector3 ExtrapolateByLength(Double_t l) const = 0; ///< Extrapolate by length (tail:0), returns extrapolated position
    //virtual Double_t LengthAt(TVector3 point) const = 0; ///< Length at POCA from point, where tail=0, head=TrackLength

    virtual double Energy(int alpha=0) const = 0; ///< Kinetic energy of track at vertex.
    virtual TVector3 Momentum(int alpha=0) const = 0; ///< Momentum of track at vertex.

    virtual GetAlphaTail() const;       ///< Alpha at tail (reconstructed back end)
    virtual Double_t LengthToAlpha(double length) const = 0;        ///< Convert track-length (mm?) to alpha
    virtual Double_t AlphaToLength(double alpha) const = 0;             ///< Convert alpha to track-length (mm?)
    virtual Double_t TrackLength(double a1=0, double a2=1) const = 0;   ///< Length of track between two alphas (default: from vertex to head).

    /**
     * Extrapolated position at given alpha.
     * Alpha (double) is scaled length variable along the track where
     * alpha=0 is position of vertex and
     * alpha=1 is position of head (reconstructed front end).
     */
    virtual TVector3 ExtrapolateToAlpha(double alpha) const = 0;
    virtual TVector3 ExtrapolateHead(Double_t dalpha) const = 0;        ///< Extrapolate head by dalpha and return position
    virtual TVector3 ExtrapolateTail(Double_t dalpha) const = 0;        ///< Extrapolate tail by dalpha and return position
    virtual TVector3 ExtrapolateToPosition(TVector3 point) const = 0;   ///< Extrapolate and return POCA from point

#ifdef ACTIVATE_EVE
    virtual bool DrawByDefault();
    virtual bool IsEveSet();
    virtual TEveElement *CreateEveElement();
    virtual void SetEveElement(TEveElement *, Double_t scale=1);
    virtual void AddToEveSet(TEveElement *eveSet, Double_t scale=1);
#endif

    virtual bool DoDrawOnDetectorPlane();
    virtual TGraph *TrajectoryOnPlane(kbaxis_t axis1, kbaxis_t axis2, bool (*fisout)(TVector3 pos), Double_t scale=1);
    virtual TGraph *TrajectoryOnPlane(kbaxis_t axis1, kbaxis_t axis2, Double_t scale=1);
    virtual TGraph *TrajectoryOnPlane(LKDetectorPlane *plane, Double_t scale=1);

    virtual TGraph *CrossSectionOnPlane(kbaxis_t, kbaxis_t, Double_t) { return (TGraph *) nullptr; }

  ClassDef(LKTracklet, 3)
};

#endif
