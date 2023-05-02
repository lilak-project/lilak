#include "TH2D.h"
#include "TMarker.h"

#include "LKLogger.hpp"
#include "LKHitArray.hpp"
#include "LKHit.hpp"
//#include "LKTpcHit.hpp"
#include "LKGeoSphere.hpp"
#include "LKGeoLine.hpp"

ClassImp(LKHitArray);

LKHitArray::LKHitArray(Int_t size)
    :TObjArray()
{
    Clear();
}

LKHitArray::~LKHitArray()
{
}

LKHit *LKHitArray::GetHit(Int_t idx) const
{
    return (LKHit *) At(idx);
}

LKHit *LKHitArray::GetLastHit() const
{
    return (LKHit *) At(GetEntriesFast()-1);
}

void LKHitArray::SortByX(bool sortEarlierIfSmaller)
{
    TIter next(this);
    LKHit *hit;
    while ((hit = (LKHit *) next()))
        hit -> SetSortByX(sortEarlierIfSmaller);
    Sort();
}

void LKHitArray::SortByY(bool sortEarlierIfSmaller)
{
    TIter next(this);
    LKHit *hit;
    while ((hit = (LKHit *) next()))
        hit -> SetSortByY(sortEarlierIfSmaller);
    Sort();
}

void LKHitArray::SortByZ(bool sortEarlierIfSmaller)
{
    TIter next(this);
    LKHit *hit;
    while ((hit = (LKHit *) next()))
        hit -> SetSortByZ(sortEarlierIfSmaller);
    Sort();
}

void LKHitArray::SortByCharge(bool sortEarlierIfSmaller)
{
    TIter next(this);
    LKHit *hit;
    while ((hit = (LKHit *) next()))
        hit -> SetSortByCharge(sortEarlierIfSmaller);
    Sort();
}

void LKHitArray::SortByDistanceTo(TVector3 point, bool sortEarlierIfCloser)
{
    TIter next(this);
    LKHit *hit;
    while ((hit = (LKHit *) next()))
        hit -> SetSortByDistanceTo(point, sortEarlierIfCloser);
    Sort();
}

/*
void LKHitArray::SortByLayer(bool sortEarlierIfSmaller)
{
    TIter next(this);
    LKTpcHit *hit;
    while ((hit = (LKTpcHit *) next()))
        hit -> SetSortByLayer(sortEarlierIfSmaller);
    Sort();
}
*/


void LKHitArray::Clear(Option_t *option)
{
    TObjArray::Clear();

    fN = 0;
    fW = 0;

    fEX = 0;
    fEY = 0;
    fEZ = 0;
    fEXX = 0;
    fEYY = 0;
    fEZZ = 0;
    fEXY = 0;
    fEYZ = 0;
    fEZX = 0;

    if (fODRFitter != nullptr)
        fODRFitter -> Reset();
}

void LKHitArray::Print(Option_t *option) const
{
    TString opts = TString(option);

    lx_info << "Number of hits: " << fN << endl;
    lx_info << "Charge sum: " << fW << endl;
    lx_info << "<x>=" << fEX << ", <y>=" << fEY << ", <z>=" << fEZ << endl;
    lx_info << "s_x=" << GetVarianceX() << ", s_y=" << GetVarianceY() << ", s_z=" << GetVarianceZ() << endl;
    lx_info << "Mat. A=|" << setw(15) << fEXX << setw(15) << fEXY << setw(15) << fEZX << "|" << endl;
    lx_info << "       |" << setw(15) << fEXY << setw(15) << fEYY << setw(15) << fEYZ << "|" << endl;
    lx_info << "       |" << setw(15) << fEZX << setw(15) << fEYZ << setw(15) << fEZZ << "|" << endl;

    if (opts.Index(">")>=0) {
        TIter next(this);
        LKHit *hit;
        while ((hit = (LKHit *) next()))
            lx_info << hit -> GetHitID() << " ";
        lx_cout << endl;
    }
}

void LKHitArray::Copy(TObject &obj) const
{
    TObject::Copy(obj);
    auto list = (LKHitArray &) obj;
    list.Clear("C");

    TIter next(this);
    LKHit *hit;
    while ((hit = (LKHit *) next()))
        list.AddHit(hit);
}

void LKHitArray::MoveHitsTo(LKHitArray *hitArray)
{
    TIter next(this);
    LKHit *hit;
    while ((hit = (LKHit *) next()))
        hitArray -> AddHit(hit);
    Clear();
}

void LKHitArray::PrintHitIDs(Int_t rank) const
{
    if (fN == 0)
        return;

    TIter next(this);
    LKHit *hit;
    while ((hit = (LKHit *) next()))
        lx_info << hit -> GetHitID() << " ";
    lx_cout << endl;
}

void LKHitArray::PrintHits(Int_t rank) const
{
    TIter next(this);
    LKHit *hit;
    while ((hit = (LKHit *) next()))
        //hit -> Print(Form("%dts",rank)); TODO
        hit -> Print("ts");
}


