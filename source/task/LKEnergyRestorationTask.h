#ifndef LKENERGYRESTORATIONTASK_HH
#define LKENERGYRESTORATIONTASK_HH

#include "LKTask.h"
#include "TClonesArray.h"
#include "TString.h"

#include <array>
#include <map>
#include <tuple>
#include <vector>

class LKSiChannel;
class SKSiHit;

class LKEnergyRestorationTask : public LKTask
{
    public:
        LKEnergyRestorationTask();
        virtual ~LKEnergyRestorationTask() {}

        bool Init();
        void Exec(Option_t *);

    private:
        using StripKey = std::tuple<int,int,int>;
        using LRKey = std::tuple<int,int,int,int>;

        // Repeated post-calibration gain rules: last matching rule wins, default 1.
        // Detector: detNum side gain; DetID: mapping detIndex side gain;
        // CAAC: cobo asad aget channel gain. Address/side -1 is a wildcard.
        // Paired strips use direction-0 channel to select one gain for both ends;
        // the calibrated RelativeZ is preserved.
        struct GainRule {
            int kind = 0; // 0 detector number, 1 detector index, 2 CAAC
            std::array<int,4> address {{-1,-1,-1,-1}};
            double gain = 1;
        };
        std::vector<GainRule> fGainRules; //!
        bool LoadGainRules();
        double GetPostCalibrationGain(const LKSiChannel *channel) const;
        bool LoadEnergyCalibrationFile(TString fileName);
        bool HasC0(const StripKey &key) const;
        bool HasC1(const StripKey &key) const;
        bool HasC2(const StripKey &key) const;
        bool HasC3(const StripKey &key) const;
        double ApplyLinear(const std::array<double,2> &par, double value) const;
        bool ApplyStandalone(LKSiChannel *channel);
        bool ApplyPaired(LKSiChannel *channel);
        void AddSiHit(LKSiChannel *channel, double energy, double relativeZ=-999);

    private:
        TClonesArray *fSiChannelArray = nullptr;
        TClonesArray *fHitArray = nullptr;
        TString fEnergyCalibrationFileName = "";
        double fC2ReferenceEnergy = 5.486;
        bool fApplyToPairChannel = true;
        bool fBuildSiHit = true;

        std::map<StripKey, std::array<double,2>> fC0Parameters;
        std::map<LRKey, std::array<double,2>> fC1Parameters;
        std::map<StripKey, std::array<double,3>> fC2Parameters;
        std::map<StripKey, std::array<double,2>> fC3Parameters;

    ClassDef(LKEnergyRestorationTask, 2)
};

#endif
