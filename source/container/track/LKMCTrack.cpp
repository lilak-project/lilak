#include "LKMCTrack.h"
#include "TVector3.h"

#ifdef ACTIVATE_EVE
#include "TDatabasePDG.h"
#include "TEveLine.h"
#endif

#include <iostream>
#include <iomanip>
using namespace std;

ClassImp(LKMCTrack)

LKMCTrack::LKMCTrack()
{
    Clear();
}

void LKMCTrack::Clear(Option_t *option)
{
    LKTracklet::Clear(option);

    fStatusID.clear();
    fVolumeID.clear();
    fPX.clear();
    fPY.clear();
    fPZ.clear();
    fVX.clear();
    fVY.clear();
    fVZ.clear();
    fEnergy.clear();

    fStatusID.push_back(-999);
    fVolumeID.push_back(-999);
    fPX.push_back(-999);
    fPY.push_back(-999);
    fPZ.push_back(-999);
    fVX.push_back(-999);
    fVY.push_back(-999);
    fVZ.push_back(-999);
    fEnergy.push_back(-999);
}

void LKMCTrack::Print(Option_t *option) const
{
    TString opts = TString(option);

    e_info << "MC-" << fTrackID << "(" << fParentID << ") " << fPDG << "[" << fStatusID[0] << "]" << endl;
    Int_t n = fVolumeID.size();
    for (auto i=0; i<n; ++i) {
        e_info << "  " << i
            <<  "> vol="  << fVolumeID[i]
            <<  ", sta="  << fStatusID[i]
            <<  ", pos=(" << fVX[i] << "," << fVY[i] << "," << fVZ[i] << ")"
            <<  ", mom=(" << fPX[i] << "," << fPY[i] << "," << fPZ[i] << ")"
            <<  ", ene=" << fEnergy[i] << endl;
    }

    /*
    if (TString(option).Index("all")>=0) {
        e_info << "MC-" << setw(3) << fTrackID << "(" << setw(3) << fParentID << ") " << setw(11) << fPDG << "[" << setw(2) << fStatusID << "]" << endl;
        Int_t n = fPX.size();
        for (auto i=0; i<n; ++i) {
            e_info << "  " << i
                <<  "> mom=(" << setw(12) << fPX[i] << "," << setw(12) << fPY[i] << "," << setw(12) << fPZ[i] << "),"
                <<   " det="  << setw(12) << fVolumeID[i] << ","
                <<   " pos=(" << setw(12) << fVX[i] << "," << setw(12) << fVY[i] << "," << setw(12) << fVZ[i] << ")" << endl;
        }
    }
    else {
        e_info << "MC-" << setw(3) << fTrackID << "(" << setw(3) << fParentID << ") " << setw(11) << fPDG
            << "[" << setw(2) << fStatusID << "]"
            << " mom=(" << setw(12) << fPX[0] << "," << setw(12) << fPY[0] << "," << setw(12) << fPZ[0] << "),"
            << " det="  << setw(12) << fVolumeID[0] << ","
            << " pos=(" << setw(12) << fVX[0] << "," << setw(12) << fVY[0] << "," << setw(12) << fVZ[0] << ")" << endl;
    }
    */
}

void LKMCTrack::SetPX(Double_t val)     { fPX[0] = val; }
void LKMCTrack::SetPY(Double_t val)     { fPY[0] = val; }
void LKMCTrack::SetPZ(Double_t val)     { fPZ[0] = val; }
void LKMCTrack::SetVX(Double_t val)     { fVX[0] = val; }
void LKMCTrack::SetVY(Double_t val)     { fVY[0] = val; }
void LKMCTrack::SetVZ(Double_t val)     { fVZ[0] = val; }
void LKMCTrack::SetEnergy(Double_t val) { fEnergy[0] = val; }
void LKMCTrack::SetVolumeID(Int_t id)   { fVolumeID[0] = id; }
void LKMCTrack::SetStatusID(Int_t id)   { fStatusID[0] = id; }
void LKMCTrack::SetDetectorID(Int_t id) { SetVolumeID(id); }

void LKMCTrack::SetMCTrack(Int_t trackID, Int_t parentID, Int_t pdg, Int_t volumeID, Int_t processID, Double_t vx, Double_t vy, Double_t vz, Double_t px, Double_t py, Double_t pz, Double_t energy)
{
    fTrackID = trackID;
    fParentID = parentID;
    fPDG = pdg;
    fPX[0] = px;
    fPY[0] = py;
    fPZ[0] = pz;
    fVX[0] = vx;
    fVY[0] = vy;
    fVZ[0] = vz;
    fEnergy[0] = energy;
    fVolumeID[0] = volumeID;
    fStatusID[0] = processID;
}

