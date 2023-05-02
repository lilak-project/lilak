#include "TEveViewer.h"
#include "TGLViewer.h"
#include "TEveGeoNode.h"
#include "TEveManager.h"
#include "TEveScene.h"
#include "TEveWindow.h"
#include "TEveWindowManager.h"
#include "TEveGedEditor.h"
#include "TEveBrowser.h"
#include "TRootBrowser.h"
#include "TBrowser.h"
#include "TGTab.h"
#include "TVirtualX.h"
#include "TGWindow.h"
#include "TGeoManager.h"
#include "TRootEmbeddedCanvas.h"
#include "TEvePointSet.h"
#include "TEveLine.h"
#include "TEveArrow.h"

#include "LKLogger.hpp"
#include "LKRun.hpp"
#include "LKEveTask.hpp"

ClassImp(LKEveTask)

LKEveTask::LKEveTask()
:LKTask("LKEveTask","")
{
}

bool LKEveTask::Init()
{
  ConfigureEventDisplay

  return true;
}

void LKEveTask::Exec(Option_t*)
{
}

void LKEveTask::RunEve(Long64_t eveEventID, TString option)
{
  if (option.Index("e")<0 && option.Index("p")<0) {
    if (fEveOption.IsNull())
      fEveOption = "ep";
  }
  else
    fEveOption = option;

  Bool_t drawEve3D = (fEveOption.Index("e")>=0) ? true : false;
  Bool_t drawDetectorPlanes = (fEveOption.Index("p")>=0) ? true : false;

  if (eveEventID>=0 && fCurrentEventID!=eveEventID)
    Event(eveEventID);

  fSelTrkIDs    = fPar -> GetParVInt("eveSelectTrackIDs");
  fIgnTrkIDs    = fPar -> GetParVInt("eveIgnoreTrackIDs");
  fSelPntIDs    = fPar -> GetParVInt("eveSelectTrackParentIDs");
  fIgnPntIDs    = fPar -> GetParVInt("eveIgnoreTrackParentIDs");
  fSelPDGs      = fPar -> GetParVInt("eveSelectTrackPDGs");
  fIgnPDGs      = fPar -> GetParVInt("eveIgnoreTrackPDGs");
  fSelMCIDs     = fPar -> GetParVInt("eveSelectMCIDs");
  fIgnMCIDs     = fPar -> GetParVInt("eveIgnoreMCIDs");
  fSelHitPntIDs = fPar -> GetParVInt("eveSelectHitParentIDs");
  fIgnHitPntIDs = fPar -> GetParVInt("eveIgnoreHitParentIDs");
  fSelBranchNames = fPar -> GetParVString("eveSelectBranches");

  fNumSelectedBranches = fSelBranchNames.size();
  if (fNumSelectedBranches==0) {
    fNumSelectedBranches = fNumBranches;
    for (auto iBranch=0; iBranch<fNumSelectedBranches; ++iBranch)
      fSelBranchNames.push_back(fBranchNames[iBranch]);
  }
  else {
    vector<TString> tempBranchNames;
    for (auto iBranch=0; iBranch<fNumSelectedBranches; ++iBranch) {
      TString branchName = fSelBranchNames.at(iBranch);
      if (fBranchPtrMap[branchName] == nullptr) {
        kb_error << "No eve-branch name " << branchName << endl;
        continue;
      }
      tempBranchNames.push_back(branchName);
    }
    fSelBranchNames.clear();
    for (auto branchName : tempBranchNames)
      fSelBranchNames.push_back(branchName);
  }

  if (drawEve3D) DrawEve3D();
  if (drawDetectorPlanes) DrawDetectorPlanes();
}

