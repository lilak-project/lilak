#include "TStyle.h"
#include "TRootBrowser.h"
#include "TBrowser.h"
#include "TGTab.h"
#include "TVirtualX.h"
#include "TGWindow.h"
#include "TGeoManager.h"
#include "TRootEmbeddedCanvas.h"

#ifdef ACTIVATE_EVE
#include "TEveViewer.h"
#include "TGLViewer.h"
#include "TEveGeoNode.h"
#include "TEveManager.h"
#include "TEveScene.h"
#include "TEveWindow.h"
#include "TEveWindowManager.h"
#include "TEveGedEditor.h"
#include "TEveBrowser.h"
#include "TEvePointSet.h"
#include "TEveLine.h"
#include "TEveArrow.h"
#endif

#include "LKLogger.h"
#include "LKRun.h"
#include "LKEveTask.h"
#include "LKWindowManager.h"

ClassImp(LKEveTask)

LKEveTask::LKEveTask()
    :LKTask("LKEveTask","")
{
}

bool LKEveTask::Init()
{
    fNumBranches = fRun -> GetNumBranches();

    if (fPar->CheckPar("LKEveTask/selectTrackIDs"))       fSelTrkIDs      = fPar -> GetParVInt("LKEveTask/selectTrackIDs");
    if (fPar->CheckPar("LKEveTask/ignoreTrackIDs"))       fIgnTrkIDs      = fPar -> GetParVInt("LKEveTask/ignoreTrackIDs");
    if (fPar->CheckPar("LKEveTask/selectTrackParentIDs")) fSelPntIDs      = fPar -> GetParVInt("LKEveTask/selectTrackParentIDs");
    if (fPar->CheckPar("LKEveTask/ignoreTrackParentIDs")) fIgnPntIDs      = fPar -> GetParVInt("LKEveTask/ignoreTrackParentIDs");
    if (fPar->CheckPar("LKEveTask/selectTrackPDGs"))      fSelPDGs        = fPar -> GetParVInt("LKEveTask/selectTrackPDGs");
    if (fPar->CheckPar("LKEveTask/ignoreTrackPDGs"))      fIgnPDGs        = fPar -> GetParVInt("LKEveTask/ignoreTrackPDGs");
    if (fPar->CheckPar("LKEveTask/selectMCIDs"))          fSelMCIDs       = fPar -> GetParVInt("LKEveTask/selectMCIDs");
    if (fPar->CheckPar("LKEveTask/ignoreMCIDs"))          fIgnMCIDs       = fPar -> GetParVInt("LKEveTask/ignoreMCIDs");
    if (fPar->CheckPar("LKEveTask/selectHitParentIDs"))   fSelHitPntIDs   = fPar -> GetParVInt("LKEveTask/selectHitParentIDs");
    if (fPar->CheckPar("LKEveTask/ignoreHitParentIDs"))   fIgnHitPntIDs   = fPar -> GetParVInt("LKEveTask/ignoreHitParentIDs");
    if (fPar->CheckPar("LKEveTask/selectBranches"))       fSelBranchNames = fPar -> GetParVString("LKEveTask/selectBranches");

    fPar -> CheckPar("LKEveTask/drawEve3D      false    # draw 3d event display using ROOT EVE package and root geometry defined in detector class");
    fPar -> CheckPar("LKEveTask/drawPlane      true     # draw 2d planes using detector plane class");
    fPar -> CheckPar("[BranchName]/lineStyle   1        # track line style of [BranchName]. [BranchName] should be replaced to the actual branch name");
    fPar -> CheckPar("[BranchName]/lineWidth   1        # track line width of [BranchName]. [BranchName] should be replaced to the actual branch name");;
    fPar -> CheckPar("[BranchName]/lineColor   kBlack   # track line color of [BranchName]. [BranchName] should be replaced to the actual branch name");;
    fPar -> CheckPar("[BranchName]/markerStyle 20       # track marker style of [BranchName]. [BranchName] should be replaced to the actual branch name");
    fPar -> CheckPar("[BranchName]/markerSize  1        # track marker size  of [BranchName]. [BranchName] should be replaced to the actual branch name");
    fPar -> CheckPar("[BranchName]/markerColor kBlue    # track marker style of [BranchName]. [BranchName] should be replaced to the actual branch name");

    fNumSelectedBranches = fSelBranchNames.size();
    if (fNumSelectedBranches==0) {
        fNumSelectedBranches = fNumBranches;
        for (auto iBranch=0; iBranch<fNumSelectedBranches; ++iBranch)
            fSelBranchNames.push_back(fRun->GetBranchName(iBranch));
    }
    else {
        vector<TString> tempBranchNames;
        for (auto iBranch=0; iBranch<fNumSelectedBranches; ++iBranch) {
            TString branchName = fSelBranchNames.at(iBranch);
            auto branchPtr = fRun -> GetBranchA(branchName);
            if (branchPtr == nullptr) {
                lk_error << "No eve-branch name " << branchName << endl;
                continue;
            }
            tempBranchNames.push_back(branchName);
        }
        fSelBranchNames.clear();
        for (auto branchName : tempBranchNames)
            fSelBranchNames.push_back(branchName);
    }

    fDetectorSystem = fRun -> GetDetectorSystem();

    ConfigureDetectorPlanes();

    return true;
}