LKGeoLine LKHitArray::FitLine()
{
    if (fODRFitter == nullptr)
        fODRFitter = LKODRFitter::GetFitter();

    LKGeoLine line;
    line.SetRMS(-1);

    if (fN < 4)
        return line;

    fODRFitter -> Reset();
    fODRFitter -> SetCentroid(fEX,fEY,fEZ);
    fODRFitter -> SetMatrixA(GetAXX(),GetAXY(),GetAZX(),GetAYY(),GetAYZ(),GetAZZ());
    fODRFitter -> SetWeightSum(fW);
    fODRFitter -> SetNumPoints(fN);
    if (fODRFitter -> Solve() == false)
        return line;

    fODRFitter -> ChooseEigenValue(0);
    line.SetLine(TVector3(fEX,fEY,fEZ), TVector3(fEX,fEY,fEZ)+fODRFitter->GetDirection());
    line.SetRMS(fODRFitter -> GetRMSLine());

    return line;
}

LKGeoPlane LKHitArray::FitPlane()
{
    if (fODRFitter == nullptr)
        fODRFitter = LKODRFitter::GetFitter();

    LKGeoPlane plane;
    plane.SetRMS(-1);

    if (fN < 4)
        return plane;

    fODRFitter -> Reset();
    fODRFitter -> SetCentroid(fEX,fEY,fEZ);
    fODRFitter -> SetMatrixA(GetAXX(),GetAXY(),GetAZX(),GetAYY(),GetAYZ(),GetAZZ());
    fODRFitter -> SetWeightSum(fW);
    fODRFitter -> SetNumPoints(fN);

    //TIter next(this);
    //LKHit *hit;
    //while ((hit = (LKHit *) next()))
    //  fODRFitter -> AddPoint(hit->GetX(), hit->GetY(), hit->GetZ(), hit->GetCharge());

    if (fODRFitter -> Solve() == false)
        return plane;

    fODRFitter -> ChooseEigenValue(2); //TODO
    plane.SetPlane(TVector3(fEX,fEY,fEZ),fODRFitter -> GetNormal());
    plane.SetRMS(fODRFitter -> GetRMSPlane());

    return plane;
}

LKGeoCircle LKHitArray::FitCircle(axis_t ref)
{
    if (fODRFitter == nullptr)
        fODRFitter = LKODRFitter::GetFitter();
    fODRFitter -> Reset();

    LKGeoCircle fitCircle;
    fitCircle.SetRMS(-1);

    if (fN < 4)
        return fitCircle;

    auto mean = GetMean(ref);
    auto variance = GetVariance(ref);

    auto rRiemann = 2 * sqrt(variance.I() + variance.J()); // Riemann sphere radius
    auto posRiemann    = LKVector3(ref,mean.I(),mean.J(),rRiemann);
    auto posRiemannTop = LKVector3(ref,mean.I(),mean.J(),2*rRiemann);
    auto posRiemannBot = LKVector3(ref,mean.I(),mean.J(),0);

    Double_t iProjectionMean = 0;
    Double_t jProjectionMean = 0;
    Double_t kProjectionMean = 0;

    TIter nextHit(this);
    LKHit *hit;
    while ((hit = (LKHit *) nextHit()))
    {
        LKVector3 pos(hit -> GetPosition(), ref);
        auto i0 = pos.I() - posRiemann.I();
        auto j0 = pos.J() - posRiemann.J();
        Double_t w = hit -> GetCharge();

        Double_t rEff = sqrt(i0*i0 + j0*j0) / (2*rRiemann);
        Double_t denominator = 1 + rEff*rEff;
        Double_t iProjection = i0 / denominator;
        Double_t jProjection = j0 / denominator;
        Double_t kProjection = 2 * rRiemann * rEff * rEff / denominator;

        iProjectionMean += w * iProjection;
        jProjectionMean += w * jProjection;
        kProjectionMean += w * kProjection;
    }

    iProjectionMean = iProjectionMean / fW;
    jProjectionMean = jProjectionMean / fW;
    kProjectionMean = kProjectionMean / fW;

    fODRFitter -> SetCentroid(iProjectionMean, jProjectionMean, kProjectionMean);
    auto meanProjection = LKVector3(ref, iProjectionMean, jProjectionMean, kProjectionMean);

    nextHit.Begin();
    while ((hit = (LKHit *) nextHit()))
    {
        LKVector3 pos(hit -> GetPosition(), ref);
        auto i0 = pos.I() - posRiemann.I();
        auto j0 = pos.J() - posRiemann.J();
        Double_t w = hit -> GetCharge();

        Double_t rEff = sqrt(i0*i0 + j0*j0) / (2*rRiemann);
        Double_t denominator = 1 + rEff*rEff;
        Double_t iProjection = i0 / denominator;
        Double_t jProjection = j0 / denominator;
        Double_t kProjection = 2 * rRiemann * rEff * rEff / denominator;

        fODRFitter -> AddPoint(iProjection, jProjection, kProjection, w);
    }

    if (fODRFitter -> Solve() == false)
        return fitCircle;

    fODRFitter -> ChooseEigenValue(0); LKVector3 vecL(ref, fODRFitter -> GetDirection());
    fODRFitter -> ChooseEigenValue(1); LKVector3 vecM(ref, fODRFitter -> GetDirection());
    fODRFitter -> ChooseEigenValue(2); LKVector3 vecN(ref, fODRFitter -> GetDirection());

    if (vecN.K()<0)
        vecN = -1*vecN;

    auto kkk = meanProjection.Dot(vecN);
    auto distToPlane = abs(vecN.Dot(LKVector3(ref,0,0,rRiemann)) - kkk);
    if (vecN.K()*rRiemann > kkk) //rRiemann is higher
        distToPlane = -distToPlane;

    LKVector3 vecU = 1./sqrt(vecL.K()*vecL.K() + vecM.K()*vecM.K()) * (vecL.K()*vecL + vecM.K()*vecM);
    LKVector3 vecV = 1./sqrt(vecU.I()*vecU.I() + vecU.J()*vecU.J()) * LKVector3(ref, vecU.I(), vecU.J(), 0);

    auto wp = distToPlane*vecN + sqrt(rRiemann*rRiemann - distToPlane*distToPlane)*vecU;
    auto wm = distToPlane*vecN - sqrt(rRiemann*rRiemann - distToPlane*distToPlane)*vecU;
    auto vp = wp.Dot(vecV);
    auto vm = wm.Dot(vecV);
    auto tangentofP = TMath::Tan(TMath::ATan2(abs(vp),-wp.K())/2);
    auto tangentM = TMath::Tan(TMath::ATan2(abs(vm),-wm.K())/2);

    auto rHelix = rRiemann * (tangentofP + tangentM);
    auto centerOffset = rRiemann * (tangentofP - tangentM);

    auto centerHelix = centerOffset * vecV + posRiemannBot;
    fitCircle.SetCircle(centerHelix.I(), centerHelix.J(), rHelix);

    fitCircle.SetRMS(1);

    return fitCircle;
}