void LKMCTrack::AddVertex(Int_t volumeID, Int_t processID, Double_t vx, Double_t vy, Double_t vz, Double_t px, Double_t py, Double_t pz, Double_t energy)
{
    fPX.push_back(px);
    fPY.push_back(py);
    fPZ.push_back(pz);
    fVX.push_back(vx);
    fVY.push_back(vy);
    fVZ.push_back(vz);
    fEnergy.push_back(energy);
    fVolumeID.push_back(volumeID);
    fStatusID.push_back(processID);
}

Int_t LKMCTrack::GetNumVertices() const { return (Int_t) fPX.size(); }

Int_t LKMCTrack::GetStatusID(Int_t idx) const { return fStatusID[idx]; }
Int_t LKMCTrack::GetDetectorID(Int_t idx) const { return fVolumeID[idx]; }
Int_t LKMCTrack::GetVolumeID(Int_t idx) const { return GetDetectorID(idx); }

Double_t LKMCTrack::GetPX(Int_t idx) const { return fPX[idx]; }
Double_t LKMCTrack::GetPY(Int_t idx) const { return fPY[idx]; }
Double_t LKMCTrack::GetPZ(Int_t idx) const { return fPZ[idx]; }
TVector3 LKMCTrack::GetMomentum(Int_t idx) const { return TVector3(fPX[idx], fPY[idx], fPZ[idx]); }

Double_t LKMCTrack::GetVX(Int_t idx) const { return fVX[idx]; }
Double_t LKMCTrack::GetVY(Int_t idx) const { return fVY[idx]; }
Double_t LKMCTrack::GetVZ(Int_t idx) const { return fVZ[idx]; }
TVector3 LKMCTrack::GetVertex(Int_t idx) const { return TVector3(fVX[idx], fVY[idx], fVZ[idx]); }
Double_t LKMCTrack::GetEnergy(Int_t idx) const { return fEnergy[idx]; }

TVector3 LKMCTrack::GetPrimaryPosition() const { return TVector3(fVX[0], fVY[0], fVZ[0]); }
Int_t LKMCTrack::GetPrimaryVolumeID() const { return fVolumeID[0]; }
Int_t LKMCTrack::GetPrimaryDetectorID() const { return GetPrimaryVolumeID(); }

void LKMCTrack::AddStep(LKMCStep *hit) { fStepArray.push_back(hit); }
vector<LKMCStep *> *LKMCTrack::GetStepArray() { return &fStepArray; }

TVector3 LKMCTrack::Momentum(Double_t) const { return GetMomentum(); }
TVector3 LKMCTrack::PositionAtHead() const { return GetPrimaryPosition() + GetMomentum(); }
TVector3 LKMCTrack::PositionAtTail() const { return GetPrimaryPosition(); }
Double_t LKMCTrack::TrackLength() const
{
    Int_t nPairs = fVX.size() - 1;
    if (nPairs < 2)
        return 0.;

    Double_t length = 0.;

    for (auto iPair = 0; iPair < nPairs; ++iPair) {
        TVector3 a(fVX[iPair], fVY[iPair], fVZ[iPair]);
        TVector3 b(fVX[iPair+1], fVY[iPair+1], fVZ[iPair+1]);
        length += (b-a).Mag();
    }

    return length;
}

TVector3 LKMCTrack::ExtrapolateTo(TVector3)       const { return TVector3(); } //@todo
TVector3 LKMCTrack::ExtrapolateHead(Double_t)     const { return TVector3(); } //@todo
TVector3 LKMCTrack::ExtrapolateTail(Double_t)     const { return TVector3(); } //@todo
TVector3 LKMCTrack::ExtrapolateByRatio(Double_t)  const { return TVector3(); } //@todo
TVector3 LKMCTrack::ExtrapolateByLength(Double_t) const { return TVector3(); } //@todo
Double_t LKMCTrack::LengthAt(TVector3)            const { return 0; }          //@todo

#ifdef ACTIVATE_EVE
bool LKMCTrack::DrawByDefault() { return true; }
bool LKMCTrack::IsEveSet() { return false; }

TEveElement *LKMCTrack::CreateEveElement()
{
    auto element = new TEveLine();

    return element;
}