void LKEveTask::Exec(Option_t*)
{
    Bool_t drawEve3D = true;
    Bool_t drawPlane = true;
    if (fPar->CheckPar("LKEveTask/drawEve3D")) drawEve3D = fPar -> GetParBool("LKEveTask/drawEve3D");
    if (fPar->CheckPar("LKEveTask/drawPlane")) drawPlane = fPar -> GetParBool("LKEveTask/drawPlane");

    if (drawEve3D) DrawEve3D();
    if (drawPlane) DrawDetectorPlanes();
}

void LKEveTask::DrawEve3D()
{
#ifdef ACTIVATE_EVE
    if (gEve!=nullptr) {
        auto numEveEvents = fEveEventManagerArray -> GetEntries();
        for (auto iEveEvent=0; iEveEvent<numEveEvents; ++iEveEvent) {
            ((TEveEventManager *) fEveEventManagerArray -> At(iEveEvent)) -> RemoveElements();
        }
    }

    if (gEve == nullptr)
        ConfigureDisplayWindow();

    bool removePointTrack = (fPar->CheckPar("LKEveTask/removePointTrack")) ? (fPar->GetParBool("LKEveTask/removePointTrack")) : false;

    for (Int_t iBranch = 0; iBranch < fNumSelectedBranches; ++iBranch)
    {
        TString branchName = fSelBranchNames.at(iBranch);
        auto branch = fRun -> GetBranchA(branchName);
        if (branch == nullptr) {
            lk_error << "No eve-branch name " << branchName << endl;
            continue;
        }
        if (branch -> GetEntries() == 0)
            continue;

        auto objSample = branch -> At(0);
        if (objSample -> InheritsFrom("LKContainer") == false)
            continue;

        bool isTracklet = (objSample -> InheritsFrom("LKTracklet")) ? true : false;
        bool isHit = (objSample -> InheritsFrom("LKHit")) ? true : false;

        LKContainer *eveObj = (LKContainer *) objSample;
        if (fSelBranchNames.size() == 0 || !eveObj -> DrawByDefault())
            continue;

        auto eveEvent = (TEveEventManager *) fEveEventManagerArray -> FindObject(branchName);
        if (eveEvent==nullptr) {
            eveEvent = new TEveEventManager(branchName);
            fEveEventManagerArray -> Add(eveEvent);
            gEve -> AddEvent(eveEvent);
        }

        int numSelected = 0;

        Int_t nObjects = branch -> GetEntries();
        if (isTracklet)
        {
            for (Int_t iObject = 0; iObject < nObjects; ++iObject) {
                LKTracklet *tracklet = (LKTracklet *) branch -> At(iObject);

                /*
                   if (removePointTrack)
                   if (tracklet -> InheritsFrom("LKMCTrack"))
                   if (((LKMCTrack *) tracklet) -> GetNumVertices() < 2)
                   continue;
                 */

                if (!SelectTrack(tracklet))
                    continue;

                auto eveLine = (TEveLine *) tracklet -> CreateEveElement();
                tracklet -> SetEveElement(eveLine, fEveScale);
                SetEveLineAtt(eveLine,branchName);
                eveEvent -> AddElement(eveLine);
                numSelected++;
            }
        }
        else if (eveObj -> IsEveSet())
        {
            auto eveSet = (TEvePointSet *) eveObj -> CreateEveElement();
            TString name = Form("%s_%s",eveSet->GetElementName(),branchName.Data());
            eveSet -> SetElementName(name);
            for (Int_t iObject = 0; iObject < nObjects; ++iObject) {
                eveObj = (LKContainer *) branch -> At(iObject);

                if (isHit)  {
                    LKHit *hit = (LKHit *) branch -> At(iObject);
                    hit -> SetSortValue(1);

                    if (!SelectHit(hit))
                        continue;
                }

                eveObj -> AddToEveSet(eveSet, fEveScale);
                numSelected++;
            }
            SetEveMarkerAtt(eveSet, branchName);
            eveEvent -> AddElement(eveSet);
        }
        else {
            for (Int_t iObject = 0; iObject < nObjects; ++iObject) {
                eveObj = (LKContainer *) branch -> At(iObject);
                auto eveElement = eveObj -> CreateEveElement();
                eveObj -> SetEveElement(eveElement, fEveScale);
                TString name = Form("%s_%d",eveElement -> GetElementName(),iObject);
                eveElement -> SetElementName(name);
                eveEvent -> AddElement(eveElement);
                numSelected++;
            }
        }

        lk_info << "Drawing " << branchName << " [" << branch -> At(0) -> ClassName() << "] " << numSelected << "(" << branch -> GetEntries() << ")" << endl;
    }

    if (fPar->CheckPar("LKEveTask/axisOrigin")) {
        Double_t length = 100.;
        if (fPar->CheckPar("LKEveTask/axisLength"))
            length = fPar->GetParDouble("LKEveTask/axisLength");
        TVector3 origin = fPar -> GetParV3("LKEveTask/axisOrigin");
        for (auto iaxis : {0,1,2}) {
            TVector3 direction;
            if (iaxis==0) direction = TVector3(length,0,0);
            else if (iaxis==1) direction = TVector3(0,length,0);
            else if (iaxis==2) direction = TVector3(0,0,length);
            auto axis = new TEveArrow(direction.X(),direction.Y(),direction.Z(),origin.X(),origin.Y(),origin.Z()); // TODO
            axis -> SetElementName(TString("axis_")+iaxis);
            if (iaxis==0) axis -> SetMainColor(kRed);
            else if (iaxis==1) axis -> SetMainColor(kBlue);
            else if (iaxis==2) axis -> SetMainColor(kBlack);
            ((TEveEventManager *) fEveEventManagerArray -> At(0)) -> AddElement(axis);
        }
    }

    gEve -> Redraw3D();
#else

    auto axis1 = LKVector3::kZ;
    auto axis2 = LKVector3::kX;
    auto axis3 = LKVector3::kY;

    if (fCanvas3D==nullptr) {
        fCanvas3D = LKWindowManager::GetWindowManager() -> CanvasSquare("LKEveCanvas3D",0.6);
        auto detector = fDetectorSystem -> GetDetector(0);
        double x1, y1, z1, x2, y2, z2;
        auto success = detector -> GetEffectiveDimension(x1, y1, z1, x2, y2, z2);
        LKVector3 min(x1,y1,z1);
        LKVector3 max(x2,y2,z2);
        //fFrame3D = new TH3D(Form("frame_%s",detector->GetName()),Form("%s;z;x;y",detector->GetName()),100,x1,x2,100,y1,y2,100,z1,z2);
        fFrame3D = new TH3D(
                Form("frame_%s",detector->GetName()),
                Form("%s;z;x;y",detector->GetName()),
                100,min.At(axis1),max.At(axis1),
                100,min.At(axis2),max.At(axis2),
                100,min.At(axis3),max.At(axis3));
        fFrame3D -> SetStats(0);
        fGraphTrack3DArray = new TClonesArray("TGraph2DErrors",100);
        fGraphHit3DArray = new TClonesArray("TGraph2DErrors",100);
    }
    fCanvas3D -> cd();
    fFrame3D -> Draw();
    fGraphTrack3DArray -> Clear("C");
    fGraphHit3DArray -> Clear("C");
    int countHitGraphs = 0;
    int countTracklets = 0;

    for (Int_t iBranch = 0; iBranch < fNumSelectedBranches; ++iBranch)
    {
        TString branchName = fSelBranchNames.at(iBranch);
        auto branchA = fRun -> GetBranchA(branchName);
        if (branchA == nullptr) {
            lk_error << "No eve-branchA name " << branchName << endl;
            continue;
        }
        Int_t numObjects = branchA -> GetEntries();
        if (numObjects==0)
            continue;

        auto objSample = branchA -> At(0);
        if (objSample -> InheritsFrom("LKContainer") == false)
            continue;

        bool isTracklet = (objSample -> InheritsFrom("LKTracklet")) ? true : false;
        bool isHit      = (objSample -> InheritsFrom("LKHit"))      ? true : false;

        LKContainer *eveObj = (LKContainer *) objSample;
        if (fSelBranchNames.size()==0 || !eveObj->DrawByDefault())
            continue;

        if (isTracklet)
        {
            lk_info << numObjects << " tracks found!" << endl;
            auto trackletSample = (LKTracklet *) objSample;
            //if (trackletSample -> DoDrawOnDetectorPlane())
            {
                for (auto iTracklet = 0; iTracklet < numObjects; ++iTracklet) {
                    auto tracklet = (LKTracklet *) branchA -> At(iTracklet);
                    if (!SelectTrack(tracklet))
                        continue;

                    auto graphTrack3D = (TGraph2DErrors*) fGraphTrack3DArray -> ConstructedAt(countTracklets++);
                    graphTrack3D -> Clear();
                    tracklet -> FillTrajectory3D(graphTrack3D,LKVector3::kZ,LKVector3::kX,LKVector3::kY);
                    graphTrack3D -> SetLineColor(kRed);
                    SetGraphAtt(graphTrack3D,branchName);
                    //SetEveMarkerAtt(graphTrack3D,branchName);
                    graphTrack3D -> Draw("same line");
                }
            }
        }
        else if (isHit)
        {
            lk_info << numObjects << " hits found!" << endl;
            auto hitSample = (LKHit *) objSample;
            //if (hitSample -> DoDrawOnDetectorPlane())
            {
                auto graphHit3D = (TGraph2DErrors*) fGraphHit3DArray -> ConstructedAt(countHitGraphs++);
                graphHit3D -> Clear();
                for (auto iHit=0; iHit<numObjects; ++iHit) {
                    auto hit = (LKHit *) branchA -> At(iHit);
                    if (!SelectHit(hit))
                        continue;

                    LKVector3 pos(hit -> GetPosition());
                    LKVector3 err(hit -> GetPositionError());
                    graphHit3D -> SetPoint(graphHit3D->GetN(),pos.At(axis1),pos.At(axis2),pos.At(axis3));
                    graphHit3D -> SetPointError(graphHit3D->GetN()-1,err.At(axis1),err.At(axis2),err.At(axis3));
                    //hit -> FillGraph3D(graphHit3D,LKVector3::kZ,LKVector3::kX,LKVector3::kY);
                }
                //graphHit3D -> Draw("same p error");
                graphHit3D -> SetMarkerStyle(20);
                graphHit3D -> SetMarkerSize(0.5);
                SetGraphAtt(graphHit3D,branchName);
                //SetEveMarkerAtt(graphHit3D,branchName);
                graphHit3D -> Draw("same p error");
            }
        }
    }
#endif
}