TCanvas *LKHitArray::DrawFitCircle(axis_t ref)
{
    auto cvs0 = new TCanvas("side t","Riemann circle fit side-t",600,600);
    auto cvs1 = new TCanvas("side v","Riemann circle fit side-v",600,600);
    auto cvs2 = new TCanvas("top",   "Riemann circle fit top",   600,600);

    if (fODRFitter == nullptr)
        fODRFitter = LKODRFitter::GetFitter();
    fODRFitter -> Reset();

    LKGeoCircle fitCircle;
    fitCircle.SetRMS(-1);

    if (fN < 4)
        return cvs0;

    auto mean = GetMean(ref);
    auto variance = GetVariance(ref);

    auto rRiemann = 2 * sqrt(variance.I() + variance.J()); // Riemann sphere radius
    auto posRiemann    = LKVector3(ref,mean.I(),mean.J(),rRiemann);
    auto posRiemannTop = LKVector3(ref,mean.I(),mean.J(),2*rRiemann);
    auto posRiemannBot = LKVector3(ref,mean.I(),mean.J(),0);

    Double_t iProjectionMean = 0;
    Double_t jProjectionMean = 0;
    Double_t kProjectionMean = 0;

    TIter nextHit(this);
    LKHit *hit;
    while ((hit = (LKHit *) nextHit()))
    {
        LKVector3 pos(hit -> GetPosition(), ref);
        auto i0 = pos.I() - posRiemann.I();
        auto j0 = pos.J() - posRiemann.J();
        Double_t w = hit -> GetCharge();

        Double_t rEff = sqrt(i0*i0 + j0*j0) / (2*rRiemann);
        Double_t denominator = 1 + rEff*rEff;
        Double_t iProjection = i0 / denominator;
        Double_t jProjection = j0 / denominator;
        Double_t kProjection = 2 * rRiemann * rEff * rEff / denominator;

        iProjectionMean += w * iProjection;
        jProjectionMean += w * jProjection;
        kProjectionMean += w * kProjection;
    }

    iProjectionMean = iProjectionMean / fW;
    jProjectionMean = jProjectionMean / fW;
    kProjectionMean = kProjectionMean / fW;

    fODRFitter -> SetCentroid(iProjectionMean, jProjectionMean, kProjectionMean);
    auto meanProjection = LKVector3(ref, iProjectionMean, jProjectionMean, kProjectionMean);

    nextHit.Begin();
    while ((hit = (LKHit *) nextHit()))
    {
        LKVector3 pos(hit -> GetPosition(), ref);
        auto i0 = pos.I() - posRiemann.I();
        auto j0 = pos.J() - posRiemann.J();
        Double_t w = hit -> GetCharge();

        Double_t rEff = sqrt(i0*i0 + j0*j0) / (2*rRiemann);
        Double_t denominator = 1 + rEff*rEff;
        Double_t iProjection = i0 / denominator;
        Double_t jProjection = j0 / denominator;
        Double_t kProjection = 2 * rRiemann * rEff * rEff / denominator;

        fODRFitter -> AddPoint(iProjection, jProjection, kProjection, w);
    }

    if (fODRFitter -> Solve() == false)
        return cvs0;

    fODRFitter -> ChooseEigenValue(0); LKVector3 vecL(ref, fODRFitter -> GetDirection());
    fODRFitter -> ChooseEigenValue(1); LKVector3 vecM(ref, fODRFitter -> GetDirection());
    fODRFitter -> ChooseEigenValue(2); LKVector3 vecN(ref, fODRFitter -> GetDirection());

    if (vecN.K()<0)
        vecN = -1*vecN;

    auto kkk = meanProjection.Dot(vecN);
    auto distToPlane = abs(vecN.Dot(LKVector3(ref,0,0,rRiemann)) - kkk);
    if (vecN.K()*rRiemann > kkk) //rRiemann is higher
        distToPlane = -distToPlane;

    LKVector3 vecU = 1./sqrt(vecL.K()*vecL.K() + vecM.K()*vecM.K()) * (vecL.K()*vecL + vecM.K()*vecM);
    LKVector3 vecV = 1./sqrt(vecU.I()*vecU.I() + vecU.J()*vecU.J()) * LKVector3(ref, vecU.I(), vecU.J(), 0);

    auto wp = distToPlane*vecN + sqrt(rRiemann*rRiemann - distToPlane*distToPlane)*vecU; // vec(w)+
    auto wm = distToPlane*vecN - sqrt(rRiemann*rRiemann - distToPlane*distToPlane)*vecU; // vec(w)-
    auto vp = wp.Dot(vecV); // vec(v)+
    auto vm = wm.Dot(vecV); // vec(v)-
    auto tangentP = TMath::Tan(TMath::ATan2(abs(vp),-wp.K())/2); // t+
    auto tangentM = TMath::Tan(TMath::ATan2(abs(vm),-wm.K())/2); // t-
    auto rHelix = rRiemann * (tangentP + tangentM); // r+
    auto centerOffset = rRiemann * (tangentP - tangentM); // r-

    auto centerHelix = centerOffset * vecV + posRiemannBot;
    fitCircle.SetCircle(centerHelix.I(), centerHelix.J(), rHelix);

    fitCircle.SetRMS(1);

    //return fitCircle;
    //XXX

    auto pocaPlane = distToPlane*vecN + posRiemann; // point of closest approach from riemann sphere center to plane of the fit.
    auto meanProjection2 = meanProjection + posRiemannBot;

    auto vRiemann = posRiemann.Dot(vecV);
    //auto vCenterHelix = centerHelix.Dot(vecV);
    auto vPocaPlane = pocaPlane.Dot(vecV);
    auto kPocaPlane = rRiemann + distToPlane * vecN.K();
    auto rPlaneCut = sqrt(rRiemann*rRiemann - distToPlane*distToPlane); // Cut Circle Radius (cutted by fit plane)
    auto dvPlaneCut = rPlaneCut * vecU.Dot(vecV);;

    LKVector3 vecT = LKVector3(ref, vecV.J(), vecV.I(), 0); // normal to vecV in ij plane
    auto tRiemann = posRiemann.Dot(vecT);

    Double_t scale = 1.1;
    Double_t dx = scale*rRiemann;

    cvs0 -> cd(); auto frame0 = new TH2D("side (t)",";t;k", 100, tRiemann-dx, tRiemann+dx, 100, rRiemann-dx, rRiemann+dx); frame0 -> Draw();
    cvs1 -> cd(); auto frame1 = new TH2D("side (v)",";v;k", 100, vRiemann-dx, vRiemann+dx, 100, rRiemann-dx, rRiemann+dx); frame1 -> Draw();
    cvs2 -> cd(); auto frame2 = new TH2D("top",     ";i;j", 100, posRiemann.I()-dx, posRiemann.I()+dx, 100, posRiemann.J()-dx, posRiemann.J()+dx); frame2 -> Draw();

    auto markProjection = new TMarker(0,0,25);
    markProjection -> SetMarkerColor(kGreen-2);
    markProjection -> SetMarkerSize(0.4);

    auto markOriginal = new TMarker(0,0,24);
    markOriginal -> SetMarkerColor(kGray+1);
    markOriginal -> SetMarkerSize(0.4);

    nextHit.Begin();
    while ((hit = (LKHit *) nextHit()))
    {
        LKVector3 pos(hit -> GetPosition(), ref);
        auto i0 = pos.I() - posRiemann.I();
        auto j0 = pos.J() - posRiemann.J();

        Double_t aaa = sqrt(i0*i0 + j0*j0) / (2*rRiemann);
        Double_t bbb = 1 + aaa*aaa;

        LKVector3 posProjection2(ref, i0/bbb + posRiemann.I(), j0/bbb + posRiemann.J(), 2*rRiemann*aaa*aaa/bbb);
        auto tProjection2 = posProjection2.Dot(vecT);
        auto tPos = pos.Dot(vecT);
        auto vProjection2 = posProjection2.Dot(vecV);
        auto vPos = pos.Dot(vecV);

        cvs0 -> cd(); markProjection -> DrawMarker(tProjection2,      posProjection2.K()); markOriginal -> DrawMarker(tPos, 0);
        cvs1 -> cd(); markProjection -> DrawMarker(vProjection2,      posProjection2.K()); markOriginal -> DrawMarker(vPos, 0);
        cvs2 -> cd(); markProjection -> DrawMarker(posProjection2.I(),posProjection2.J()); markOriginal -> DrawMarker(pos.I(),pos.J());
    }

    markProjection -> SetMarkerColor(kViolet+1);
    markProjection -> SetMarkerStyle(34);
    markProjection -> SetMarkerSize(1.2);

    markOriginal -> SetMarkerColor(kPink);
    markOriginal -> SetMarkerStyle(41);
    markOriginal -> SetMarkerSize(1.2);

    for (auto hit0 : {GetHit(0), GetLastHit()})
    {
        LKVector3 pos(hit0 -> GetPosition(), ref);
        auto i0 = pos.I() - posRiemann.I();
        auto j0 = pos.J() - posRiemann.J();

        Double_t aaa = sqrt(i0*i0 + j0*j0) / (2*rRiemann);
        Double_t bbb = 1 + aaa*aaa;

        LKVector3 posProjection2(ref, i0/bbb + posRiemann.I(), j0/bbb + posRiemann.J(), 2*rRiemann*aaa*aaa/bbb);
        auto tProjection2 = posProjection2.Dot(vecT);
        auto tPos = pos.Dot(vecT);
        auto vProjection2 = posProjection2.Dot(vecV);
        auto vPos = pos.Dot(vecV);

        cvs0 -> cd(); markProjection -> DrawMarker(tProjection2,      posProjection2.K()); markOriginal -> DrawMarker(tPos,0);
        cvs1 -> cd(); markProjection -> DrawMarker(vProjection2,      posProjection2.K()); markOriginal -> DrawMarker(vPos,0);
        cvs2 -> cd(); markProjection -> DrawMarker(posProjection2.I(),posProjection2.J()); markOriginal -> DrawMarker(pos.I(),pos.J());

        auto lst1 = LKGeoLine(tPos ,0, 0, tRiemann, 2*rRiemann, 0).DrawArrowXY(); lst1 -> SetLineStyle(2);
        auto lst2 = LKGeoLine(vPos ,0, 0, vRiemann, 2*rRiemann, 0).DrawArrowXY(); lst2 -> SetLineStyle(2);

        cvs0 -> cd(); lst1 -> Draw("samel");
        cvs1 -> cd(); lst2 -> Draw("samel");
    }

    cvs0 -> cd();
    auto riemannCircle0 = LKGeoSphere(0,tRiemann,rRiemann,rRiemann).DrawCircleYZ(); riemannCircle0 -> SetLineStyle(2);
    riemannCircle0 -> Draw("same");

    auto markRiemann0 = new TMarker(tRiemann,posRiemann.K(),24);
    markRiemann0 -> Draw("samep");

    cvs1 -> cd();
    auto riemannCircle1 = LKGeoSphere(0,vRiemann,rRiemann,rRiemann).DrawCircleYZ();
    riemannCircle1 -> SetLineStyle(2);

    auto lineToPlane1    = LKGeoLine(vRiemann, rRiemann, 0, vPocaPlane, kPocaPlane, 0).DrawArrowXY();
    auto lineToEndP1     = LKGeoLine(vPocaPlane, kPocaPlane, 0, vPocaPlane+dvPlaneCut, kPocaPlane+rPlaneCut*vecU.K(), 0).DrawArrowXY();
    auto lineToEndM1     = LKGeoLine(vPocaPlane, kPocaPlane, 0, vPocaPlane-dvPlaneCut, kPocaPlane-rPlaneCut*vecU.K(), 0).DrawArrowXY();
    auto lineFromPlaneP1 = LKGeoLine(vRiemann, rRiemann, 0, vRiemann+vp, rRiemann+wp.K(), 0).DrawArrowXY();
    auto lineFromPlaneM1 = LKGeoLine(vRiemann, rRiemann, 0, vRiemann+vm, rRiemann+wm.K(), 0).DrawArrowXY();
    auto lineFromTopP1   = LKGeoLine(vRiemann, 2*rRiemann, 0, vRiemann+(vp>0?1:-1)*2*rRiemann*tangentP, 0, 0).DrawArrowXY();
    auto lineFromTopM1   = LKGeoLine(vRiemann, 2*rRiemann, 0, vRiemann+(vm>0?1:-1)*2*rRiemann*tangentM, 0, 0).DrawArrowXY();
    auto markPocaPlane1  = new TMarker(vPocaPlane,kPocaPlane,20);
    auto markRiemann1    = new TMarker(vRiemann,posRiemann.K(),24);

    lineToEndP1     -> SetLineColor(kOrange);
    lineToEndM1     -> SetLineColor(kOrange);
    lineFromPlaneP1 -> SetLineColor(kBlue);
    lineFromPlaneM1 -> SetLineColor(kBlue);
    lineFromTopP1   -> SetLineColor(kGray);
    lineFromTopM1   -> SetLineColor(kGray);

    //auto lineVecN = LKGeoLine(vRiemann, rRiemann, 0, 20*vecN.Dot(vecV), 20*vecN.K(), 0).DrawArrowXY();
    //lineVecN -> SetLineWidth(2);
    //lineVecN -> Draw("same>");

    riemannCircle1 -> Draw("samel");
    lineToPlane1 -> Draw("same>");
    lineToEndP1 -> Draw("same>");
    lineToEndM1 -> Draw("same>");
    lineFromPlaneP1 -> Draw("same>");
    lineFromPlaneM1 -> Draw("same>");
    lineFromTopP1 -> Draw("same>");
    lineFromTopM1 -> Draw("same>");
    markPocaPlane1 -> Draw("samep");
    markRiemann1 -> Draw("samep");

    cvs2 -> cd();
    auto riemannCircle2 = LKGeoSphere(posRiemann.I(),posRiemann.J(),0,rRiemann).DrawCircleXY();
    auto lineToPlane2 = LKGeoLine(posRiemann.I(),posRiemann.J(),0,distToPlane*vecN.I()+posRiemann.I(),distToPlane*vecN.J()+posRiemann.J(),0).DrawArrowXY();
    auto lineToEndu2 = LKGeoLine(pocaPlane.I()-dvPlaneCut*vecV.I(),pocaPlane.J()-dvPlaneCut*vecV.J(),0, pocaPlane.I()+dvPlaneCut*vecV.I(),pocaPlane.J()+dvPlaneCut*vecV.J(),0).DrawArrowXY();
    auto markPocaPlane2 = new TMarker(pocaPlane.I(),pocaPlane.J(),29);
    auto markRiemann2 = new TMarker(posRiemann.I(),posRiemann.J(),24);

    riemannCircle2 -> SetLineStyle(2);
    lineToEndu2 -> SetLineColor(kOrange);

    riemannCircle2 -> Draw("samel");
    lineToPlane2 -> Draw("samel>");
    lineToEndu2 -> Draw("same>");
    markPocaPlane2 -> Draw("samep");
    markRiemann2 -> Draw("samep");

    //auto tspMean = meanProjection.Dot(vecT);
    auto vMeanProjection = meanProjection.Dot(vecV);
    auto markMeanProjection = new TMarker(0,0,29);
    markMeanProjection -> SetMarkerSize(1.2);
    markMeanProjection -> SetMarkerColor(kBlue+1);

    cvs1 -> cd();
    markMeanProjection -> DrawMarker(vMeanProjection+vRiemann,meanProjection.K());
    //auto finalCircleFitv = LKGeoLine(vCenterHelix-rHelix, 0, 0, vCenterHelix+rHelix, 0, 0).DrawArrowXY();
    //finalCircleFitv -> SetLineColor(kRed);
    //finalCircleFitv -> Draw("samel");

    cvs2 -> cd();
    markMeanProjection -> DrawMarker(meanProjection2.I(),meanProjection2.J());
    auto finalCircleFit = fitCircle.DrawCircle();
    finalCircleFit -> SetLineColor(kRed);
    finalCircleFit -> Draw("samel");

    return cvs0;
}

