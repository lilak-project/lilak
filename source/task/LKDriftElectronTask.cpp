#include "LKDriftElectronTask.h"

ClassImp(LKDriftElectronTask);

LKDriftElectronTask::LKDriftElectronTask()
{
    fName = "LKDriftElectronTask";
}

bool LKDriftElectronTask::Init()
{
    lk_info << "Initializing LKDriftElectronTask" << std::endl;

    fPar -> Require("LKDriftElectronTask/Wvalue",        "41.0",   "W value of mixture gas [eV]");
    fPar -> Require("LKDriftElectronTask/DriftVelocity", "55.0",   "Drift velocity in TPC active area [mm/ns]");
    fPar -> Require("LKDriftElectronTask/DiffusionT",    "1.0",    "Transverse diffusion [mm/sqrt{mm}]");
    fPar -> Require("LKDriftElectronTask/DiffusionL",    "1.0",    "Longitudinal diffusion [mm/sqrt{mm}]");
    fPar -> Require("LKDriftElectronTask/TBTime",        "10.0",   "Time bucket unit [ns]");

    if(fPar->CheckPar("LKDriftElectronTask/Wvalue")){
        fWvalue = fPar->GetParDouble("LKDriftElectronTask/Wvalue");
    }
    if(fPar->CheckPar("LKDriftElectronTask/DriftVelocity")){
        fDriftVelocity = fPar->GetParDouble("LKDriftElectronTask/DriftVelocity");
    }
    if(fPar->CheckPar("LKDriftElectronTask/DiffusionT")){
        fDiffusionT = fPar->GetParDouble("LKDriftElectronTask/DiffusionT");
    }
    if(fPar->CheckPar("LKDriftElectronTask/DiffusionL")){
        fDiffusionL = fPar->GetParDouble("LKDriftElectronTask/DiffusionL");
    }
    if(fPar->CheckPar("LKDriftElectronTask/TBTime")){
        fTBTime = fPar->GetParDouble("LKDriftElectronTask/TBTime");
    }
    if(fPar->CheckPar("LKDriftElectronTask/AvalancheDataPath")){
        fAvalancheDataPath = fPar->GetParString("LKDriftElectronTask/AvalancheDataPath");
    }
    else{
        fAvalancheDataPath = "Default";
    }

    fRandom = new TRandom3();

    int branchNum = fRun -> GetNumBranches();
    TString stepBranchName = "";
    for (int b=0; b<branchNum; b++) 
    {
        TString branchName = fRun -> GetBranchName(b);
        if(branchName.Index("MCStep") != -1){
            stepBranchName = branchName;
        }
    }
    if(stepBranchName == ""){
        lk_error << "No simulation input" <<std::endl;
        return false;
    }

    fStepArray = fRun -> GetBranchA(stepBranchName);
    fChannelArray = fRun -> RegisterBranchA("RawPad", "GETChannel");
    fMCTagArray = fRun -> RegisterBranchA("MCTag", "LKMCTag");

    fDetector = fRun -> GetDetector();

    if (!InitAvalancheFunction()) return false;

    return true;
}