void LKEveTask::DrawDetectorPlanes()
{
    auto numPlanes = fDetectorSystem -> GetNumPlanes();
    for (auto iPlane = 0; iPlane < numPlanes; ++iPlane)
    {
        auto plane = fDetectorSystem -> GetDetectorPlane(iPlane);
        lk_info << "Drawing " << plane -> GetName() << endl;

        auto cvs = (TCanvas *) fCvsDetectorPlaneArray -> At(iPlane);

        cvs -> cd();
        plane -> Draw();

        auto axis1 = plane -> GetAxis1();
        auto axis2 = plane -> GetAxis2();
        if (axis1==LKVector3::kNon||axis2==LKVector3::kNon)
            continue;

        auto numPads = plane -> GetNumCPads();
        for (auto iPad=0; iPad<numPads; ++iPad)
        {
            for (Int_t iBranch = 0; iBranch < fNumSelectedBranches; ++iBranch)
            {
                TClonesArray *branchA = nullptr;
                TString branchName = "";
                if (fNumSelectedBranches != 0) {
                    branchName = fSelBranchNames.at(iBranch);
                    branchA = fRun -> GetBranchA(branchName);
                }
                else {
                    branchName = fRun -> GetBranchName(iBranch);
                    branchA = fRun -> GetBranchA(iBranch);
                }

                Int_t numObjects = branchA -> GetEntries();
                if (iPlane==0&&iPad==0) lk_info << "Branch: " << branchName << " " << numObjects << endl;
                if (numObjects==0)
                    continue;

                TObject *objSample = branchA -> At(0);
                bool isTracklet = (objSample -> InheritsFrom("LKTracklet")) ? true : false;
                bool isHit = (objSample -> InheritsFrom("LKHit")) ? true : false;
                if (!isTracklet && !isHit)
                    continue;

                if (isTracklet)
                {
                    if (iPlane==0&&iPad==0) lk_info << numObjects << " tracks found!" << endl;
                    auto trackletSample = (LKTracklet *) objSample;
                    if (trackletSample -> DoDrawOnDetectorPlane())
                    {
                        for (auto iTracklet = 0; iTracklet < numObjects; ++iTracklet) {
                            auto tracklet = (LKTracklet *) branchA -> At(iTracklet);
                            if (!SelectTrack(tracklet))
                                continue;

                            plane -> GetCPad(iPad);
                            tracklet -> TrajectoryOnPlane(axis1,axis2) -> Draw("samel"); // @todo
                        }
                    }
                }
                if (isHit) {;}
            }
        }
    }

    // @todo palette is changed when drawing top node because of TGeoMan(?)
    gStyle -> SetPalette(kBird);
}