LKGeoHelix LKHitArray::FitHelix(axis_t ref)
{
    auto circle = FitCircle(ref);

    LKGeoHelix helix;
    helix.SetRMS (-1);
    helix.SetRMSR(-1);
    helix.SetRMST(-1);

    if (fN < 5)
        return helix;

    SortByX(true);
    SortByY(true);
    SortByZ(true);

    auto helixI = circle.GetX();
    auto helixJ = circle.GetY();

    auto pos0 = GetHit(0) -> GetPosition(ref);
    auto i = pos0.I() - helixI;
    auto j = pos0.J() - helixJ;
    auto axisI = TVector2( i,j).Unit();
    auto axisJ = TVector2(-j,i).Unit();

    auto meanK      = 0.;  // <k>
    auto meanAlpha  = 0.;  // <alpha>
    auto meanAlpha2 = 0.;  // <alpha*alpha>
    auto meanAlphaK = 0.;  // <alpha*k>

    auto alphaStack = 0.;                 // alpha angle stack (save point)
    auto alphaInit = TMath::ATan2(j, i);  // alpha angle of first hit
    auto alphaMin = alphaInit;            // maximum value of alpha angle
    auto alphaMax = alphaInit;            // minimum value of alpha angle

    auto halfOfPi = .5*TMath::Pi();

    TIter nextHit(this);
    LKHit *hit;
    while ((hit = (LKHit *) nextHit()))
    {
        auto w = hit -> GetCharge();
        auto p = hit -> GetPosition(ref);
        auto v = TVector2(p.I()-helixI,p.J()-helixJ);
        auto rotI = v*axisI;
        auto rotJ = v*axisJ;

        auto alpha = TMath::ATan2(rotJ, rotI);
        if (alpha > halfOfPi || alpha < -halfOfPi) { // if the angle is over +-pi/2 relative to alphaStack
            alpha += alphaStack;
            alphaStack = alpha;
            axisI = v.Unit();
            axisJ = TVector2(-v.Y(), v.X()).Unit();
        } else
            alpha += alphaStack;

        alpha += alphaInit;
        if (alpha < alphaMin) alphaMin = alpha;
        if (alpha > alphaMax) alphaMax = alpha;
        hit -> SetAlpha(alpha);

        meanK      += w * p.K();
        meanAlpha  += w * alpha;
        meanAlpha2 += w * alpha * alpha;
        meanAlphaK += w * alpha * p.K();
    }

    meanK      /= fW;
    meanAlpha  /= fW;
    meanAlpha2 /= fW;
    meanAlphaK /= fW;

    Double_t slope  = (meanAlphaK - meanAlpha*meanK) / (meanAlpha2 - meanAlpha*meanAlpha);
    Double_t offset = (meanAlpha2*meanK - meanAlpha*meanAlphaK) / (meanAlpha2 - meanAlpha*meanAlpha);

    if (std::isinf(slope))
        return helix;

    helix.SetI(helixI);
    helix.SetJ(helixJ);
    helix.SetR(circle.GetR());
    helix.SetH(alphaMax);
    helix.SetT(alphaMin);
    helix.SetS(slope);
    helix.SetK(offset);
    helix.SetA(ref);

    Double_t sr = 0; // = sum of distance from hit to track in radial axis
    Double_t st = 0; // = sum of distance from hit to track in axial axis
    Double_t ss = 0; // = sum of distance from hit to track

    nextHit.Begin();
    while ((hit = (LKHit *) nextHit()))
    {
        auto w = hit -> GetCharge();
        TVector3 mp = helix.HelicoidMap(hit->GetPosition(),hit->GetAlpha());
        auto xx = mp.X() * mp.X();
        auto yy = mp.Y() * mp.Y();

        sr += w * xx;
        st += w * yy;
        ss += w * (xx + yy);
    }

    helix.SetRMS (sqrt(ss/(fW*(1-3/fN))));
    helix.SetRMSR(sqrt(sr/(fW*(1-3/fN))));
    helix.SetRMST(sqrt(st/(fW*(1-3/fN))));

    return helix;
}

