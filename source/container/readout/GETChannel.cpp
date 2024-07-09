#include "GETChannel.h"

ClassImp(GETChannel);

GETChannel::GETChannel()
{
    Clear();
}

void GETChannel::Clear(Option_t *option)
{
    LKChannel::Clear(option);
    GETParameters::ClearParameters();
    fBufferRawSig.Clear();
    fHitArray.Clear("C");
}

void GETChannel::Copy(TObject &object) const
{
    LKChannel::Copy(object);
    auto objCopy = (GETChannel &) object;
    objCopy.SetDetType(fDetType);
    objCopy.SetCobo(fCobo);
    objCopy.SetAsad(fAsad);
    objCopy.SetAget(fAget);
    objCopy.SetChan(fChan);
    objCopy.SetBuffer(fBufferRawSig);
}

void GETChannel::Print(Option_t *option) const
{
    if (TString(option).Index("!title")<0)
        e_info << "[GETChannel]" << std::endl;
    LKChannel::Print("!title");
    GETParameters::PrintParameters("!title");
    fHitArray.Print("!title");
    fBufferRawSig.Print();
}

TObject* GETChannel::Clone(const char *newname) const
{
    GETChannel *obj = (GETChannel*) LKChannel::Clone(newname);
    obj -> SetDetType(fDetType);
    obj -> SetCobo(fCobo);
    obj -> SetAsad(fAsad);
    obj -> SetAget(fAget);
    obj -> SetChan(fChan);
    obj -> SetBuffer(fBufferRawSig);
    return obj;
}

void GETChannel::Draw(Option_t *option)
{
    GetHist() -> Draw(option);
    GetHitGraph() -> Draw("samel");
}

TH1D *GETChannel::GetHist(TString name)
{
    if (name.IsNull())
        name = Form("hist_GET%d",GetCAAC());
    auto hist = fBufferRawSig.GetHist(name);
    hist -> SetTitle(MakeTitle());
    return hist;
}

void GETChannel::FillHist(TH1* hist)
{
    hist -> Reset();
    if (TString(hist->GetTitle()).IsNull()) hist -> SetTitle(MakeTitle());
    fBufferRawSig.FillHist(hist);
}

void GETChannel::FillGraph(TGraph* graph)
{
    graph -> Set(0);
    fBufferRawSig.FillGraph(graph);
}

TGraph *GETChannel::GetHitGraph()
{
    if (fGraphHit==nullptr)
        fGraphHit = new TGraph();
    fGraphHit -> Set(0);
    fGraphHit -> SetPoint(0,fTime-5,fPedestal);
    fGraphHit -> SetPoint(1,fTime  ,fPedestal);
    fGraphHit -> SetPoint(2,fTime  ,fEnergy+fPedestal);
    fGraphHit -> SetPoint(3,fTime+5,fEnergy+fPedestal);
    fGraphHit -> SetLineColor(kRed);
    fGraphHit -> SetMarkerColor(kRed);
    return fGraphHit;
}

LKHit *GETChannel::PullOutNextFreeHit()
{
    int numHits = fHitArray.GetNumHits();
    if (numHits==0)
        return nullptr;

    for (auto iHit=0; iHit<numHits; ++iHit) {
        auto hit = fHitArray.GetHit(iHit);
        if (hit->GetNumTrackCands()==0) {
            fHitArray.RemoveHit(iHit);
            return hit;
        }
    }

    return (LKHit*) nullptr;
}

void GETChannel::PullOutHits(LKHitArray *hits)
{
    int numHits = fHitArray.GetNumHits();
    if (numHits==0)
        return;

    for (auto iHit=0; iHit<numHits; ++iHit) {
        hits -> AddHit(fHitArray.GetHit(iHit));
    }
    fHitArray.Clear();
}

void GETChannel::PullOutHits(vector<LKHit *> *hits)
{
    int numHits = fHitArray.GetNumHits();
    if (numHits==0)
        return;

    for (auto iHit=0; iHit<numHits; ++iHit)
        hits -> push_back(fHitArray.GetHit(iHit));
    fHitArray.Clear();
}
