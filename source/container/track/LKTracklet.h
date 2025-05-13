#ifndef LKTRACKLET_HH
#define LKTRACKLET_HH

#include "TGraphErrors.h"
#include "TGraph2DErrors.h"
#include "TClonesArray.h"

#include "LKContainer.h"

#include "LKHitArray.h"
#include "LKHit.h"
#include "LKDetectorPlane.h"

#include <functional>

class LKTracklet : public LKContainer
{
    public:
        enum LKFitStatus : int {
            kNone = -1,
            kBad = 0,
            kLine = 1,
            kPlane = 2,
            kHelix = 3,
            kGenfitTrack = 4
        };

        void SetFitStatus(LKFitStatus value)  { fFitStatus = value; }
        void SetIsBad()          { fFitStatus = LKTracklet::kBad; }
        void SetIsLine()         { fFitStatus = LKTracklet::kLine; }
        void SetIsPlane()        { fFitStatus = LKTracklet::kPlane; }
        void SetIsHelix()        { fFitStatus = LKTracklet::kHelix; }
        void SetIsGenfitTrack()  { fFitStatus = LKTracklet::kGenfitTrack; }

        LKTracklet::LKFitStatus GetFitStatus() const { return fFitStatus; }
        bool IsBad() const          { return (fFitStatus==kBad); }
        bool IsLine()  const        { return (fFitStatus==kLine); }
        bool IsPlane() const        { return (fFitStatus==kPlane); }
        bool IsHelix() const        { return (fFitStatus==kHelix); }
        bool IsGenfitTrack() const  { return (fFitStatus==kGenfitTrack); }

        TString GetFitStatusString() const {
            if      (fFitStatus==kBad) return "Bad";
            else if (fFitStatus==kLine) return "Line";
            else if (fFitStatus==kPlane) return "Plane";
            else if (fFitStatus==kHelix) return "Helix";
            else if (fFitStatus==kGenfitTrack) return "Genfit";
            return "Bad";
        }

    protected:
        int fTrackID = -1;
        int fParentID = -1;  ///< Vertex ID
        int fPDG = -1;

        LKHitArray fHitArray; //!
        vector<int> fHitIDArray;

        TGraphErrors *fTrajectoryOnPlane = nullptr; //!

        LKFitStatus fFitStatus;  ///< One of kBad, kHelix and kLine.

    public:
        LKTracklet() {}
        virtual ~LKTracklet() {}

        virtual void Clear(Option_t *option = "");
        //virtual void Print(Option_t *option = "") const;
        virtual void Copy (TObject &object) const;

        //virtual void PropagateMC();

        virtual void ClearHits();

        void SetTrackID(int val) { fTrackID = val; }
        void SetParentID(int val) { fParentID = val; }
        void SetPDG(int val) { fPDG = val; }

        int GetTrackID() const { return fTrackID; }
        int GetParentID() const { return fParentID; }
        int GetPDG() const { return fPDG; }

        int GetNumHits() const { return fHitIDArray.size(); }
        LKHit *GetHit(int idx) const { return fHitArray.GetHit(idx); }
        int GetHitID(int idx) const { return fHitIDArray[idx]; }
        LKHitArray *GetHitArray() { return &fHitArray; }
        std::vector<int> *GetHitIDArray() { return &fHitIDArray; }

        TVector3 GetMean()         const { return fHitArray.GetMean(); }
        double GetChargeSum()    const { return fHitArray.GetW(); }

        virtual void AddHit(LKHit *hit);
        virtual void RemoveHit(LKHit *hit);
        virtual void FinalizeHits();

        /// Be cautious on using this method!
        /// This method assumes that hit array and track were cretaed in same run and event
        /// This method will recover hit-array of track, which is removed before end of LILAK run.
        /// The track ID is set in hit container. The method will use this information to recover hit array.
        virtual int RecoverHitArrayWithTrackID(TClonesArray* hitArray);