void LKHitArray::AddHit(LKHit* hit)
{
    Add(hit);

    AddXYZW(hit->GetX(), hit->GetY(), hit->GetZ(), hit->GetCharge());
}

void LKHitArray::AddXYZW(TVector3 pos, Double_t w)
{
    AddXYZW(pos.X(),pos.Y(),pos.Z(),w);
}

void LKHitArray::AddXYZW(Double_t x, Double_t y, Double_t z, Double_t w)
{
    Double_t wsum = fW + w;

    fEX  = (fW*fEX + w*x) / wsum;
    fEY  = (fW*fEY + w*y) / wsum;
    fEZ  = (fW*fEZ + w*z) / wsum;
    fEXX = (fW*fEXX + w*x*x) / wsum;
    fEYY = (fW*fEYY + w*y*y) / wsum;
    fEZZ = (fW*fEZZ + w*z*z) / wsum;
    fEXY = (fW*fEXY + w*x*y) / wsum;
    fEYZ = (fW*fEYZ + w*y*z) / wsum;
    fEZX = (fW*fEZX + w*z*x) / wsum;

    fW = wsum;
    ++fN;
}

bool LKHitArray::RemoveHit(Int_t iHit)
{
    auto hit = GetHit(iHit);
    RemoveAt(iHit);
    return Subtract(hit->GetX(), hit->GetY(), hit->GetZ(), hit->GetCharge());
}