void LKEveTask::ConfigureDetectorPlanes()
{
    if (fCvsDetectorPlaneArray==nullptr)
        fCvsDetectorPlaneArray = new TObjArray();

    if (fCvsDetectorPlaneArray->GetEntries()>0)
        return;

    auto numPlanes = fDetectorSystem -> GetNumPlanes();
    for (Int_t iPlane = 0; iPlane < numPlanes; iPlane++) {
        LKDetectorPlane *plane = fDetectorSystem -> GetDetectorPlane(iPlane);
        TCanvas *cvs = plane -> GetCanvas();
        fCvsDetectorPlaneArray -> Add(cvs);
    }
}

bool LKEveTask::SelectTrack(LKTracklet *tracklet)
{
    bool isGood = 1;
    if (fSelTrkIDs.size()!=0) { isGood=0; for (auto id:fSelTrkIDs) { if (tracklet->GetTrackID()==id)  { isGood=1; break; }}} if (!isGood) return false;
    if (fIgnTrkIDs.size()!=0) { isGood=1; for (auto id:fIgnTrkIDs) { if (tracklet->GetTrackID()==id)  { isGood=0; break; }}} if (!isGood) return false;
    if (fSelPntIDs.size()!=0) { isGood=0; for (auto id:fSelPntIDs) { if (tracklet->GetParentID()==id) { isGood=1; break; }}} if (!isGood) return false;
    if (fIgnPntIDs.size()!=0) { isGood=1; for (auto id:fIgnPntIDs) { if (tracklet->GetParentID()==id) { isGood=0; break; }}} if (!isGood) return false;
    if (fSelPDGs.size()!=0)   { isGood=0; for (auto id:fSelPDGs)   { if (tracklet->GetPDG()==id)      { isGood=1; break; }}} if (!isGood) return false;
    if (fIgnPDGs.size()!=0)   { isGood=1; for (auto id:fIgnPDGs)   { if (tracklet->GetPDG()==id)      { isGood=0; break; }}} if (!isGood) return false;
    //if (fSelMCIDs.size()!=0)  { isGood=0; for (auto id:fSelMCIDs)  { if (tracklet->GetMCID()==id)     { isGood=1; break; }}} if (!isGood) return false;
    //if (fIgnMCIDs.size()!=0)  { isGood=1; for (auto id:fIgnMCIDs)  { if (tracklet->GetMCID()==id)     { isGood=0; break; }}} if (!isGood) return false;
    return true;
}