void LKEveTask::ConfigureEventDisplay()
{
  if (fDetectorSystem -> GetEntries() == 0)
    kb_warning << "Cannot open event display: detector is not set." << endl;

  if (gEve != nullptr) {
    kb_error << "gEve is nullptr" << endl;
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

void LKEveTask::DrawEve3D()
{
  if (gEve!=nullptr) {
    auto numEveEvents = fEveEventManagerArray -> GetEntries();
    for (auto iEveEvent=0; iEveEvent<numEveEvents; ++iEveEvent) {
      ((TEveEventManager *) fEveEventManagerArray -> At(iEveEvent)) -> RemoveElements();
    }
  }

  if (gEve == nullptr)
    ConfigureEventDisplay();

  bool removePointTrack = (fPar->CheckPar("eveRemovePointTrack")) ? (fPar->GetParBool("eveRemovePointTrack")) : false;

  for (Int_t iBranch = 0; iBranch < fNumSelectedBranches; ++iBranch)
  {
    TString branchName = fSelBranchNames.at(iBranch);
    auto branch = (TObjArray *) fBranchPtrMap[branchName];
    if (branch == nullptr) {
      kb_error << "No eve-branch name " << branchName << endl;
      continue;
    }
    if (branch -> GetEntries() == 0)
      continue;

    auto objSample = branch -> At(0);
    if (objSample -> InheritsFrom("KBContainer") == false)
      continue;

    bool isTracklet = (objSample -> InheritsFrom("KBTracklet")) ? true : false;
    bool isHit = (objSample -> InheritsFrom("KBHit")) ? true : false;

    KBContainer *eveObj = (KBContainer *) objSample;
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
        KBTracklet *tracklet = (KBTracklet *) branch -> At(iObject);

        if (removePointTrack)
          if (tracklet -> InheritsFrom("KBMCTrack"))
            if (((KBMCTrack *) tracklet) -> GetNumVertices() < 2)
              continue;

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
        eveObj = (KBContainer *) branch -> At(iObject);

        if (isHit)  {
          KBHit *hit = (KBHit *) branch -> At(iObject);
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
        eveObj = (KBContainer *) branch -> At(iObject);
        auto eveElement = eveObj -> CreateEveElement();
        eveObj -> SetEveElement(eveElement, fEveScale);
        TString name = Form("%s_%d",eveElement -> GetElementName(),iObject);
        eveElement -> SetElementName(name);
        eveEvent -> AddElement(eveElement);
        numSelected++;
      }
    }

    kb_info << "Drawing " << branchName << " [" << branch -> At(0) -> ClassName() << "] " << numSelected << "(" << branch -> GetEntries() << ")" << endl;
  }

  if (fPar->CheckPar("eveAxisOrigin")) {
    Double_t length = 100.;
    if (fPar->CheckPar("eveAxisLength"))
      length = fPar->GetParDouble("eveAxisLength");
    TVector3 origin = fPar -> GetParV3("eveAxisOrigin");
    for (auto kaxis : {KBVector3::kX,KBVector3::kY,KBVector3::kZ}) {
      KBVector3 direction(0,0,0);
      direction.AddAt(length,kaxis);
      auto axis = new TEveArrow(direction.X(),direction.Y(),direction.Z(),origin.X(),origin.Y(),origin.Z()); // TODO
      axis -> SetElementName(KBVector3::AxisName(KBVector3::kX) + " axis");
      if (kaxis==KBVector3::kX) axis -> SetMainColor(kRed);
      if (kaxis==KBVector3::kY) axis -> SetMainColor(kBlue);
      if (kaxis==KBVector3::kZ) axis -> SetMainColor(kBlack);
      ((TEveEventManager *) fEveEventManagerArray -> At(0)) -> AddElement(axis);
    }
  }

  gEve -> Redraw3D();
}

void LKEveTask::DrawDetectorPlanes()
{
  ConfigureDetectorPlanes();

  if (fGraphChannelBoundaryNb[0] == nullptr) { // TODO
    for (Int_t iGraph = 0; iGraph < 20; ++iGraph) {
      fGraphChannelBoundaryNb[iGraph] = new TGraph();
      fGraphChannelBoundaryNb[iGraph] -> SetLineColor(kGreen);
      fGraphChannelBoundaryNb[iGraph] -> SetLineWidth(2);
    }
  }

  auto hitArray = (TClonesArray *) fBranchPtrMap[TString("Hit")];
  auto padArray = (TClonesArray *) fBranchPtrMap[TString("Pad")];

  auto ppHistMin = 0.01;
  if (fPar->CheckPar("evePPHistMin"))
    ppHistMin = fPar -> GetParDouble("evePPHistMin");

  auto numPlanes = fDetectorSystem -> GetNumPlanes();
  for (auto iPlane = 0; iPlane < numPlanes; ++iPlane)
  {
    auto plane = fDetectorSystem -> GetDetectorPlane(iPlane);
    kb_info << "Drawing " << plane -> GetName() << endl;

    auto histPlane = plane -> GetHist();
    histPlane -> SetMinimum(ppHistMin);
    histPlane -> Reset();

    auto cvs = (TCanvas *) fCvsDetectorPlaneArray -> At(iPlane);

    if (plane -> InheritsFrom("KBPadPlane"))
    {
      auto padplane = (KBPadPlane *) plane;

      bool exist_hit = false;
      bool exist_pad = false;

      if (hitArray != nullptr)
        exist_hit = true;
      else {
        for (Int_t iBranch = 0; iBranch < fNumSelectedBranches; ++iBranch)
        {
          TString branchName = fSelBranchNames.at(iBranch);
          if (branchName.Index("Hit")==0) {
            kb_info << branchName << " is to be filled to pad plane" << endl;
            hitArray = (TClonesArray *) fBranchPtrMap[branchName];
            hitArray -> Print();
            exist_hit = true;
            break;
          }
        }
      }
      if (padArray != nullptr)
        exist_pad = true;

      if (!exist_hit && !exist_pad) {
        cvs -> cd();
        histPlane -> Draw();
        plane -> DrawFrame();
        continue;
      }

      padplane -> Clear();
      if (exist_hit) padplane -> SetHitArray(hitArray);
      if (exist_pad) padplane -> SetPadArray(padArray);

      if (fPar -> CheckPar("evePPFillOption"))
      {
        auto fillOption = fPar -> GetParString("evePPFillOption");
        kb_info << "Filling " << fillOption << " to PadPlane" << endl;
        padplane -> FillDataToHist(fillOption);
      }
      else if (exist_hit)
      {
        kb_info << "Filling Hits to PadPlane" << endl;
        padplane -> FillDataToHist("hit");
      }
      else if (exist_pad)
      {
        kb_info << "Filling Pads to PadPlane" << endl;
        padplane -> FillDataToHist("out");
      }
    }

    cvs -> Clear();
    cvs -> cd();
    histPlane -> DrawClone("colz");
    histPlane -> Reset();
    histPlane -> Draw("same");
    plane -> DrawFrame();

    KBVector3::Axis axis1 = plane -> GetAxis1();
    KBVector3::Axis axis2 = plane -> GetAxis2();

    for (Int_t iBranch = 0; iBranch < fNumSelectedBranches; ++iBranch)
    {
      TClonesArray *branch = nullptr;
      if (fNumSelectedBranches != 0) {
        TString branchName = fSelBranchNames.at(iBranch);
        //TString branchName = ((TObjString *) eveBranchNames -> At(iBranch)) -> GetString();
        branch = (TClonesArray *) fBranchPtrMap[branchName];
      }
      else
        branch = (TClonesArray *) fBranchPtr[iBranch];

      TObject *objSample = nullptr;

      Int_t numTracklets = branch -> GetEntries();
      if (numTracklets != 0) {
        objSample = branch -> At(0);
        if (objSample -> InheritsFrom("KBContainer") == false || objSample -> InheritsFrom("KBTracklet") == false)
          continue;
      }
      else
        continue;

      auto trackletSample = (KBTracklet *) objSample;
      if (trackletSample -> DoDrawOnDetectorPlane())
      {
        for (auto iTracklet = 0; iTracklet < numTracklets; ++iTracklet) {
          auto tracklet = (KBTracklet *) branch -> At(iTracklet);
          if (!SelectTrack(tracklet))
            continue;

          tracklet -> TrajectoryOnPlane(axis1, axis2) -> Draw("samel");
        }
      }
    }
  }

  // @todo palette is changed when drawing top node because of TGeoMan(?)
  gStyle -> SetPalette(kBird);
}

void LKEveTask::AddEveElementToEvent(KBContainer *eveObj, bool permanent)
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
