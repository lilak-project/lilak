#ifndef LKHELIXTRACK_HH
#define LKHELIXTRACK_HH

#include "TString.h"

#include "LKHit.h"
#include "LKTracklet.h"
#include "LKGeoHelix.h"
#include "LKVector3.h"
#include <vector>

typedef LKVector3::Axis axis_t;

/**
 * SpiRIT Helix Track Container.
 *
 * All units in [mm], [ADC], [radian], [MeV].
 */

class LKHelixTrack : public LKTracklet, public LKGeoHelix
{
    private:
        Int_t    fGenfitID;        ///< GENFIT Track ID
        Double_t fGenfitMomentum;  ///< Momentum reconstructed by GENFIT

        Bool_t fIsPositiveChargeParticle;
        std::vector<Double_t> fdEdxArray;  ///< dE/dx array;

    public:
        LKHelixTrack();
        LKHelixTrack(Int_t id);
        virtual ~LKHelixTrack() {};

    public:
        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option="") const;
        virtual void Copy (TObject &object) const;
        //virtual const char *GetName() const;

#ifdef ACTIVATE_EVE
        virtual bool DrawByDefault();
        virtual bool IsEveSet();
        virtual TEveElement *CreateEveElement();
        virtual void SetEveElement(TEveElement *, Double_t scale=1);
        virtual void AddToEveSet(TEveElement *eveSet, Double_t scale=1);