bool LKEveTask::SelectHit(LKHit *hit)
{
    bool isGood = 1;
    if (fSelHitPntIDs.size()!=0) { isGood=0; for (auto id:fSelHitPntIDs) { if (hit->GetTrackID()==id) { isGood=1; break; }}} if (!isGood) { hit->SetSortValue(-1); return false; }
    if (fIgnHitPntIDs.size()!=0) { isGood=1; for (auto id:fIgnHitPntIDs) { if (hit->GetTrackID()==id) { isGood=0; break; }}} if (!isGood) { hit->SetSortValue(-1); return false; }
    //if (fSelMCIDs.size()!=0)     { isGood=0; for (auto id:fSelMCIDs)     { if (hit->GetMCID()==id)    { isGood=1; break; }}} if (!isGood) { hit->SetSortValue(-1); return false; }
    //if (fIgnMCIDs.size()!=0)     { isGood=1; for (auto id:fIgnMCIDs)     { if (hit->GetMCID()==id)    { isGood=0; break; }}} if (!isGood) { hit->SetSortValue(-1); return false; }
    return true;
}

void LKEveTask::SetEveLineAtt(TAttLine *el, TString branchName)
{
    if (fPar->CheckPar(branchName+"/lineAtt")) {
        auto style = fPar -> GetParStyle(branchName+"/lineAtt",0);
        auto color = fPar -> GetParWidth(branchName+"/lineAtt",1);
        auto width = fPar -> GetParColor(branchName+"/lineAtt",2);
        el -> SetLineStyle(style);
        el -> SetLineWidth(width);
        el -> SetLineColor(color);
    }
    if (fPar->CheckPar(branchName+"/lineStyle")) el -> SetLineStyle(fPar -> GetParStyle(branchName+"/lineStyle")); 
    if (fPar->CheckPar(branchName+"/lineWidth")) el -> SetLineWidth(fPar -> GetParWidth(branchName+"/lineWidth"));
    if (fPar->CheckPar(branchName+"/lineColor")) el -> SetLineColor(fPar -> GetParColor(branchName+"/lineColor"));
}

