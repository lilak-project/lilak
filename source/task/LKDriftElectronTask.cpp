#include "LKDriftElectronTask.h"
#include "LKMCStep.h"
#include "TString.h"

ClassImp(LKDriftElectronTask);

LKDriftElectronTask::LKDriftElectronTask()
{
    fName = "LKDriftElectronTask";
}

bool LKDriftElectronTask::Init()
{
    lk_info << "Initializing LKDriftElectronTask" << std::endl;

    fNumTPCs = fPar -> GetParInt("LKDriftElectronTask/numTPCs");
    for (auto iTPC=0; iTPC<fNumTPCs; ++iTPC)
    {
        fMCStepArray[iTPC] = nullptr;
        fPadArray[iTPC] = nullptr;
        fDetectorPlane[iTPC] = nullptr;
        fDriftElectronSim[iTPC] = nullptr;

        TString tpcName = fPar -> GetParString("LKDriftElectronTask/tpcNames",iTPC);
        TString g4DetName = fPar -> GetParString(tpcName+"/G4DetectorName");
        TString padPlaneName = fPar -> GetParString(tpcName+"/padPlaneName");
        TString outputBranchName = fPar -> GetParString(tpcName+"/outputBranchName");

        lk_info << "Initializing LKDriftElectronTask >> " << tpcName << " : " << g4DetName << " " << padPlaneName << " " << outputBranchName << std::endl;

        double driftVelocity = fPar -> GetParDouble(tpcName+"/driftVelocity");
        double ionizationEnergy = fPar -> GetParDouble(tpcName+"/ionizationEnergy");
        int numElectronsInCluster = fPar -> GetParInt(tpcName+"/numElectronsInCluster");

        int offTimeBucket = fPar -> GetParInt(tpcName+"/offTimeBucket");
        int numTimeBuckets = fPar -> GetParInt(tpcName+"/numTimeBuckets");
        double timeBucketTime = fPar -> GetParInt(tpcName+"/timeBucketTime");

        //fMCStepArray[iTPC] = (TClonesArray *) fRun -> GetBranch(Form("MCStep_%s",g4DetName.Data()));
        fMCStepArray[iTPC] = (TClonesArray *) fRun -> GetBranchA(Form("MCStep_%s",g4DetName.Data()));
        fDetectorPlane[iTPC] = fRun -> FindDetectorPlane(padPlaneName);
        fDriftElectronSim[iTPC] = new LKDriftElectronSim();
        auto sim = fDriftElectronSim[iTPC];
        sim -> SetDriftVelocity(driftVelocity);
        sim -> SetIonizationEnergy(ionizationEnergy);
        sim -> SetNumElectronsInCluster(numElectronsInCluster);
        sim -> SetOffTimeBucket(offTimeBucket);
        sim -> SetNumTimeBuckets(numTimeBuckets);
        sim -> SetTimeBucketTime(timeBucketTime);

        TString gasDiffusion = fPar -> GetParString(tpcName+"/gasDiffusion",0);
        if (gasDiffusion=="value") {
            double valueLD = fPar -> GetParDouble(tpcName+"/gasDiffusion",1);
            double valueTD = fPar -> GetParDouble(tpcName+"/gasDiffusion",2);
            sim -> SetGasDiffusion(valueLD,valueTD);
        }
        if (gasDiffusion.Index("hist")==0) {
            fPar -> GetParDouble(tpcName+"/gasDiffusion",1);
            TString diffValueFile = fPar -> GetParString(tpcName+"/gasDiffusion",1);
            TFile* file = new TFile(diffValueFile,"read");
            auto funcLD = (TF1*) file -> Get("longDiff");
            auto funcTD = (TF1*) file -> Get("tranDiff");
            sim -> SetGasDiffusionFunc(funcLD, funcTD);
        }
        if (gasDiffusion.Index("func")==0) {
            fPar -> GetParDouble(tpcName+"/gasDiffusion",1);
            TString diffValueFile = fPar -> GetParString(tpcName+"/gasDiffusion",1);
            TFile* file = new TFile(diffValueFile,"read");
            auto funcLD = (TF1*) file -> Get("longDiff");
            auto funcTD = (TF1*) file -> Get("tranDiff");
            sim -> SetGasDiffusionFunc(funcLD, funcTD);
        }

        TString carDiffusion = fPar -> GetParString(tpcName+"/CARDiffusion",0);
        if (carDiffusion=="value") {
            double diffValue = fPar -> GetParDouble(tpcName+"/CARDiffusion",1);
            sim -> SetCARDiffusion(diffValue);
        }
        if (carDiffusion.Index("hist")==0) {
            TString gainValueFile = fPar -> GetParString(tpcName+"/CARDiffusion",1);
            TFile* file = new TFile(gainValueFile,"read");
            auto diffHist = (TH2D*) file -> Get("diffusion");
            sim -> SetCARDiffusionHist(diffHist);
        }
        if (carDiffusion.Index("func")==0) {
            TString gainValueFile = fPar -> GetParString(tpcName+"/CARDiffusion",1);
            TFile* file = new TFile(gainValueFile,"read");
            auto diffFunc = (TF2*) file -> Get("diffusion");
            sim -> SetCARDiffusionFunc(diffFunc);
        }

        TString carGain = fPar -> GetParString(tpcName+"/CARGain",0);
        if (carGain=="value") {
            double gainValue = fPar -> GetParDouble(tpcName+"/CARGain",1);
            sim -> SetCARGain(gainValue);
        }
        if (carGain.Index("hist")==0) {
            TString gainValueFile = fPar -> GetParString(tpcName+"/CARGain",1);
            TFile* file = new TFile(gainValueFile,"read");
            auto gainHist = (TH1D*) file -> Get("gain");
            sim -> SetCARGainHist(gainHist);
        }
        if (carGain.Index("func")==0) {
            TString gainValueFile = fPar -> GetParString(tpcName+"/CARGain",1);
            TFile* file = new TFile(gainValueFile,"read");
            auto gainFunction = (TF1*) file -> Get("gain");
            sim -> SetCARGainFunc(gainFunction);
        }

        //fPadArray[iTPC] = new TClonesArray("LKPad");
        //fRun -> RegisterBranch(outputBranchName, fPadArray[iTPC]);
        auto array = fRun -> RegisterBranchA(outputBranchName, "LKPad", 500);
        fPadArray[iTPC] = array;
    }

    return true;
}