#endif

        virtual bool Fit();
        virtual LKGeoPlane FitPlane();

        virtual void SortHits(bool increasing = true);
        void SortHitsByTimeOrder();

        /*
         * LINE
         */
        void SetLineDirection(TVector3 dir);  ///< If track is straight. (ONLY USED IN TRACK FINDING)
        LKVector3 GetLineDirection() const;   ///< If track is straight. (ONLY USED IN TRACK FINDING)
        LKVector3 PerpLine(TVector3 p) const; ///< If track is straight. (ONLY USED IN TRACK FINDING)

        /*
         * PLANE
         */
        void SetPlaneNormal(TVector3 norm);    ///< If track is straight. (ONLY USED IN TRACK FINDING)
        LKVector3 GetPlaneNormal() const;      ///< If track is straight. (ONLY USED IN TRACK FINDING)
        LKVector3 PerpPlane(TVector3 p) const; ///< If track is straight. (ONLY USED IN TRACK FINDING)

        /*
         * HELIX
         */
        void SetHelixCenter(Double_t i, Double_t j);
        void SetHelixRadius(Double_t r);
        void SetKInitial(Double_t k);
        void SetAlphaSlope(Double_t s);
        void SetAlphaHead(Double_t alpha);
        void SetAlphaTail(Double_t alpha);
        void SetReferenceAxis(LKVector3::Axis ref);

        //TVector3 GetMean() const; // mean value of hit positions
        Double_t GetHelixCenterJ() const;
        Double_t GetHelixCenterI() const;
        Double_t GetHelixRadius() const;
        Double_t GetKInitial() const;
        Double_t GetAlphaSlope() const;
        //Double_t GetChargeSum() const;
        LKVector3::Axis GetReferenceAxis() const;

        /*
         * GENFIT
         */
        void SetGenfitID(Int_t idx);
        Int_t GetGenfitID() const;

        void SetGenfitMomentum(Double_t p);
        Double_t GetGenfitMomentum() const;    /// Momentum reconstructed by genfit (if is set)

        /*
         */
        void SetIsPositiveChargeParticle(Bool_t val);
        Bool_t IsPositiveChargeParticle() const;

        void DetermineParticleCharge(TVector3 vertex); //@todo TODO

        /*
         */
        //Int_t GetNumHits() const;
        LKHit *GetHit(Int_t idx) const;
        Int_t GetHitID(Int_t idx) const;
        //LKHitArray *GetHitArray();
        //std::vector<Int_t> *GetHitIDArray();

        /*
         */
        std::vector<Double_t> *GetdEdxArray();
        Double_t GetdEdxWithCut(Double_t lowR, Double_t highR) const;

        /**
         * Extrapolate track due to alpha angle of the given point.
         * Returns alpha;
         */
        Double_t AlphaAtPosition(TVector3 p) const;

        /**
         * Extrapolate track due to given alpha.
         * Returns extrapolation length from the initial track reference position.
         */
        Double_t ExtrapolateToAlpha(Double_t alpha) const;

        /**
         * Extrapolate track due to given alpha.
         * Returns extrapolation length from the initial track reference position.
         *
         * @param alpha         given alpha.
         * @param pointOnHelix  extrapolated position on the helix.
         */
        Double_t ExtrapolateToAlpha(Double_t alpha, TVector3 &pointOnHelix) const;

        /**
         * Extrapolate track due to alpha angle of the given point.
         * Returns extrapolation length from the initial track reference position.
         *
         * @param pointGiven    given point.
         * @param pointOnHelix  extrapolated position on the helix.
         * @param alpha         extrapolated alpha angle.
         */
        Double_t ExtrapolateToPointAlpha(TVector3 pointGiven, TVector3 &pointOnHelix, Double_t &alpha) const;

        /**
         * Extrapolate track due to y-position of the given point.
         * Returns extrapolation length from the initial track reference position.
         *
         * @param pointGiven    given point.
         * @param pointOnHelix  extrapolated position on the helix.
         * @param alpha         extrapolated alpha angle.
         */
        Double_t ExtrapolateToPointK(TVector3 pointGiven, TVector3 &pointOnHelix, Double_t &alpha) const;

        /** Check feasibility of Extrapolation of track to zy-plane at x-position. */
        bool CheckExtrapolateToI(Double_t x) const;

        /** Check feasibility of Extrapolation of track to xy-plane at z-position. */
        bool CheckExtrapolateToJ(Double_t z) const;

        /**
         * Extrapolate track to zy-plane at x-position.
         * Returns true if extrapolation is possible, false if not.
         *
         * @param x              x-position of zy-plane.
         * @param pointOnHelix1  extrapolated position on the helix close to fAlphaHead
         * @param alpha1         extrapolated alpha angle close to fAlphaHead
         * @param pointOnHelix2  extrapolated position on the helix close to fAlphaTail
         * @param alpha2         extrapolated alpha angle close to fAlphaTail
         */
        bool ExtrapolateToI(Double_t x, 
                TVector3 &pointOnHelix1, Double_t &alpha1,
                TVector3 &pointOnHelix2, Double_t &alpha2) const;

        /**
         * Extrapolate track to xy-plane at z-position.
         * Returns true if extrapolation is possible, false if not.
         *
         * @param z              z-position of xy-plane.
         * @param pointOnHelix1  extrapolated position on the helix close to fAlphaHead
         * @param alpha1         extrapolated alpha angle close to fAlphaHead
         * @param pointOnHelix2  extrapolated position on the helix close to fAlphaTail
         * @param alpha2         extrapolated alpha angle close to fAlphaTail
         */
        bool ExtrapolateToJ(Double_t z,
                TVector3 &pointOnHelix1, Double_t &alpha1,
                TVector3 &pointOnHelix2, Double_t &alpha2) const;

        /**
         * Extrapolate track to zy-plane at x-position.
         * Returns true if extrapolation is possible, false if not.
         *
         * @param x              x-position of zy-plane.
         * @param alphaRef       reference position for extapolation
         * @param pointOnHelix   extrapolated position on the helix close to alphaRef
         */
        bool ExtrapolateToI(Double_t x, Double_t alphaRef, TVector3 &pointOnHelix) const;

        /**
         * Extrapolate track to xy-plane at z-position.
         * Returns true if extrapolation is possible, false if not.
         *
         * @param z              z-position of xy-plane
         * @param alphaRef       reference position for extapolation
         * @param pointOnHelix   extrapolated position on the helix close to alphaRef
         */
        bool ExtrapolateToJ(Double_t z, Double_t alphaRef, TVector3 &pointOnHelix) const;

        /**
         * Extrapolate track to xy-plane at z-position.
         * Returns true if extrapolation is possible, false if not.
         *
         * @param z             z-position of xy-plane.
         * @param pointOnHelix  extrapolated position close to tracjectory.
         */
        bool ExtrapolateToJ(Double_t z, TVector3 &pointOnHelix) const;

        /**
         * Interpolate between head and tail of helix.
         * Return interpolated position using ratio
         * Interpolate when 0 < r < 1.
         * Extrapolate when r < 0 or r > 1.
         */
        TVector3 InterpolateByRatio(Double_t r) const;

        /**
         * Interpolate between head and tail of helix.
         * Return interpolated position using length
         * Interpolate when 0 < length < TrackLength().
         * Extrapolate when length < 0 or length > TrackLength().
         */
        TVector3 InterpolateByLength(Double_t r) const;

        /**
         * Map and return position.
         * Concept of this mapping is to strecth helix to straight line.
         * - 1st axis : radial axis
         * - 2nd axis : normal to 1st and 3rd axis
         * - 3nd axis : length along helix line
         */
        TVector3 Map(TVector3 p) const;

        /**
         * Extrapolate to cloesest position from p to helix by Mapping.
         * Return length
         * @param p  input position
         * @param q  position on helix.
         * @param m  mapped position
         */
        Double_t ExtrapolateByMap(TVector3 p, TVector3 &q, TVector3 &m) const;

        /**
         * Check continuity of the track. Hit array must be filled.
         * Returns ratio of the continuous region. (-1 if less than 2 hits)
         *
         * If difference between "length from hits" is smaller than 20,
         * (> pad-diagonal, < 2*pad-row) the region is considered to be connected.
         *
         * The total length calculation is different from TrackLength(),
         * because TrackLength() use only the head/tail alpha angle informatiotn
         * but this method calculate length directly from the hit using
         * Map(...) method. 
         *
         * @param continuousLength  calculated length of continuos region.
         * @param totalLength       calculated total length of the track.
         */
        virtual double Continuity(double &totalLength, double &continuousLength, double distCut=25);
        //Double_t Continuity(Double_t &totalLength, Double_t &continuousLength);

        virtual TVector3 Momentum(Double_t B = 0.5) const;       ///< Momentum of track (B = 0.5 by default)
        virtual TVector3 PositionAtHead() const;                 ///< Position at head of helix
        virtual TVector3 PositionAtTail() const;                 ///< Position at tail of helix
        virtual Double_t TrackLength() const;                    ///< Length of track calculated from head to tail.
        virtual TVector3 ExtrapolateTo(TVector3 point) const;    ///< Extrapolate to POCA from point, returns extrapolated position
        virtual TVector3 ExtrapolateHead(Double_t length) const; ///< Extrapolate head of track about length, returns extrapolated position
        virtual TVector3 ExtrapolateTail(Double_t length) const; ///< Extrapolate tail of track about length, returns extrapolated position
        virtual TVector3 ExtrapolateByRatio(Double_t r) const;   ///< Extrapolate by ratio (tail:0, head:1), returns extrapolated position
        virtual TVector3 ExtrapolateByLength(Double_t l) const;  ///< Extrapolate by length (tail:0), returns extrapolated position
        virtual Double_t LengthAt(TVector3 point) const;         ///< Length at POCA from point, where tail=0, head=TrackLength


        TGraph *CrossSectionOnPlane(axis_t axis1, axis_t axis2, Double_t scale=1);

        ClassDef(LKHelixTrack, 1)
};

class LKHitSortByIncreasingLength {
    public:
        LKHitSortByIncreasingLength(LKHelixTrack *track):fTrk(track) {}
        bool operator() (LKHit* a, LKHit* b) { return fTrk->Map(a->GetPosition()).Z() < fTrk->Map(b->GetPosition()).Z(); }
    private:
        LKHelixTrack *fTrk;
};

class LKHitSortByDecreasingLength {
    public:
        LKHitSortByDecreasingLength(LKHelixTrack *track):fTrk(track) {}
        bool operator() (LKHit* a, LKHit* b) { return fTrk->Map(a->GetPosition()).Z() > fTrk->Map(b->GetPosition()).Z(); }
    private:
        LKHelixTrack *fTrk;
};

#endif