void LKEveTask::SetEveMarkerAtt(TAttMarker *el, TString branchName)
{
    if (fPar->CheckPar(branchName+"/markerAtt")) {
        auto style = fPar -> GetParStyle(branchName+"/markerAtt",0);
        auto size  = fPar -> GetParSize (branchName+"/markerAtt",1);
        auto color = fPar -> GetParColor(branchName+"/markerAtt",2);
        el -> SetMarkerStyle(style);
        el -> SetMarkerSize(size);
        el -> SetMarkerColor(color);
    }
    if (fPar->CheckPar(branchName+"/markerStyle")) el -> SetMarkerStyle(fPar -> GetParStyle(branchName+"/markerStyle"));
    if (fPar->CheckPar(branchName+"/markerSize"))  el -> SetMarkerSize (fPar -> GetParSize (branchName+"/markerSize" ));
    if (fPar->CheckPar(branchName+"/markerColor")) el -> SetMarkerColor(fPar -> GetParColor(branchName+"/markerColor"));
}

void LKEveTask::SetGraphAtt(TGraph *graph, TString branchName)
{
    if (fPar->CheckPar(branchName+"/lineAtt")) {
        auto lstyle = fPar -> GetParStyle(branchName+"/lineAtt",0);
        auto lcolor = fPar -> GetParWidth(branchName+"/lineAtt",1);
        auto lwidth = fPar -> GetParColor(branchName+"/lineAtt",2);
        graph -> SetLineStyle(lstyle);
        graph -> SetLineWidth(lwidth);
        graph -> SetLineColor(lcolor);
    }
    if (fPar->CheckPar(branchName+"/lineStyle")) graph -> SetLineStyle(fPar -> GetParStyle(branchName+"/lineStyle"));
    if (fPar->CheckPar(branchName+"/lineWidth")) graph -> SetLineWidth(fPar -> GetParWidth(branchName+"/lineWidth"));
    if (fPar->CheckPar(branchName+"/lineColor")) graph -> SetLineColor(fPar -> GetParColor(branchName+"/lineColor"));

    if (fPar->CheckPar(branchName+"/markerAtt")) {
        auto mstyle = fPar -> GetParStyle(branchName+"/markerAtt",0);
        auto msize  = fPar -> GetParSize (branchName+"/markerAtt",1);
        auto mcolor = fPar -> GetParColor(branchName+"/markerAtt",2);
        graph -> SetMarkerStyle(mstyle);
        graph -> SetMarkerSize(msize);
        graph -> SetMarkerColor(mcolor);
    }
    if (fPar->CheckPar(branchName+"/markerStyle")) graph -> SetMarkerStyle(fPar -> GetParStyle(branchName+"/markerStyle"));
    if (fPar->CheckPar(branchName+"/markerSize"))  graph -> SetMarkerSize (fPar -> GetParSize (branchName+"/markerSize" ));
    if (fPar->CheckPar(branchName+"/markerColor")) graph -> SetMarkerColor(fPar -> GetParColor(branchName+"/markerColor"));
}


