#ifndef LKBRAGGPEAKTRIGGERTASK_HH
#define LKBRAGGPEAKTRIGGERTASK_HH

#include "LKTask.h"

class TClonesArray;
class TGraph;

/**
 * @brief Generates Bragg-peak energy-loss points as an event trigger.
 *
 * This task generates on-the-fly events containing a compact array of
 * `LKWPoint` objects. The point `X` coordinate is the depth inside the matter,
 * and the point weight `W` is the relative charged-particle energy loss
 * dE/dx at that depth. The generated profile is a generic Bragg-peak shape:
 * a slowly rising ionization loss followed by a sharp peak near the end of the
 * particle range.
 *
 * **Input branches**
 * - none
 *
 * **Output branches**
 * - `BraggPeak` (`LKWPoint`) - generated depth and energy-loss points
 *
 * **Parameters**
 * | Parameter key                         | Type   | Default | Description |
 * |---------------------------------------|--------|---------|-------------|
 * | `LKBraggPeakTriggerTask/numEvents`    | int    | 1       | Number of events generated when `Run(-1)` is used |
 * | `LKBraggPeakTriggerTask/numPoints`    | int    | 80      | Number of generated points, clamped to [2, 99] |
 * | `LKBraggPeakTriggerTask/range`        | double | 100.0   | Particle range in arbitrary depth units |
 * | `LKBraggPeakTriggerTask/peakDepth`    | double | 86.0    | Depth where the Bragg peak is centered |
 * | `LKBraggPeakTriggerTask/peakWidth`    | double | 7.0     | Width of the Bragg peak |
 * | `LKBraggPeakTriggerTask/baselineLoss` | double | 1.0     | Baseline energy-loss scale |
 * | `LKBraggPeakTriggerTask/peakLoss`     | double | 8.0     | Peak energy-loss scale |
 */
class LKBraggPeakTriggerTask : public LKTask
{
    public:
        LKBraggPeakTriggerTask();
        virtual ~LKBraggPeakTriggerTask() {}

        bool IsEventTrigger() { return true; }

        bool Init();
        void Run(Long64_t numEvents = -1);
        void SignalNextEvent();
        void Draw(Option_t *option = "");

    private:
        void FillBraggPeakPoints(Long64_t eventID);
        double EvalEnergyLoss(double depth) const;

        TClonesArray *fBraggPeakArray = nullptr; ///< BraggPeak branch, owned by this task.
        TGraph *fGraph = nullptr;                ///< Draw helper for dE/dx versus depth.

        int fNumEvents = 1;          ///< Set by LKBraggPeakTriggerTask/numEvents.
        int fNumPoints = 80;         ///< Set by LKBraggPeakTriggerTask/numPoints; clamped below 100.
        double fRange = 100.0;       ///< Set by LKBraggPeakTriggerTask/range.
        double fPeakDepth = 86.0;    ///< Set by LKBraggPeakTriggerTask/peakDepth.
        double fPeakWidth = 7.0;     ///< Set by LKBraggPeakTriggerTask/peakWidth.
        double fBaselineLoss = 1.0;  ///< Set by LKBraggPeakTriggerTask/baselineLoss.
        double fPeakLoss = 8.0;      ///< Set by LKBraggPeakTriggerTask/peakLoss.

    ClassDef(LKBraggPeakTriggerTask, 1)
};

#endif
