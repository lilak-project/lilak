#include "LKLogger.h"
#include "LKTpcHit.h"
#include "LKPulseGenerator.h"
#ifdef ACTIVATE_EVE
#include "TEvePointSet.h"
#endif
#include <iostream>
#include <iomanip>

ClassImp(LKTpcHit)

void LKTpcHit::Clear(Option_t *option)
{
    LKHit::Clear(option);

    fPadID = -1;
    fSection = -999;
    fRow = -999;
    fLayer = -999;
    fTb = -1;

    fTrackCandArray.clear();
}

void LKTpcHit::Print(Option_t *option) const
{
    e_info
        //<< "HTMP-ID: " << fHitID << ", " << fTrackID << ", " << fMCID << ", " << fPadID
        << "HTP-ID: " << fHitID << ", " << fTrackID << ", " << fPadID
        << "| XYZ: " << fX << ", " << fY << ", " << fZ << " |, Q: " << fW
        << "| SLR: " << fSection << ", "<< fRow << ", "<< fLayer << " | Tb: " << fTb << endl;
}

void LKTpcHit::PrintTpcHit() const
{
    e_info
        //<< "HTMP-ID: "
        << "HTP-ID: "
        << setw(4)  << fHitID
        << setw(4)  << fTrackID
        //<< setw(4)  << fMCID
        << setw(4)  << fPadID
        << "| XYZ: "
        << setw(12) << fX
        << setw(12) << fY
        << setw(12) << fZ
        << "| Q: "
        << setw(12) << fW
        << "| SRL: "
        << setw(4) << fSection
        << setw(4) << fRow
        << setw(4) << fLayer
        << "| Tb: "
        << setw(4) << fTb << endl;
}

void LKTpcHit::CopyFrom(LKTpcHit const *hit)
{
    //fMCError   = hit -> GetMCError  ();
    //fMCID      = hit -> GetMCID     ();
    //fMCPurity  = hit -> GetMCPurity ();
    fX         = hit -> GetX        ();
    fY         = hit -> GetY        ();
    fZ         = hit -> GetZ        ();
    fW         = hit -> GetCharge   ();
    fAlpha     = hit -> GetAlpha    ();
    fHitID     = hit -> GetHitID    ();
    fSortValue = hit -> GetSortValue();
    fTrackID   = hit -> GetTrackID  ();
    fPadID     = hit -> GetPadID    ();
    fSection   = hit -> GetSection  ();
    fRow       = hit -> GetRow      ();
    fLayer     = hit -> GetLayer    ();
    fTb        = hit -> GetTb       ();
}

void LKTpcHit::Copy(TObject &obj) const
{
    //LKHit::Copy(obj);
    auto tpchit = (LKTpcHit &) obj;

    //tpchit.fMCError   = fMCError  ;
    //tpchit.fMCID      = fMCID     ;
    //tpchit.fMCPurity  = fMCPurity ;
    tpchit.fX         = fX        ;
    tpchit.fY         = fY        ;
    tpchit.fZ         = fZ        ;
    tpchit.fW         = fW        ;
    tpchit.fAlpha     = fAlpha    ;
    tpchit.fHitID     = fHitID    ;
    tpchit.fSortValue = fSortValue;
    tpchit.fTrackID   = fTrackID  ;
    tpchit.fPadID     = fPadID    ;
    tpchit.fSection   = fSection  ;
    tpchit.fRow       = fRow      ;
    tpchit.fLayer     = fLayer    ;
    tpchit.fTb        = fTb       ;
}

void LKTpcHit::AddHit(LKTpcHit *hit)
{
    auto w0 = fW;
    LKHit::AddHit((LKHit *) hit);
    fTb = (w0*fTb + hit->GetCharge()*hit->GetTb()) / (w0+hit->GetCharge());
}

TF1 *LKTpcHit::GetPulseFunction(Option_t *)
{
    auto pulseGen = LKPulseGenerator::GetPulseGenerator();
    auto f1 = pulseGen -> GetPulseFunction("pulse");
    f1 -> SetParameters(fW, fTb);
    return f1;
}

void LKTpcHit::SetPadID(Int_t id) { fPadID = id; }
void LKTpcHit::SetSection(Int_t section) { fSection = section; }
void LKTpcHit::SetRow(Int_t row) { fRow = row; }
void LKTpcHit::SetLayer(Int_t layer) { fLayer = layer; }
void LKTpcHit::SetTb(Double_t tb) { fTb = tb; }

Int_t LKTpcHit::GetPadID() const { return fPadID; }
Int_t LKTpcHit::GetSection() const { return fSection; }
Int_t LKTpcHit::GetRow() const { return fRow; }
Int_t LKTpcHit::GetLayer() const { return fLayer; }
Double_t LKTpcHit::GetTb() const { return fTb; }

void LKTpcHit::SetSortByLayer(bool sortEarlierIfSmaller)
{
    if (sortEarlierIfSmaller) fSortValue =  fLayer;
    else                      fSortValue = -fLayer;
}

#ifdef ACTIVATE_EVE
TEveElement *LKTpcHit::CreateEveElement()
{
    auto pointSet = new TEvePointSet("TpcHit");
    pointSet -> SetMarkerColor(kAzure-8);
    pointSet -> SetMarkerSize(0.5);
    pointSet -> SetMarkerStyle(20);

    return pointSet;
}
#endif
