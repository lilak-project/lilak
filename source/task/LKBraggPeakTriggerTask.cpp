#include "LKBraggPeakTriggerTask.h"

#include <algorithm>
#include <cmath>

#include "LKLogger.h"
#include "LKRun.h"
#include "LKWPoint.h"
#include "TClonesArray.h"
#include "TGraph.h"

ClassImp(LKBraggPeakTriggerTask)

LKBraggPeakTriggerTask::LKBraggPeakTriggerTask()
    : LKTask("LKBraggPeakTriggerTask", "Generate Bragg peak LKWPoint events")
{
}

bool LKBraggPeakTriggerTask::Init()
{
    fPar -> UpdatePar(fNumEvents,    "LKBraggPeakTriggerTask/numEvents");
    fPar -> UpdatePar(fNumPoints,    "LKBraggPeakTriggerTask/numPoints");
    fPar -> UpdatePar(fRange,        "LKBraggPeakTriggerTask/range");
    fPar -> UpdatePar(fPeakDepth,    "LKBraggPeakTriggerTask/peakDepth");
    fPar -> UpdatePar(fPeakWidth,    "LKBraggPeakTriggerTask/peakWidth");
    fPar -> UpdatePar(fBaselineLoss, "LKBraggPeakTriggerTask/baselineLoss");
    fPar -> UpdatePar(fPeakLoss,     "LKBraggPeakTriggerTask/peakLoss");

    fNumEvents = std::max(1, fNumEvents);
    fNumPoints = std::max(2, std::min(99, fNumPoints));
    fRange = std::max(1.e-9, fRange);
    fPeakDepth = std::max(0.0, std::min(fRange, fPeakDepth));
    fPeakWidth = std::max(1.e-9, fPeakWidth);
    fBaselineLoss = std::max(0.0, fBaselineLoss);
    fPeakLoss = std::max(0.0, fPeakLoss);

    fBraggPeakArray = fRun -> RegisterBranchA("BraggPeak", "LKWPoint", fNumPoints);

    return true;
}

void LKBraggPeakTriggerTask::Run(Long64_t numEvents)
{
    const auto countEvents = (numEvents > 0 ? numEvents : fNumEvents);

    for (Long64_t iEvent = 0; iEvent < countEvents; ++iEvent) {
        FillBraggPeakPoints(iEvent);
        SignalNextEvent();
    }
}

void LKBraggPeakTriggerTask::SignalNextEvent()
{
    fRun -> ExecuteNextEvent();
}

void LKBraggPeakTriggerTask::FillBraggPeakPoints(Long64_t eventID)
{
    fBraggPeakArray -> Clear("C");

    const double eventScale = 1.0 + 0.03 * std::sin(0.73 * double(eventID));

    for (int iPoint = 0; iPoint < fNumPoints; ++iPoint) {
        const double fraction = double(iPoint) / double(fNumPoints - 1);
        const double depth = fRange * fraction;
        const double energyLoss = eventScale * EvalEnergyLoss(depth);

        auto point = (LKWPoint *) fBraggPeakArray -> ConstructedAt(iPoint);
        point -> Set(depth, 0.0, 0.0, energyLoss);
    }

    lk_info << fNumPoints << " BraggPeak LKWPoint objects created" << endl;
}

double LKBraggPeakTriggerTask::EvalEnergyLoss(double depth) const
{
    const double residualRange = std::max(0.04, (fRange - depth) / fRange);
    const double stoppingRise = fBaselineLoss / std::sqrt(residualRange);
    const double peakArg = (depth - fPeakDepth) / fPeakWidth;
    const double braggPeak = fPeakLoss * std::exp(-0.5 * peakArg * peakArg);
    const double exitFalloff = 1.0 / (1.0 + std::exp((depth - fRange) / (0.02 * fRange)));

    return (stoppingRise + braggPeak) * exitFalloff;
}

void LKBraggPeakTriggerTask::Draw(Option_t *)
{
    if (fBraggPeakArray == nullptr)
        return;

    const auto numPoints = fBraggPeakArray -> GetEntriesFast();
    if (fGraph == nullptr)
        fGraph = new TGraph();

    fGraph -> Set(0);
    fGraph -> SetName("LKBraggPeakTriggerTask_BraggPeak");
    fGraph -> SetTitle("Bragg peak;depth in matter;relative energy loss dE/dx");
    fGraph -> SetMarkerStyle(20);
    fGraph -> SetMarkerSize(0.8);
    fGraph -> SetLineWidth(2);

    for (int iPoint = 0; iPoint < numPoints; ++iPoint) {
        auto point = (LKWPoint *) fBraggPeakArray -> At(iPoint);
        fGraph -> SetPoint(iPoint, point -> X(), point -> W());
    }

    fGraph -> Draw("ALP");
}
