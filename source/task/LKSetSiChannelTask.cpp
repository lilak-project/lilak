#include "TStyle.h"
#include <cmath>
#include <algorithm>
#include "TFormula.h"

#include "LKRun.h"
#include "LKLogger.h"
#include "GETChannel.h"
#include "LKSiChannel.h"

#include "LKSetSiChannelTask.h"

#include "LKPulseFitData.h"

ClassImp(LKSetSiChannelTask)

LKSetSiChannelTask::LKSetSiChannelTask()
    :LKTask("LKSetSiChannelTask","LKSetSiChannelTask")
{
}

LKSetSiChannelTask::~LKSetSiChannelTask()
{
    delete fSaturationEnergyFormula;
}

bool LKSetSiChannelTask::Init()
{
    fPar -> UpdatePar(fPulserAnalysis,
            "LKSetSiChannelTask/pulser_analysis false # Force inverted pulse polarity and skip saturation processing.");

    fPar->UpdatePar(fPedestalTbFirst, "LKSetSiChannelTask/PedestalTbRange", 0);
    fPar->UpdatePar(fPedestalTbLast, "LKSetSiChannelTask/PedestalTbRange", 1);
    fPar->UpdatePar(fBipolarMinPercent, "LKSetSiChannelTask/BipolarPercentRange", 0);
    fPar->UpdatePar(fBipolarMaxPercent, "LKSetSiChannelTask/BipolarPercentRange", 1);
    fPar->UpdatePar(fBipolarWindow, "LKSetSiChannelTask/BipolarWindow");
    if (fPedestalTbFirst < 0 || fPedestalTbLast < fPedestalTbFirst || fPedestalTbLast >= 512 ||
        !std::isfinite(fBipolarMinPercent) || !std::isfinite(fBipolarMaxPercent) ||
        fBipolarMinPercent <= 0 || fBipolarMaxPercent < fBipolarMinPercent || fBipolarWindow < 1 || fBipolarWindow >= 512) {
        lk_error << "Invalid pedestal or bipolar parameters" << endl;
        return false;
    }

    TString expression = "10000";
    const TString formulaParameter = "LKSetSiChannelTask/SaturationEnergyFormula";
    if (fPar->CheckPar(formulaParameter)) expression = fPar->GetParString(formulaParameter);
    fPar->UpdatePar(fSaturationSlopeWindow, "LKSetSiChannelTask/SaturationSlopeWindow");
    if (fSaturationSlopeWindow < 2 || fSaturationSlopeWindow > 511) {
        lk_error << "SaturationSlopeWindow must be between 2 and 511 TB" << endl;
        return false;
    }
    delete fSaturationEnergyFormula;
    fSaturationEnergyFormula = new TFormula("SiSaturationEnergy", expression, false);
    if (!fSaturationEnergyFormula->IsValid() || fSaturationEnergyFormula->GetNdim() > 1) {
        lk_error << "Invalid " << formulaParameter << ": " << expression << endl;
        return false;
    }
    fSaturationNeedsSlope = fSaturationEnergyFormula->GetNdim() == 1;
    for (int i=0; i<fSaturationEnergyFormula->GetNpar(); ++i) {
        TString name = fSaturationEnergyFormula->GetParName(i);
        if (name == "slope") fSaturationNeedsSlope = true;
        else if (name != "amplitude" && name != "pedestal" && name != "time" && name != "noise") {
            lk_error << "Unknown saturation formula parameter [" << name << "]" << endl;
            return false;
        }
    }

    fSiliconArray = (LKSiliconArray*) fRun -> FindDetectorPlane("LKSiliconArray");
    if (fSiliconArray == nullptr) {
        lk_error << "LKSiliconArray detector plane is not found. Add STARK or LKSiliconArray before LKSetSiChannelTask." << endl;
        return false;
    }

    fRawDataArray = fRun -> GetBranchA("RawData","GETChannel");
    if (fRawDataArray == nullptr)
        return false;

    fSiChannelArray = fRun -> RegisterBranchA("SiChannel","LKSiChannel",20);
    fFitDataArray = fRun -> RegisterBranchA("PFData","LKPulseFitData",10,false);

    TString pulseFileName;
    fPar -> UpdatePar(pulseFileName,"stark/pulseFile");

    fChannelAnalyzer = new LKChannelAnalyzer();
    fChannelAnalyzer->SetPedestalTbRange(fPedestalTbFirst, fPedestalTbLast);
    fChannelAnalyzer->SetBipolarPercentRange(fBipolarMinPercent, fBipolarMaxPercent);
    fChannelAnalyzer->SetBipolarWindow(fBipolarWindow);
    //fChannelAnalyzer -> SetPulse(pulseFileName);
    //fChannelAnalyzer -> Print();

    fChannelAnalyzer2 = new LKChannelAnalyzer();
    fChannelAnalyzer2 -> SetPulse(pulseFileName);
    //fChannelAnalyzer2 -> Print();

    return true;
}