void LKDriftElectronTask::Exec(Option_t *option)
{
    for (auto iTPC=0; iTPC<fNumTPCs; ++iTPC)
    {
        auto mcArray = fMCStepArray[iTPC];
        auto padArray = fPadArray[iTPC];
        auto padPlane = fDetectorPlane[iTPC];
        auto sim = fDriftElectronSim[iTPC];

        padArray -> Clear("C");
        padPlane -> Clear();

        /////////////////////////////////////////////////////////////////
        // Fill electrons into pad plane
        /////////////////////////////////////////////////////////////////
        Long64_t numMCSteps = mcArray -> GetEntries();
        for (Long64_t iStep = 0; iStep < numMCSteps; ++iStep)
        {
            LKMCStep* step = (LKMCStep*) mcArray -> At(iStep);
            Int_t trackID = step -> GetTrackID();
            Double_t edep = step -> GetEdep();
            TVector3 posMC(step -> GetX(), step -> GetY(), step -> GetZ());

            TVector3 posFinal;
            double driftLength;
            padPlane -> DriftElectron(posMC, posFinal, driftLength);
            TVector3 xyl(posFinal.X(),posFinal.Y(),driftLength);
            auto xylDiff = sim -> CalculateGasDiffusion(driftLength);
            auto xylGas = xyl + xylDiff;

            sim -> SetNumElectrons(edep);
            while(int numElectrons = sim -> GetNextElectronBunch())
            {
                if (numElectrons==0)
                    break;

                auto xPlane = xylGas.X();
                auto yPlane = xylGas.Y();

                auto tb = sim -> GetTimeBucket(xylGas.Z());
                auto xyc = sim -> CalculateCARDiffusion(xPlane, yPlane, numElectrons);

                xPlane = xPlane + xyc.X();
                yPlane = yPlane + xyc.Y();
                auto charge = xyc.Z();
                padPlane -> FillPlane(xPlane, yPlane, tb, charge, trackID);
            }
        }
        /////////////////////////////////////////////////////////////////


        /////////////////////////////////////////////////////////////////
        // Write active pad to output file
        /////////////////////////////////////////////////////////////////
        Int_t idx = 0;
        LKPad *pad;
        TIter itChannel(padPlane -> GetChannelArray());
        while ((pad = (LKPad *) itChannel.Next())) {
            if (pad -> IsActive() == false)
                continue;
            auto padSave = (LKPad*) padArray -> ConstructedAt(idx);
            padSave -> SetPad(pad);
            padSave -> CopyPadData(pad);
            idx++;
        }
        /////////////////////////////////////////////////////////////////

        lk_info << "Number of fired pads in plane-" << iTPC << ": " << padArray -> GetEntries() << endl;
    }
}

bool LKDriftElectronTask::EndOfRun()
{
    return true;
}