void LKDriftElectronTask::Exec(Option_t *option)
{
    fRandom -> SetSeed(time(0));

    fChannelArray -> Clear("C");
    fMCTagArray -> Clear("C");

    int stepNum = fStepArray -> GetEntries();
    for (int i=0; i<stepNum; i++)
    {
        fStep = (LKMCStep*)fStepArray -> UncheckedAt(i);

        fStepPos.SetXYZ(fStep->GetX(), fStep->GetY(), fStep->GetZ());

        bool isIn = fDetector -> IsInBoundary(fStepPos.X(), fStepPos.Y(), fStepPos.Z()); // Note: Geant global position
        if (!isIn) continue;

        bool findDriftPlane = false;
        for (int plane=0; plane<fDetector->GetNumPlanes(); plane++)
        {
            fDetectorPlane = fDetector->GetDetectorPlane(plane);
            fStepPos = fDetectorPlane -> GlobalToLocalAxis(fStepPos);

            bool isInPlane = fDetectorPlane -> IsInBoundary(fStepPos.X(), fStepPos.Y());
            if (isInPlane) {
                findDriftPlane = true;
                break;
            }
        }
        if (!findDriftPlane) continue;

        double energy = fStep->GetEdep() * 1000000.; // [eV]
        int clusterNum = int(energy/fWvalue) + 1;
        for (int e=0; e<clusterNum; e++)
        {   
            int tb = 0;
            int w = 1.;

            bool isDrift = DriftElectron(fStepPos, fStep->GetTime(), fDriftPos, tb); // detector plane axis coordinate
            if (!isDrift) continue;
            
            w = w * GetAvalancheElectron();

            int chanID = fDetectorPlane -> FindChannelID(fDriftPos.X(), fDriftPos.Y());
            fChannel = (GETChannel*)fDetectorPlane -> GetChannel(chanID);
            fChannel->GetBufferArray()[tb] = w;

            // fMCTag = (LKMCTag*)fDetectorPlane -> GetMCTag(padID); // LKDetectorPlane has not GetMCTag yet!
            // fMCTag -> AddMCTag(fStep->GetTrackID(), tb);
        }
    }

    TIter itChannel(fDetectorPlane -> GetChannelArray());
    int idx = 0;
    while ((fChannel = (GETChannel*)itChannel.Next())) {    
        // fMCTag = (LKMCTag*)fDetectorPlane -> GetMCTag(idx); // LKDetectorPlane has not GetMCTag yet!
        fChannel -> Copy(*(GETChannel*)fChannelArray -> ConstructedAt(idx));
        // fMCTag -> Copy(*(LKMCTag*)fMCTagArray -> ConstructedAt(idx)); // LKDetectorPlane has not GetMCTag yet!
        idx++;
    }
}

bool LKDriftElectronTask::EndOfRun()
{
    return true;
}

bool LKDriftElectronTask::InitAvalancheFunction()
{
    if (fAvalancheDataPath == "Default") 
    {
        // Gain fluctuation distribution based on Polya distribution
        TF1* gain = new TF1("function", this, &LKDriftElectronTask::PolyaDistribution, 0., 50000., 2);
        gain -> SetParameter(0, 1.5); // M, See the STAR TPC gain fluctuation
        gain -> SetParameter(1, 6000); // Intrincsic gain

        fAvalancheFunction = (TH1D*)gain -> GetHistogram();
        if (!fAvalancheFunction) return false;
        return true;
    }

    TFile* file = new TFile(fAvalancheDataPath, "READ");
    fAvalancheFunction = (TH1D*)file -> Get("GainFunction");
    if (!fAvalancheFunction) return false;

    return true;
}

bool LKDriftElectronTask::DriftElectron(TVector3 startPos, Double_t startT, TVector3& endPos, Int_t& tb)
{
    double drfitLength = startPos.Z(); // [mm]
    double sigmaT = fDiffusionT * sqrt(drfitLength); // [mm]
    double sigmaL = fDiffusionL * sqrt(drfitLength); // [mm]

    double dr = fRandom -> Gaus(0., sigmaT);
    double dt = fRandom -> Gaus(0, sigmaL) / fDriftVelocity;
    double phi = fRandom -> Uniform(2*TMath::Pi());

    double dx = dr * cos(phi);
    double dy = dr * sin(phi);

    endPos.SetX(startPos.X() + dx);
    endPos.SetY(startPos.Y() + dy);
    endPos.SetZ(0.);

    double t = startT + dt;
    tb = t/fTBTime;

    if (tb < 0) tb = 0;
    if (tb >= 512) tb = 511;

    return true;
}

double LKDriftElectronTask::GetAvalancheElectron()
{
    weight = fAvalancheFunction -> GetRandom();
    return true;
}

Double_t LKDriftElectronTask::PolyaDistribution(Double_t* x, Double_t* par)
{
    Double_t value = par[0]*pow(par[0]*(x[0]/par[1]), par[0]-1.)/TMath::Gamma(par[0]) * exp(-1. * par[0]*(x[0]/par[1]));
    return value;
}