bool LKHitArray::RemoveHit(LKHit* hit)
{
    auto id = hit -> GetHitID(); 
    Int_t numHits = GetEntriesFast();
    for (auto iHit = 0; iHit < numHits; iHit++) {
        auto hit0 = GetHit(iHit);
        auto id0 = hit0 -> GetHitID();
        if (id0 == id) {
            RemoveAt(iHit);
            return Subtract(hit->GetX(), hit->GetY(), hit->GetZ(), hit->GetCharge());
        }
    }

    return false;
}

void LKHitArray::RemoveLastHit()
{
    auto iHit = GetEntriesFast()-1;
    auto hit = GetHit(iHit);
    RemoveAt(iHit);
    Subtract(hit->GetX(), hit->GetY(), hit->GetZ(), hit->GetCharge());
}


bool LKHitArray::Subtract(TVector3 pos, Double_t w)
{
    return Subtract(pos.X(),pos.Y(),pos.Z(),w);
}

bool LKHitArray::Subtract(Double_t x, Double_t y, Double_t z, Double_t w)
{
    Double_t wsum = fW - w;

    fEX  = (fW * fEX - w * x) / wsum;
    fEY  = (fW * fEY - w * y) / wsum;
    fEZ  = (fW * fEZ - w * z) / wsum;
    fEXX = (fW * fEXX - w * x * x) / wsum;
    fEYY = (fW * fEYY - w * y * y) / wsum;
    fEZZ = (fW * fEZZ - w * z * z) / wsum;
    fEXY = (fW * fEXY - w * x * y) / wsum;
    fEYZ = (fW * fEYZ - w * y * z) / wsum;
    fEZX = (fW * fEZX - w * z * x) / wsum;

    fW = wsum;
    --fN;

    return true;
}