void LKEveTask::SetGraphAtt(TGraph2D *graph, TString branchName)
{
    if (fPar->CheckPar(branchName+"/lineAtt")) {
        auto lstyle = fPar -> GetParStyle(branchName+"/lineAtt",0);
        auto lcolor = fPar -> GetParWidth(branchName+"/lineAtt",1);
        auto lwidth = fPar -> GetParColor(branchName+"/lineAtt",2);
        graph -> SetLineStyle(lstyle);
        graph -> SetLineWidth(lwidth);
        graph -> SetLineColor(lcolor);
    }
    if (fPar->CheckPar(branchName+"/lineStyle")) graph -> SetLineStyle(fPar -> GetParStyle(branchName+"/lineStyle"));
    if (fPar->CheckPar(branchName+"/lineWidth")) graph -> SetLineWidth(fPar -> GetParWidth(branchName+"/lineWidth"));
    if (fPar->CheckPar(branchName+"/lineColor")) graph -> SetLineColor(fPar -> GetParColor(branchName+"/lineColor"));

    if (fPar->CheckPar(branchName+"/markerAtt")) {
        auto mstyle = fPar -> GetParStyle(branchName+"/markerAtt",0);
        auto msize  = fPar -> GetParSize (branchName+"/markerAtt",1);
        auto mcolor = fPar -> GetParColor(branchName+"/markerAtt",2);
        graph -> SetMarkerStyle(mstyle);
        graph -> SetMarkerSize(msize);
        graph -> SetMarkerColor(mcolor);
    }
    if (fPar->CheckPar(branchName+"/markerStyle")) graph -> SetMarkerStyle(fPar -> GetParStyle(branchName+"/markerStyle"));
    if (fPar->CheckPar(branchName+"/markerSize"))  graph -> SetMarkerSize (fPar -> GetParSize (branchName+"/markerSize" ));
    if (fPar->CheckPar(branchName+"/markerColor")) graph -> SetMarkerColor(fPar -> GetParColor(branchName+"/markerColor"));
}

