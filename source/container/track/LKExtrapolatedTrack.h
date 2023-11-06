#ifndef LKEXTRAPOLATEDTRACK_HH
#define LKEXTRAPOLATEDTRACK_HH

#include "TVector3.h"
#include "TObject.h"
#include <vector>
#include "LKTracklet.h"

using namespace std;

/**
 * Track holding discrete points (which should be filled) from tail to head.
 * The track position is calculated by 2-point (line) extrapolation.
 */

class LKExtrapolatedTrack : public LKTracklet
{
    public:
        LKExtrapolatedTrack();
        virtual ~LKExtrapolatedTrack() {};

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option="") const;



        virtual TVector3 Momentum(Double_t B = 0.5) const;
        virtual TVector3 PositionAtHead() const;
        virtual TVector3 PositionAtTail() const;
        virtual Double_t TrackLength() const;

        virtual TVector3 ExtrapolateTo(TVector3) const;
        virtual TVector3 ExtrapolateHead(Double_t) const;
        virtual TVector3 ExtrapolateTail(Double_t) const;
        virtual TVector3 ExtrapolateByRatio(Double_t) const { return TVector3(); } // TODO

        virtual Double_t LengthAt(TVector3) const { return 0.; } // TODO

        virtual TGraphErrors *TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, Double_t scale=1);
        virtual TGraphErrors *TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos), Double_t scale=1);



        void SetParentID(Int_t id);
        void SetTrackID(Int_t id);
        void AddPoint(TVector3 point, Double_t length = -1);

        Int_t GetParentID() const;
        Int_t GetTrackID() const;
        vector<TVector3> *GetPoints();
        vector<Double_t> *GetLengths(); 

        Int_t GetNumPoints() const;

        virtual TVector3 ExtrapolateByLength(Double_t length) const;

    private:
        Int_t fParentID;
        Int_t fTrackID;

        vector<TVector3> fPoints;
        vector<Double_t> fLengths;

        ClassDef(LKExtrapolatedTrack, 1)
};

#endif