Int_t LKHitArray::GetNumHits() const { return fN; };


Double_t LKHitArray::GetW()  const { return fW; }
Double_t LKHitArray::GetChargeSum()  const { return fW; }

Double_t LKHitArray::GetMeanX() const { return fEX; }
Double_t LKHitArray::GetMeanY() const { return fEY; }
Double_t LKHitArray::GetMeanZ() const { return fEZ; }

Double_t LKHitArray::GetMeanXX() const { return fEXX; }
Double_t LKHitArray::GetMeanYY() const { return fEYY; }
Double_t LKHitArray::GetMeanZZ() const { return fEZZ; }
Double_t LKHitArray::GetMeanXY() const { return fEXY; }
Double_t LKHitArray::GetMeanYZ() const { return fEYZ; }
Double_t LKHitArray::GetMeanZX() const { return fEZX; }

Double_t LKHitArray::GetVarianceX()  const { return fEXX - fEX * fEX; }
Double_t LKHitArray::GetVarianceY()  const { return fEYY - fEY * fEY; }
Double_t LKHitArray::GetVarianceZ()  const { return fEZZ - fEZ * fEZ; }

Double_t LKHitArray::GetAXX() const { return fW * (fEXX - fEX * fEX); }
Double_t LKHitArray::GetAYY() const { return fW * (fEYY - fEY * fEY); }
Double_t LKHitArray::GetAZZ() const { return fW * (fEZZ - fEZ * fEZ); }
Double_t LKHitArray::GetAXY() const { return fW * (fEXY - fEX * fEY); }
Double_t LKHitArray::GetAYZ() const { return fW * (fEYZ - fEY * fEZ); }
Double_t LKHitArray::GetAZX() const { return fW * (fEZX - fEZ * fEX); }