void LKMCTrack::SetEveElement(TEveElement *element, Double_t scale)
{
    auto line = (TEveLine *) element;
    line -> Reset();

    auto particle = TDatabasePDG::Instance() -> GetParticle(fPDG);

    TString pName;
    TString pClass;
    Int_t pCharge = 0;

    if (particle == nullptr)
        pName = Form("%d",fPDG);
    else {
        pName = particle -> GetName();
        pClass = particle -> ParticleClass();
        pCharge = particle -> Charge();
    }

    Color_t color = kGray+2;
    Color_t width = 1;

    if (pClass == "Lepton") {
        if (pCharge < 0) color = kOrange-3;
        else if (pCharge > 0) color = kAzure+7;
        else                  color = kGray+1;
    }
    else if (pClass == "Meson") {
        if (pCharge < 0) color = kPink-1;
        else if (pCharge > 0) color = kCyan+1;
        else                  color = kGray+1;
    }
    else if (pClass == "Baryon") {
        if (pCharge < 0) color = kRed-4;
        else if (pCharge > 0) color = kBlue-4;
        else                  color = kOrange;
    }
    else if (pClass == "Ion") {
        width = 2;
        if (pCharge < 0) color = kRed;
        else if (pCharge > 0) color = kBlue;
        else                  color = kGray+1;
    }
    else if (pClass == "GaugeBoson")
        color = kGreen;

    line -> SetLineColor(color);
    line -> SetLineWidth(width);

    TString eveName = Form("mc_%s:%d(%d)[%.1f]{%d;%d}",pName.Data(),fTrackID,fParentID,Momentum().Mag(),fVolumeID.at(0),GetNumVertices());
    line -> SetElementName(eveName);

    if (fParentID != 0)
        line -> SetLineStyle(2);

    Int_t numVertices = fPX.size();
    if (numVertices==1) {
        TVector3 pos0 = scale*GetPrimaryPosition();
        TVector3 pos1 = scale*pos0 + scale*GetMomentum().Unit();

        line -> SetNextPoint(pos0.X(), pos0.Y(), pos0.Z());
        line -> SetNextPoint(pos1.X(), pos1.Y(), pos1.Z());
    }
    else {
        for (auto idx = 0; idx < numVertices; ++idx)
            line -> SetNextPoint(scale*fVX[idx], scale*fVY[idx], scale*fVZ[idx]);
    }
}

void LKMCTrack::AddToEveSet(TEveElement *, Double_t)
{
}
#endif

TGraphErrors *LKMCTrack::TrajectoryOnPlane(axis_t axis1, axis_t axis2, bool (*fisout)(TVector3 pos), Double_t scale)
{
    if (fTrajectoryOnPlane == nullptr)
    {
        fTrajectoryOnPlane = new TGraphErrors();
        fTrajectoryOnPlane -> SetLineColor(kRed);

        TString pClass;
        Int_t pCharge = 0;

#ifdef ACTIVATE_EVE
        auto particle = TDatabasePDG::Instance() -> GetParticle(fPDG);

        if (particle == nullptr) {}
        else {
            pClass = particle -> ParticleClass();
            pCharge = particle -> Charge();
        }
#endif

        Color_t color = kGray+2;
        Color_t width = 1;

        if (pClass == "Lepton") {
            if (pCharge < 0) color = kOrange-3;
            else if (pCharge > 0) color = kAzure+7;
            else                  color = kGray+1;
        }
        else if (pClass == "Meson") {
            if (pCharge < 0) color = kPink-1;
            else if (pCharge > 0) color = kCyan+1;
            else                  color = kGray+1;
        }
        else if (pClass == "Baryon") {
            if (pCharge < 0) color = kRed-4;
            else if (pCharge > 0) color = kBlue-4;
            else                  color = kOrange;
        }
        else if (pClass == "Ion") {
            width = 2;
            if (pCharge < 0) color = kRed;
            else if (pCharge > 0) color = kBlue;
            else                  color = kGray+1;
        }
        else if (pClass == "GaugeBoson")
            color = kGreen;

        fTrajectoryOnPlane  -> SetLineColor(color);
        fTrajectoryOnPlane  -> SetLineWidth(width);
    }

    fTrajectoryOnPlane -> Set(0);

    if (fParentID != 0)
        fTrajectoryOnPlane  -> SetLineStyle(2);

    Int_t numVertices = fPX.size();
    if (numVertices==1) {
        auto pos0 = scale * LKVector3(GetPrimaryPosition());
        auto pos1 = scale * (pos0 + LKVector3(GetMomentum().Unit()));

        fTrajectoryOnPlane -> SetPoint(fTrajectoryOnPlane -> GetN(), pos0.At(axis1), pos0.At(axis2));
        fTrajectoryOnPlane -> SetPoint(fTrajectoryOnPlane -> GetN(), pos1.At(axis1), pos1.At(axis2));
    }
    else {
        bool isout;
        for (auto idx = 0; idx < numVertices; ++idx) {
            auto pos = scale * LKVector3(fVX[idx], fVY[idx], fVZ[idx]);
            isout = fisout(pos);
            if (isout)
                break;

            fTrajectoryOnPlane -> SetPoint(fTrajectoryOnPlane -> GetN(), pos.At(axis1), pos.At(axis2));
        }
    }

    return fTrajectoryOnPlane;
}
