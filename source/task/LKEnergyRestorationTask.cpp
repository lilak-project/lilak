#include "LKEnergyRestorationTask.h"

#include "LKLogger.h"
#include "LKRun.h"
#include "LKSiChannel.h"
#include "SKSiHit.h"

#include <fstream>
#include <sstream>
#include <string>

ClassImp(LKEnergyRestorationTask)

LKEnergyRestorationTask::LKEnergyRestorationTask()
    : LKTask("LKEnergyRestorationTask", "LKEnergyRestorationTask")
{
}

bool LKEnergyRestorationTask::Init()
{
    fSiChannelArray = fRun -> GetBranchA("SiChannel", "LKSiChannel");
    if (fSiChannelArray == nullptr) {
        lk_error << "Branch SiChannel does not exist. Run LKSetSiChannelTask before LKEnergyRestorationTask." << endl;
        return false;
    }

    fPar -> UpdatePar(fEnergyCalibrationFileName, "LKEnergyRestorationTask/energyCalibrationFile");
    if (fEnergyCalibrationFileName.IsNull()) {
        fPar -> UpdatePar(fEnergyCalibrationFileName, "si_array/EnergyCalibrationFile");
        if (!fEnergyCalibrationFileName.IsNull())
            lk_warning << "Parameter si_array/EnergyCalibrationFile is deprecated for LKEnergyRestorationTask. "
                       << "Use LKEnergyRestorationTask/energyCalibrationFile instead." << endl;
    }
    if (fEnergyCalibrationFileName.IsNull()) {
        fPar -> UpdatePar(fEnergyCalibrationFileName, "stark/EnergyCalibrationFile");
        if (!fEnergyCalibrationFileName.IsNull())
            lk_warning << "Parameter stark/EnergyCalibrationFile is deprecated for LKEnergyRestorationTask. "
                       << "Use LKEnergyRestorationTask/energyCalibrationFile instead." << endl;
    }

    fPar -> UpdatePar(fC2ReferenceEnergy, "LKEnergyRestorationTask/c2ReferenceEnergy");
    fPar -> UpdatePar(fApplyToPairChannel, "LKEnergyRestorationTask/applyToPairChannel");
    fPar -> UpdatePar(fBuildSiHit, "LKEnergyRestorationTask/buildSiHit");

    if (fBuildSiHit)
        fHitArray = fRun -> RegisterBranchA("SiHit", "SKSiHit", 20);

    if (fEnergyCalibrationFileName.IsNull()) {
        lk_error << "Set LKEnergyRestorationTask/energyCalibrationFile." << endl;
        return false;
    }

    if (!LoadEnergyCalibrationFile(fEnergyCalibrationFileName))
        return false;

    lk_info << "Loaded energy calibration file " << fEnergyCalibrationFileName
            << " with C0=" << fC0Parameters.size()
            << ", C1=" << fC1Parameters.size()
            << ", C2=" << fC2Parameters.size()
            << ", C3=" << fC3Parameters.size() << endl;
    return true;
}

bool LKEnergyRestorationTask::LoadEnergyCalibrationFile(TString fileName)
{
    std::ifstream file(fileName.Data());
    if (!file.is_open()) {
        lk_error << "Cannot open energy calibration file " << fileName << endl;
        return false;
    }

    fC0Parameters.clear();
    fC1Parameters.clear();
    fC2Parameters.clear();
    fC3Parameters.clear();

    std::string line;
    while (std::getline(file, line)) {
        auto comment = line.find('#');
        if (comment != std::string::npos)
            line = line.substr(0, comment);
        std::stringstream stream(line);

        int det = 0;
        int side = 0;
        int strip = 0;
        double c0i = 0;
        double c0s = 1;
        double c1Li = 0;
        double c1Ls = 1;
        double c1Ri = 0;
        double c1Rs = 1;
        double c2b0 = fC2ReferenceEnergy;
        double c2b1 = 0;
        double c2b2 = 0;
        double c3i = 0;
        double c3s = 1;
        if (!(stream >> det >> side >> strip
              >> c0i >> c0s
              >> c1Li >> c1Ls
              >> c1Ri >> c1Rs
              >> c2b0 >> c2b1 >> c2b2
              >> c3i >> c3s))
            continue;

        auto key = StripKey(det, side, strip);
        fC0Parameters[key] = {c0i, c0s};
        fC1Parameters[LRKey(det, side, strip, 0)] = {c1Li, c1Ls};
        fC1Parameters[LRKey(det, side, strip, 1)] = {c1Ri, c1Rs};
        fC2Parameters[key] = {c2b0, c2b1, c2b2};
        fC3Parameters[key] = {c3i, c3s};
    }

    return !fC0Parameters.empty();
}

bool LKEnergyRestorationTask::HasC0(const StripKey &key) const
{
    return fC0Parameters.find(key) != fC0Parameters.end();
}

bool LKEnergyRestorationTask::HasC1(const StripKey &key) const
{
    auto det = std::get<0>(key);
    auto side = std::get<1>(key);
    auto strip = std::get<2>(key);
    return fC1Parameters.find(LRKey(det, side, strip, 0)) != fC1Parameters.end()
        && fC1Parameters.find(LRKey(det, side, strip, 1)) != fC1Parameters.end();
}

bool LKEnergyRestorationTask::HasC2(const StripKey &key) const
{
    return fC2Parameters.find(key) != fC2Parameters.end();
}