#ifdef ACTIVATE_EVE
void LKEveTask::ConfigureDisplayWindow()
{
    if (fDetectorSystem -> GetEntries() == 0)
        lk_warning << "Cannot open event display: detector is not set." << endl;

    if (gEve != nullptr) {
        lk_error << "gEve is nullptr" << endl;
        return;
    }

    TEveManager::Create(true, "V");
    auto eveEventManagerGlobal = new TEveEventManager("global");
    fEveEventManagerArray -> Add(eveEventManagerGlobal);
    gEve -> AddEvent(eveEventManagerGlobal);

    {
        Int_t dummy;
        UInt_t w, h;
        UInt_t wMax = 1200;
        UInt_t hMax = 720;
        Double_t r = (Double_t)wMax/hMax;
        gVirtualX -> GetWindowSize(gClient -> GetRoot() -> GetId(), dummy, dummy, w, h);

        if (w > wMax) {
            w = wMax;
            h = hMax;
        } else
            h = (Int_t)(w/r);

        gEve -> GetMainWindow() -> Resize(w, h);
    }

    gEve -> GetDefaultGLViewer() -> SetClearColor(kWhite);

    TGeoNode* geoNode = gGeoManager -> GetTopNode();
    TEveGeoTopNode* topNode = new TEveGeoTopNode(gGeoManager, geoNode, 1, 3, 10000);
    gEve -> AddGlobalElement(topNode);

    gEve -> FullRedraw3D(kTRUE);

    //gEve -> GetDefaultViewer() -> GetGLViewer() -> SetClearColor(kWhite);

    //return;

    /*
       gEve -> GetBrowser() -> SetTabTitle("3D", TRootBrowser::kRight);

       auto slotOv = TEveWindow::CreateWindowInTab(gEve -> GetBrowser() -> GetTabRight()); slotOv -> SetElementName("Overview Slot");
       auto packOv = slotOv -> MakePack(); packOv -> SetElementName("Overview Pack");

    // 1st Row
    auto slotPA = packOv -> NewSlot();
    auto packPA = slotPA -> MakePack();

    // Planes in 1st Row
    packPA -> SetHorizontal();
    for (auto iPlane = 0; iPlane < fDetectorSystem -> GetNumPlanes(); iPlane++) {
    auto slotPlane = packPA -> NewSlot(); slotPlane -> SetElementName(Form("Plane%d Slot", iPlane));
    auto ecvsPlane = new TRootEmbeddedCanvas();
    auto framPlane = slotPlane -> MakeFrame(ecvsPlane); framPlane -> SetElementName(Form("Detector Plane%d Frame", iPlane));

    TCanvas *cvs = ecvsPlane -> GetCanvas();
    fCvsDetectorPlaneArray -> Add(cvs);
    cvs -> cd();
    fDetectorSystem -> GetDetectorPlane(iPlane) -> GetHist(1) -> Draw("col");
    }

    // 2nd Row
    packOv -> SetVertical();
    auto slotCh = packOv -> NewSlotWithWeight(.35); slotCh -> SetElementName("Channel Buffer Slot");
    auto ecvsCh = new TRootEmbeddedCanvas();
    auto frameCh = slotCh -> MakeFrame(ecvsCh); frameCh -> SetElementName("Channel Buffer Frame");
    fCvsChannelBuffer = ecvsCh -> GetCanvas();

    gEve -> GetBrowser() -> GetTabRight() -> SetTab(1);
     */

    gEve -> GetBrowser() -> HideBottomTab();
    gEve -> ElementSelect(gEve -> GetCurrentEvent());
    gEve -> GetWindowManager() -> HideAllEveDecorations();
}

void LKEveTask::AddEveElementToEvent(LKContainer *eveObj, bool permanent)
{
    if (eveObj -> IsEveSet()) {
        TEveElement *eveSet = eveObj -> CreateEveElement();
        eveObj -> AddToEveSet(eveSet, fEveScale);
        AddEveElementToEvent(eveSet, permanent);
    }
    else {
        TEveElement *eveElement = eveObj -> CreateEveElement();
        eveObj -> SetEveElement(eveElement, fEveScale);
        AddEveElementToEvent(eveElement, permanent);
    }
}

void LKEveTask::AddEveElementToEvent(TEveElement *element, bool permanent)
{
    ((TEveEventManager *) fEveEventManagerArray -> At(0)) -> AddElement(element);
    if (permanent) fPermanentEveElementList.push_back(element);
    gEve -> Redraw3D();
}
#endif