TVector3 LKHitArray::GetMean()          const { return TVector3(fEX,fEY,fEZ); }
TVector3 LKHitArray::GetVariance()      const { return TVector3(fEXX-fEX*fEX,fEYY-fEY*fEY,fEZZ-fEZ*fEZ); }
TVector3 LKHitArray::GetCovariance()    const { return TVector3(fEXY-fEX*fEY,fEYZ-fEY*fEZ,fEZX-fEZ*fEX); }
TVector3 LKHitArray::GetStdDev()        const { return TVector3(TMath::Sqrt(fEXY-fEX*fEY),TMath::Sqrt(fEYZ-fEY*fEZ),TMath::Sqrt(fEZX-fEZ*fEX)); }
TVector3 LKHitArray::GetSquaredMean()   const { return TVector3(fEXX,fEYY,fEZZ); }
TVector3 LKHitArray::GetCoSquaredMean() const { return TVector3(fEXY,fEYZ,fEZX); }

LKVector3 LKHitArray::GetMean(axis_t ref)          const { return LKVector3(fEX,fEY,fEZ,ref); }
LKVector3 LKHitArray::GetVariance(axis_t ref)      const { return LKVector3(fEXX-fEX*fEX,fEYY-fEY*fEY,fEZZ-fEZ*fEZ,ref); }
LKVector3 LKHitArray::GetCovariance(axis_t ref)    const { return LKVector3(fEXY-fEX*fEY,fEYZ-fEY*fEZ,fEZX-fEZ*fEX,ref); }
LKVector3 LKHitArray::GetStdDev(axis_t ref)        const { return LKVector3(TMath::Sqrt(fEXY-fEX*fEY),TMath::Sqrt(fEYZ-fEY*fEZ),TMath::Sqrt(fEZX-fEZ*fEX),ref); }
LKVector3 LKHitArray::GetSquaredMean(axis_t ref)   const { return LKVector3(fEXX,fEYY,fEZZ,ref); }
LKVector3 LKHitArray::GetCoSquaredMean(axis_t ref) const { return LKVector3(fEXY,fEYZ,fEZX,ref); }

TGraphErrors *LKHitArray::DrawGraph(axis_t axis1, axis_t axis2)
{
    auto graph = new TGraphErrors();
    graph -> SetMarkerStyle(20);

    TIter next(this);
    LKHit *hit;
    while ((hit = (LKHit *) next())) {
        LKVector3 pos(hit->GetPosition());
        graph -> SetPoint(graph->GetN(),pos.At(axis1),pos.At(axis2));
    }

    return graph;
}