        /// Be cautious on using this method!
        /// This method assumes that hit array was used to create track in same run and event
        /// This method will recover hit-array of track, which is removed before end of LILAK run.
        /// The track store hit IDs. The method will use this information to recover hit array.
        virtual int RecoverHitArrayWithHitID(TClonesArray* hitArray);

        bool IsHoldingHits();

        virtual bool Fit() { return true; }
        virtual LKGeoLine   FitLine();
        virtual LKGeoPlane  FitPlane();
        virtual LKGeoHelix  FitHelix(LKVector3::Axis ref);

        virtual void SortHits(bool increasing=true);

        virtual TVector3 Momentum(double B = 0.5) const = 0; ///< Momentum of track at head.
        virtual TVector3 PositionAtHead() const = 0; ///< Position at head of helix
        virtual TVector3 PositionAtTail() const = 0; ///< Position at tail of helix
        virtual double TrackLength() const = 0; ///< Length of track calculated from head to tail.

        virtual TVector3 ExtrapolateTo(TVector3 point) const = 0; ///< Extrapolate to POCA from point, returns extrapolated position
        virtual TVector3 ExtrapolateHead(double l) const = 0; ///< Extrapolate head of track about length, returns extrapolated position
        virtual TVector3 ExtrapolateTail(double l) const = 0; ///< Extrapolate tail of track about length, returns extrapolated position
        virtual TVector3 ExtrapolateByRatio(double r) const = 0; ///< Extrapolate by ratio (tail:0, head:1), returns extrapolated position
        virtual TVector3 ExtrapolateByLength(double l) const = 0; ///< Extrapolate by length (tail:0), returns extrapolated position

        virtual double LengthAt(TVector3 point) const = 0; ///< Length at POCA from point, where tail=0, head=TrackLength

        virtual bool DrawByDefault();
#ifdef ACTIVATE_EVE
        virtual bool IsEveSet();
        virtual TEveElement *CreateEveElement();
        virtual void SetEveElement(TEveElement *, double scale=1);
        virtual void AddToEveSet(TEveElement *eveSet, double scale=1);
#endif

        virtual bool DoDrawOnDetectorPlane();
        virtual TGraphErrors *TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos), double scale=1);
        virtual TGraphErrors *TrajectoryOnPlane(LKVector3::Axis axis1, LKVector3::Axis axis2, double scale=1);
        virtual TGraphErrors *TrajectoryOnPlane(LKDetectorPlane *plane, double scale=1);
        virtual void FillTrajectory(TGraphErrors* graphTrack, LKVector3::Axis axis1, LKVector3::Axis axis2, bool (*fisout)(TVector3 pos));
        virtual void FillTrajectory(TGraphErrors* graphTrack, LKVector3::Axis axis1, LKVector3::Axis axis2);
        virtual void FillTrajectory3D(TGraph2DErrors* graphTrack3D, LKVector3::Axis axis1, LKVector3::Axis axis2, LKVector3::Axis axis3, bool (*fisout)(TVector3 pos));
        virtual void FillTrajectory3D(TGraph2DErrors* graphTrack3D, LKVector3::Axis axis1, LKVector3::Axis axis2, LKVector3::Axis axis3);
        //virtual TGraph *CrossSectionOnPlane(TVector3, TVector3, double) { return (TGraph *) nullptr; }

        /**
         * Check continuity of the track. Hit array must be filled.
         * Returns ratio of the continuous region. (-1 if less than 2 hits)
         *
         * If difference between "length from hits" is smaller than distCut,
         * (> pad-diagonal, < 2*pad-row) the region is considered to be connected.
         *
         * The total length calculation is different from TrackLength(),
         * because TrackLength() use only the head/tail alpha angle information
         * but this method calculate length directly from the hit using
         * Map(...) method.
         *
         * @param continuousLength  calculated length of continuos region.
         * @param totalLength       calculated total length of the track.
         */
        virtual double Continuity(double &totalLength, double &continuousLength, double distCut=25);
        virtual double Continuity(double distCut=25) { double l1, l2; return Continuity(l1, l2, distCut); }

        ClassDef(LKTracklet, 1)
};

#endif