bool LKEnergyRestorationTask::HasC3(const StripKey &key) const
{
    return fC3Parameters.find(key) != fC3Parameters.end();
}

double LKEnergyRestorationTask::ApplyLinear(const std::array<double,2> &par, double value) const
{
    return par[0] + par[1] * value;
}

bool LKEnergyRestorationTask::ApplyStandalone(LKSiChannel *channel)
{
    auto key = StripKey(channel->GetDetNum(), channel->GetSide(), channel->GetStrip());
    if (!HasC0(key))
        return false;

    auto energy = ApplyLinear(fC0Parameters.at(key), channel->GetEnergy());
    channel -> SetEnergy(energy);
    AddSiHit(channel, energy);
    return true;
}

bool LKEnergyRestorationTask::ApplyPaired(LKSiChannel *channel)
{
    if (channel->GetDirection() != 0 || channel->GetEnergy() <= 0 || channel->GetEnergy2() <= 0)
        return false;

    auto det = channel -> GetDetNum();
    auto side = channel -> GetSide();
    auto strip = channel -> GetStrip();
    auto key = StripKey(det, side, strip);
    if (!HasC1(key) || !HasC2(key) || !HasC3(key))
        return false;

    // Direction 0 is the X6 high/up end and direction 1 is the low/down end.
    // Keep RelativeZ increasing geometrically from low (-1) to high (+1).
    auto energyHigh = channel -> GetEnergy();
    auto energyLow = channel -> GetEnergy2();
    energyLow = ApplyLinear(fC1Parameters.at(LRKey(det, side, strip, 0)), energyLow);
    energyHigh = ApplyLinear(fC1Parameters.at(LRKey(det, side, strip, 1)), energyHigh);

    if (energyLow <= 0 || energyHigh <= 0)
        return false;

    auto energySum = energyLow + energyHigh;
    if (energySum <= 0)
        return false;

    auto position = (energyHigh - energyLow) / energySum;
    auto c2 = fC2Parameters.at(key);
    auto scale = c2[0] + c2[1] * position + c2[2] * position * position;
    if (scale == 0)
        return false;

    auto correctedSum = energySum / scale * fC2ReferenceEnergy;
    correctedSum = ApplyLinear(fC3Parameters.at(key), correctedSum);
    if (correctedSum <= 0)
        return false;

    auto factor = correctedSum / energySum;
    energyLow *= factor;
    energyHigh *= factor;
    channel -> SetEnergy1(energyHigh);
    channel -> SetEnergy2(energyLow);
    AddSiHit(channel, correctedSum, position);

    if (!fApplyToPairChannel)
        return true;

    auto pairIndex = channel -> GetPairArrayIndex();
    if (pairIndex < 0 || pairIndex >= fSiChannelArray->GetEntriesFast())
        return true;

    auto pairChannel = (LKSiChannel *) fSiChannelArray -> At(pairIndex);
    if (pairChannel == nullptr)
        return true;

    pairChannel -> SetEnergy1(energyLow);
    pairChannel -> SetEnergy2(energyHigh);
    return true;
}

void LKEnergyRestorationTask::AddSiHit(LKSiChannel *channel, double energy, double relativeZ)
{
    if (fHitArray == nullptr || energy <= 0)
        return;

    auto countHits = fHitArray -> GetEntriesFast();
    auto siHit = (SKSiHit *) fHitArray -> ConstructedAt(countHits);
    siHit -> SetDetID(channel->GetDetID());
    siHit -> SetJunctionStrip(channel->GetStrip());
    siHit -> SetKeyEnergy(energy);
    siHit -> SetEnergy(energy);
    siHit -> SetStripPosition(channel->GetPosition());
    siHit -> SetPhi(channel->GetPhi0());
    siHit -> SetTheta(channel->GetTheta0());
    siHit -> SetIsEDetector(true);
    siHit -> SetIsEPairDetector(channel->IsPairedChannel());

    if (channel->GetSide() == 1) {
        siHit -> SetOhmicStrip(channel->GetStrip());
        siHit -> SetEnergyOhmic(energy);
    }

    if (channel->IsPairedChannel()) {
        siHit -> SetRelativeZ(relativeZ);
        siHit -> SetEnergyRight(channel->GetEnergy1());
        siHit -> SetEnergyLeft(channel->GetEnergy2());
    }
}

void LKEnergyRestorationTask::Exec(Option_t *)
{
    if (fHitArray != nullptr)
        fHitArray -> Clear("C");

    auto numChannels = fSiChannelArray -> GetEntriesFast();
    auto countStandalone = 0;
    auto countPaired = 0;
    for (auto iChannel = 0; iChannel < numChannels; ++iChannel) {
        auto channel = (LKSiChannel *) fSiChannelArray -> At(iChannel);
        if (channel == nullptr)
            continue;

        if (channel->IsStandaloneChannel()) {
            if (ApplyStandalone(channel))
                ++countStandalone;
        }
        else if (channel->IsPairedChannel() && channel->GetDirection() == 0) {
            if (ApplyPaired(channel))
                ++countPaired;
        }
    }

    lk_info << "Restored energy for standalone=" << countStandalone
            << ", paired=" << countPaired
            << ", hits=" << (fHitArray == nullptr ? 0 : fHitArray->GetEntriesFast())
            << " silicon channels" << endl;
}
