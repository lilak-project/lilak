#ifndef LKLINEARTRACK_HH
#define LKLINEARTRACK_HH

#include "LKTracklet.h"
#include "LKGeoLine.h"

typedef LKVector3::Axis axis_t;

class LKLinearTrack : public LKTracklet, public LKGeoLine
{
    public:
        LKLinearTrack();
        LKLinearTrack(Double_t x1, Double_t y1, Double_t z1, Double_t x2, Double_t y2, Double_t z2);
        LKLinearTrack(TVector3 pos1, TVector3 pos2);
        virtual ~LKLinearTrack() {};

        void Clear(Option_t *option = "");
        virtual void Print(Option_t *option="") const;

        virtual void SetLine(Double_t x1, Double_t y1, Double_t z1, Double_t x2, Double_t y2, Double_t z2);
        virtual void SetLine(TVector3 pos1, TVector3 pos2);
        virtual void SetLine(LKGeoLine *line);

        virtual bool Fit();

        void SetQuality(Double_t val);
        Double_t GetQuality();

        void SetTrack(TVector3 pos1, TVector3 pos2);

        virtual TVector3 Momentum(Double_t B = 0.5) const;
        virtual TVector3 PositionAtHead() const;
        virtual TVector3 PositionAtTail() const;
        virtual Double_t TrackLength() const;

        virtual TVector3 ExtrapolateTo(TVector3 point) const;
        virtual TVector3 ExtrapolateHead(Double_t l) const;
        virtual TVector3 ExtrapolateTail(Double_t l) const;
        virtual TVector3 ExtrapolateByRatio(Double_t r) const;
        virtual TVector3 ExtrapolateByLength(Double_t l) const;

        virtual Double_t LengthAt(TVector3 point) const;

        virtual TGraph *TrajectoryOnPlane(axis_t axis1, axis_t axis2, Double_t scale=1);
        virtual TGraph *TrajectoryOnPlane(LKDetectorPlane *plane, Double_t scale=1);
        virtual TGraph *CrossSectionOnPlane(axis_t axis1, axis_t axis2, Double_t scale=1);

    protected:
        Double_t fQuality = -1;
        Double_t fWidth = -1;
        Double_t fHeight = -1;
        TVector3 fPerpDirectionInPlane;

        ClassDef(LKLinearTrack, 1)
};

#endif
