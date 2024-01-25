#ifndef LKDRIFTELECTRONSIM_HH
#define LKDRIFTELECTRONSIM_HH

#include "TObject.h"
#include "LKLogger.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TF1.h"
#include "TF2.h"
#include "TVector3.h"

class LKDriftElectronSim : public TObject
{
    public:
        LKDriftElectronSim();
        virtual ~LKDriftElectronSim() { ; }

        bool Init();
        void Print(Option_t *option="") const;

        TString GetTpcName() const  { return fTpcName; }
        TString GetG4DetName() const  { return fG4DetName; }
        TString GetPadPlaneName() const  { return fPadPlaneName; }
        double GetDriftVelocity() const  { return fDriftVelocity; }
        double GetCoefLongDiff() const  { return fCoefLongDiff; }
        double GetCoefTranDiff() const  { return fCoefTranDiff; }
        double GetIonizationEnergy() const  { return fIonizationEnergy; }
        int GetNumElectronsInCluster() const  { return fNumElectronsInCluster; }
        int GetNumTimeBuckets() const { return fNumTimeBuckets; }
        double GetOffTimeBucket() const { return fOffTimeBucket; }
        double GetTimeBucketTime() const { return fTimeBucketTime; }

        void SetTpcName(TString tpcName) { fTpcName = tpcName; }
        void SetG4DetName(TString g4DetName) { fG4DetName = g4DetName; }
        void SetPadPlaneName(TString padPlaneName) { fPadPlaneName = padPlaneName; }
        void SetDriftVelocity(double driftVelocity) { fDriftVelocity = driftVelocity; }
        void SetIonizationEnergy(double ionizationEnergy) { fIonizationEnergy = ionizationEnergy; }
        void SetNumElectronsInCluster(int numElectronsInCluster) { fNumElectronsInCluster = numElectronsInCluster; }
        void SetNumTimeBuckets(int numTimeBuckets) { fNumTimeBuckets = numTimeBuckets; }
        void SetOffTimeBucket(double offTimeBucket) { fOffTimeBucket = offTimeBucket ; }
        void SetTimeBucketTime(double timeBucketTime) { fTimeBucketTime = timeBucketTime; }

        void SetGasDiffusion(double valueLongDiff, double valueTranDiff)
        {
            fGasDiffusionType = 0;
            fCoefLongDiff = valueLongDiff;
            fCoefTranDiff = valueTranDiff;
        }
        void SetGasDiffusionFunc(TH1D *histLongDiff, TH1D*histTranDiff)
        {
            fGasDiffusionType = 1;
            fGasLongDiffHist = histLongDiff;
            fGasTranDiffHist = histTranDiff;
        }
        void SetGasDiffusionFunc(TF1 *funcLongDiff, TF1*funcTranDiff)
        {
            fGasDiffusionType = 2;
            fGasLongDiffFunc = funcLongDiff;
            fGasTranDiffFunc = funcTranDiff;
        }

        void SetCARDiffusion(double value)   { fCARDiffusionType = 0; fCARDiffusionSigma = value; }
        void SetCARDiffusionHist(TH2D *hist) { fCARDiffusionType = 1; fCARDiffusionHist = hist; }
        void SetCARDiffusionFunc(TF2 *func)  { fCARDiffusionType = 2; fCARDiffusionFunc = func; }

        void SetCARGain(double value)   { fCARGainType = 0; fCARGain = value; }
        void SetCARGainHist(TH1D *hist) { fCARGainType = 1; fCARGainHist = hist; }
        void SetCARGainFunc(TF1 *func)  { fCARGainType = 2; fCARGainFunc = func; }

        /// Set number of electrons from currenct energy deposit
        void SetNumElectrons(double edep);
        int GetNextElectronBunch();

        int GetTimeBucket(double driftLength);

        /// return (dx,dy,dl) where dx, dy and dl are diffusion in x, y and length-axis
        TVector3 CalculateGasDiffusion(double driftLength); 

        /// return (dx,dy,ac) where dx, dy and ac are diffusion in x, y and amplified charge
        TVector3 CalculateCARDiffusion(double x, double y, double initialCharge);

    private:
        TString      fTpcName;
        TString      fG4DetName;
        TString      fPadPlaneName;

        double       fTbTime = 0;
        double       fDriftVelocity = 0;
        double       fCoefLongDiff = 0;
        double       fCoefTranDiff = 0;
        double       fIonizationEnergy = 0;
        int          fNumElectronsInCluster = 0;
        int          fNumTimeBuckets = 0;
        double       fOffTimeBucket  = 512;
        double       fTimeBucketTime = 20;

        int          fNumElectronsFromEdep = 0;

        int          fGasDiffusionType = 0; ///< 0: value, 2: function
        TH1D*        fGasLongDiffHist = nullptr;
        TH1D*        fGasTranDiffHist = nullptr;
        TF1*         fGasLongDiffFunc = nullptr;
        TF1*         fGasTranDiffFunc = nullptr;

        int          fCARDiffusionType = 0; ///< 0: value, 1: hist, 2: function
        double       fCARDiffusionSigma = 0;
        TH2D*        fCARDiffusionHist = nullptr;
        TF2*         fCARDiffusionFunc = nullptr;

        int          fCARGainType = 0; ///< 0: value, 1: hist, 2: function
        double       fCARGain = 0;
        TH1D*        fCARGainHist = nullptr;
        TF1*         fCARGainFunc = nullptr;

    ClassDef(LKDriftElectronSim,1);
};

#endif
