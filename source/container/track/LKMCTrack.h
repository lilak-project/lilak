#ifndef LKMCTRACK_HH
#define LKMCTRACK_HH

#include "TObject.h"
#include "TVector3.h"
#include "LKMCStep.h"
#include "LKTracklet.h"
#include <vector>
using namespace std;

#ifdef ACTIVATE_EVE
#include "TEveElement.h"
#endif

typedef LKVector3::Axis axis_t;

/**
 * Eve naming concept : mc_pnm:tid(pid)[mom]{dnm;nvx}
 * - pnm: particle name or PDG encoding number
 * - tid: track id
 * - pid: parent id
 * - mom: momentum value in MeV/c
 * - dnm: detector name or detector id
 * - nvx: number of vertex
 */

class LKMCTrack : public LKTracklet
{
    public:
        LKMCTrack();
        virtual ~LKMCTrack() {}

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option="") const;

        void SetPX(Double_t val);
        void SetPY(Double_t val);
        void SetPZ(Double_t val);
        void SetVX(Double_t val);
        void SetVY(Double_t val);
        void SetVZ(Double_t val);
        void SetStatusID(Int_t id);
        void SetVolumeID(Int_t id);
        void SetDetectorID(Int_t id);
        void SetEnergy(Double_t val);

        void SetMCTrack(Int_t trackID, Int_t parentID, Int_t pdg, Int_t detectorID, Int_t processID, Double_t vx, Double_t vy, Double_t vz, Double_t px, Double_t py, Double_t pz, Double_t energy);
        void AddVertex(Int_t detectorID, Int_t processID, Double_t vx, Double_t vy, Double_t vz, Double_t px, Double_t py, Double_t pz, Double_t energy);

        Int_t GetNumVertices() const;

        Int_t GetStatusID(Int_t idx = 0) const;
        Int_t GetVolumeID(Int_t idx = 0) const;
        Int_t GetDetectorID(Int_t idx = 0) const;

        Double_t GetPX(Int_t idx = 0) const;
        Double_t GetPY(Int_t idx = 0) const;
        Double_t GetPZ(Int_t idx = 0) const;
        TVector3 GetMomentum(Int_t idx = 0) const;

        Double_t GetVX(Int_t idx = 0) const;
        Double_t GetVY(Int_t idx = 0) const;
        Double_t GetVZ(Int_t idx = 0) const;
        TVector3 GetVertex(Int_t idx = 0) const;

        Double_t GetEnergy(Int_t idx = 0) const;


        TVector3 GetPrimaryPosition() const;
        Int_t GetPrimaryVolumeID() const;
        Int_t GetPrimaryDetectorID() const;

        void AddStep(LKMCStep *hit);
        vector<LKMCStep *> *GetStepArray();

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

#ifdef ACTIVATE_EVE
        virtual bool DrawByDefault();
        virtual bool IsEveSet();
        virtual TEveElement *CreateEveElement();
        virtual void SetEveElement(TEveElement *, Double_t scale=1);
        virtual void AddToEveSet(TEveElement *, Double_t scale=1);
#endif

        //virtual TGraph *TrajectoryOnPlane(axis_t axis1, axis_t axis2, Double_t scale=1);
        virtual TGraphErrors *TrajectoryOnPlane(axis_t axis1, axis_t axis2, bool (*fisout)(TVector3 pos), Double_t scale=1);

    protected:
        vector<Int_t>    fStatusID;
        vector<Int_t>    fVolumeID; ///< detector ID (= copyNo)
        vector<Double_t> fPX;
        vector<Double_t> fPY;
        vector<Double_t> fPZ;
        vector<Double_t> fVX;
        vector<Double_t> fVY;
        vector<Double_t> fVZ;
        vector<Double_t> fEnergy;

        vector<LKMCStep *> fStepArray; //!

        ClassDef(LKMCTrack, 2)
};

#endif
