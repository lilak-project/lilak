#include "LKDriftElectronSim.h"
#include "TRandom.h"

ClassImp(LKDriftElectronSim);

LKDriftElectronSim::LKDriftElectronSim()
{
    ;
}

bool LKDriftElectronSim::Init()
{
    // Put intialization todos here which are not iterative job though event
    e_info << "Initializing LKDriftElectronSim" << std::endl;

    fTbTime = 0;
    fDriftVelocity = 0;
    fCoefLongDiff = 0;
    fCoefTranDiff = 0;
    fIonizationEnergy = 0;
    fNumElectronsInCluster = 0;
    fNumElectronsFromEdep = 0;
    fGasDiffusionType = 0; ///< 0: value, 2: function
    fGasLongDiffFunc = nullptr;
    fGasTranDiffFunc = nullptr;
    fCARDiffusionType = 0; ///< 0: value, 1: hist, 2: function
    fCARDiffusionSigma = 0;
    fCARDiffusionHist = nullptr;
    fCARDiffusionFunc = nullptr;
    fCARGainType = 0; ///< 0: value, 1: hist, 2: function
    fCARGain = 0;
    fCARGainHist = nullptr;
    fCARGainFunc = nullptr;

    return true;
}

void LKDriftElectronSim::Print(Option_t *option) const
{
    // You will probability need to modify here
    e_info << "LKDriftElectronSim" << std::endl;
    e_info << "fTpcName : " << fTpcName << std::endl;
    e_info << "fG4DetName : " << fG4DetName << std::endl;
    e_info << "fPadPlaneName : " << fPadPlaneName << std::endl;
    e_info << "fTbTime : " << fTbTime << std::endl;
    e_info << "fDriftVelocity : " << fDriftVelocity << std::endl;
    e_info << "fCoefLongDiff : " << fCoefLongDiff << std::endl;
    e_info << "fCoefTranDiff : " << fCoefTranDiff << std::endl;
    e_info << "fIonizationEnergy : " << fIonizationEnergy << std::endl;
    e_info << "fNumElectronsInCluster : " << fNumElectronsInCluster << std::endl;
}

void LKDriftElectronSim::SetNumElectrons(double edep) {
    fNumElectronsFromEdep = int(edep/fIonizationEnergy);
}

int LKDriftElectronSim::GetNextElectronBunch()
{
    if (fNumElectronsFromEdep > fNumElectronsInCluster)
        return fNumElectronsInCluster;

    else if (fNumElectronsFromEdep <= 0)
        return 0;

    return fNumElectronsFromEdep - fNumElectronsInCluster;

    fNumElectronsFromEdep = fNumElectronsFromEdep - fNumElectronsInCluster;
}

int LKDriftElectronSim::GetTimeBucket(double driftLength)
{
    double driftTime = driftLength/fDriftVelocity;
    int tb = int(driftTime/fTimeBucketTime);
    return tb;
}

TVector3 LKDriftElectronSim::CalculateGasDiffusion(double driftLength)
{
    double dx, dy, dt;
    if (fGasDiffusionType==0) {
        double dr = gRandom -> Gaus(0, fCoefTranDiff*sqrt(driftLength));
        double angle = gRandom -> Uniform(2*TMath::Pi());
        dx = dr*TMath::Cos(angle);
        dy = dr*TMath::Sin(angle);
        dt = gRandom -> Gaus(0,fCoefLongDiff*sqrt(driftLength))/fDriftVelocity;
    }
    else if (fGasDiffusionType==2) {
        double dr = fGasLongDiffFunc -> Eval(driftLength);
        double angle = gRandom -> Uniform(2*TMath::Pi());
        dx = dr*TMath::Cos(angle);
        dy = dr*TMath::Sin(angle);
        dt = fGasTranDiffFunc -> Eval(driftLength);
    }
    else if (fGasDiffusionType==2) {
        double dr = fGasLongDiffFunc -> Eval(driftLength);
        double angle = gRandom -> Uniform(2*TMath::Pi());
        dx = dr*TMath::Cos(angle);
        dy = dr*TMath::Sin(angle);
        dt = fGasTranDiffFunc -> Eval(driftLength);
    }

    return TVector3(dx,dy,dt);
}

/// Drift Charge amplification region
TVector3 LKDriftElectronSim::CalculateCARDiffusion(double x, double y, double initialCharge)
{
    double dx, dy, amplifiedCharge;
    if (fCARDiffusionType==0) {
        double dr = gRandom -> Gaus(0, fCARDiffusionSigma);
        double angle = gRandom -> Uniform(2*TMath::Pi());
        dx = dr*TMath::Cos(angle);
        dy = dr*TMath::Sin(angle);
    }
    else if (fCARDiffusionType==1) {
        fCARDiffusionHist -> GetRandom2(dx, dy);
    }
    else if (fCARDiffusionType==2) {
        fCARDiffusionFunc -> GetRandom2(dx, dy);
    }


    if (fCARGainType==0) {
        amplifiedCharge = initialCharge * fCARGain;
    }
    else if (fCARGainType==1) {
        double gain = fCARGainHist -> GetRandom();
        amplifiedCharge = initialCharge * gain;
    }
    else if (fCARGainType==2) {
        double gain = fCARGainFunc -> GetRandom();
        amplifiedCharge = initialCharge * gain;
    }

    TVector3 xyc(dx, dy, amplifiedCharge);
    return xyc;
}