void LKSetSiChannelTask::Exec(Option_t*)
{
    fSiChannelArray -> Clear("C");
    fFitDataArray -> Clear("C");

    int fitDataCount = 0;
    int channelCount = 0;
    auto numChannels = fRawDataArray -> GetEntries();
    for (auto iChannel=0; iChannel<numChannels; ++iChannel)
    {
        auto channel = (GETChannel*) fRawDataArray -> At(iChannel);
        auto siChannel1 = (LKSiChannel*) fSiChannelArray -> ConstructedAt(channelCount);
        siChannel1 -> SetChannelID(channelCount);
        bool good = fSiliconArray -> SetSiChannelData(siChannel1, channel);
        if (!good) {
            fSiChannelArray -> RemoveAt(channelCount);
            continue;
        }

        bool isInverted = fPulserAnalysis || siChannel1->GetSide()==0;

        fChannelAnalyzer -> SetDataIsInverted(isInverted);
        fChannelAnalyzer2 -> SetDataIsInverted(isInverted);
        siChannel1 -> SetInverted(isInverted);

        auto data = channel -> GetWaveformY();
        fChannelAnalyzer -> Analyze(data);
        double noise = fChannelAnalyzer->GetNoiseScale();
        if (fChannelAnalyzer->IsBipolar())
            noise = -std::max(noise, 1.e-12);
        siChannel1->SetNoiseScale(noise);
        channel->SetNoiseScale(noise);
        siChannel1->SetPedestal(fChannelAnalyzer->GetPedestal());
        channel->SetPedestal(fChannelAnalyzer->GetPedestal());
        auto numRecoHits = fChannelAnalyzer -> GetNumHits();
        if (numRecoHits>=1)
        {
            auto pedestal = fChannelAnalyzer -> GetPedestal();
            auto energy = fChannelAnalyzer -> GetAmplitude(0);
            auto time = fChannelAnalyzer -> GetTbHit(0);
            double energy0 = energy;

            bool isSaturated = false;
            int saturatedTb = -1;
            if (!fPulserAnalysis && isInverted)
            {
                for (auto t=0; t<512; ++t) {
                    if (data[t]==0) {
                        isSaturated = true;
                        saturatedTb = t;
                        break;
                    }
                }
            }
            else if (!fPulserAnalysis) {
                for (auto t=0; t<512; ++t) {
                    if (data[t]==4095) {
                        isSaturated = true;
                        saturatedTb = t;
                        break;
                    }
                }
            }

            double saturationSlope = -1;
            int slopeFirst = -1, slopeLast = -1;
            if (isSaturated) {
                energy = kSaturatedEnergy;
                bool canEvaluate = true;
                if (fSaturationNeedsSlope) {
                    const int first = std::max(0, saturatedTb-fSaturationSlopeWindow);
                    const int count = saturatedTb-first;
                    canEvaluate = count >= 2;
                    if (canEvaluate) {
                        // Unweighted straight-line regression of unclipped samples only.
                        // Center x to avoid cancellation and remove pedestal dependence.
                        double xy = 0;
                        for (int j=0; j<count; ++j) {
                            const double y = isInverted ? 4095-data[first+j] : data[first+j];
                            xy += (j-0.5*(count-1))*y;
                        }
                        const double xx = double(count)*(double(count)*count-1)/12.;
                        saturationSlope = xy/xx;
                        slopeFirst = first;
                        slopeLast = saturatedTb-1;
                        canEvaluate = std::isfinite(saturationSlope) && saturationSlope > 0;
                    }
                }
                if (canEvaluate) {
                    for (int i=0; i<fSaturationEnergyFormula->GetNpar(); ++i) {
                        TString name = fSaturationEnergyFormula->GetParName(i);
                        double value = name == "slope" ? saturationSlope : name == "amplitude" ? energy0 :
                                       name == "pedestal" ? pedestal : name == "time" ? time : std::abs(noise);
                        fSaturationEnergyFormula->SetParameter(i, value);
                    }
                    double value = fSaturationEnergyFormula->Eval(fSaturationNeedsSlope ? saturationSlope : 0.);
                    if (std::isfinite(value) && value > 0) energy = value;
                    else canEvaluate = false;
                }
                if (!canEvaluate)
                    lk_warning << "Cannot evaluate saturation energy for CAAC " << channel->GetCAAC()
                               << "; using marker " << kSaturatedEnergy << endl;
            }

            auto fitData = (LKPulseFitData*) fFitDataArray -> ConstructedAt(fitDataCount++);
            fitData -> Clear(); // Retain the observed amplitude and the independent saturation flag.
            fitData -> fHitIndex = channelCount;
            fitData -> fIsSaturated = isSaturated;
            fitData -> fNumHitsInChannel = 1;
            fitData -> fTb = time;
            fitData -> fAmplitude = energy0;
            fitData -> fSlopeAmplitude = energy;
            fitData -> fSlope = saturationSlope;
            fitData -> fFitRange1 = slopeFirst;
            fitData -> fFitRange2 = slopeLast;

            siChannel1 -> SetPedestal(pedestal);
            siChannel1 -> SetEnergy(energy);
            siChannel1 -> SetTime(time);
            channel -> SetPedestal(pedestal);
            channel -> SetEnergy(energy);
            channel -> SetTime(time);
        }

        channelCount++;
        if (siChannel1 -> IsPairedChannel())
        {
            for (auto iSiChannel2=0; iSiChannel2<channelCount; ++iSiChannel2)
            {
                if (iSiChannel2==channelCount)
                    continue;
                auto siChannel2 = (LKSiChannel*) fSiChannelArray -> At(iSiChannel2);
                if (siChannel1 -> IsPair(siChannel2))
                {
                    if (siChannel1->GetDirection()==0)
                    {
                        siChannel1 -> SetPairChannel(siChannel2);
                        siChannel1 -> SetPairArrayIndex(siChannel2 -> GetChannelID());
                        siChannel1 -> SetEnergy2(siChannel2->GetEnergy());
                    }
                    else
                    {
                        siChannel2 -> SetPairChannel(siChannel1);
                        siChannel2 -> SetPairArrayIndex(siChannel1 -> GetChannelID());
                        siChannel2 -> SetEnergy2(siChannel1->GetEnergy());
                    }
                }
            }
        }
    }

    lk_info << channelCount << " si-channels found" << endl;
}
