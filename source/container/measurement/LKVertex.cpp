#include "LKVertex.h"
#ifdef ACTIVATE_EVE
#include "TEvePointSet.h"
#endif

ClassImp(LKVertex)

LKVertex::LKVertex()
{
    Clear();
}

void LKVertex::Clear(Option_t *option)
{
    LKHit::Clear(option);

    fTrackArray.clear();
    fTrackIDArray.clear();
}

void LKVertex::Print(Option_t *option) const
{
    TString opts = TString(option);

    if (opts.Index("s")>=0)
        e_info << "Vertex at (" << fX << "," << fY << "," << fZ
            << ") [mm] containing " << GetNumTracks() << "tracks" << endl;
    else //if (opts.Index("a")>=0)
        e_info << "Vertex at (" << setw(12) << fX <<"," << setw(12) << fY <<"," << setw(12) << fZ
            << ") [mm] containing " << GetNumTracks() << "tracks" << endl;
}

void LKVertex::Copy(TObject &obj) const
{
    LKHit::Copy(obj);

    auto vertex = (LKVertex &) obj;

    for (auto track : fTrackArray)
        vertex.AddTrack(track);
}

void LKVertex::AddTrack(LKTracklet* track)
{
    fTrackArray.push_back(track);
    fTrackIDArray.push_back(track->GetTrackID());
}

#ifdef ACTIVATE_EVE
bool LKVertex::DrawByDefault() { return true; }

bool LKVertex::IsEveSet() { return true; }

TEveElement *LKVertex::CreateEveElement() {
    auto pointSet = new TEvePointSet("Vertex");
    pointSet -> SetMarkerColor(kBlack);
    pointSet -> SetMarkerSize(2.5);
    pointSet -> SetMarkerStyle(20);
    return pointSet;
}

void LKVertex::AddToEveSet(TEveElement *eveSet, Double_t scale) {
    auto pointSet = (TEvePointSet *) eveSet;
    pointSet -> SetNextPoint(scale*fX, scale*fY, scale*fZ);
}
#endif
