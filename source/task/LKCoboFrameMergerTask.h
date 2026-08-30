#ifndef LKCOBOFRAMEMERGERTASK_HH
#define LKCOBOFRAMEMERGERTASK_HH

#include "LKRun.h"
#include "LKTask.h"

#include "get_convter/LKGETFrameParser.h"
#include "get_convter/LKGETRawConverter.h"
#include "get_convter/LKCoboFrameMerger.h"

#include <string>
#include <vector>

/**
 * Event-trigger task that merges several CoBo/MuTanT inputs on the fly.
 *
 * GET stores each CoBo into its own raw (.dat/.graw) stream. The cobo-frame-merge
 * tool gathers the frames sharing the same event identifier into one layered
 * frame (0xFF01) so that a single merged file can be analysed. That extra
 * offline step prevents online analysis from running directly on the per-CoBo
 * buffers.
 *
 * LKCoboFrameMergerTask removes that step: it takes the several per-CoBo (and
 * MuTanT) inputs - either files (like cobo-frame-merge) or online buffers -
 * merges them by event id with LKCoboFrameMerger, and, every time one event is
 * complete, decodes the merged frame with LKGETRawConverter and emits the
 * GETChannel "RawData" branch exactly like LKGETConversionTask.
 *
 * Parameters:
 *   LKCoboFrameMergerTask/InputFileName  file0 file1 ...  # one file per source
 *                                        online           # online (watcher) mode
 *   LKCoboFrameMergerTask/watcherIP      ip0 ip1 ...       # one ip per source (online)
 *   LKCoboFrameMergerTask/watcherPort    port0 port1 ...   # one port per source (online)
 * Inputs may also be supplied through LKRun (LKRun/SearchRun, AddInputFile),
 * in which case every input file is treated as one source.
 */
class LKCoboFrameMergerTask : public LKTask
{
    private:
        TClonesArray* fEventHeaderArray = nullptr;
        TClonesArray* fChannelArray = nullptr;

        LKGETFrameParser fParser;
        LKGETRawConverter* fConverter = nullptr;
        LKCoboFrameMerger* fMerger = nullptr;

        Long64_t fNumEvents = -1;
        Long64_t fCountEvents = 0;
        bool fContinueEvent = false;

        Long64_t fMergedBufferLast = 0;       //!
        Int_t fLastMergedSize = 0;            //!

        bool fRunOnline = false;
        std::vector<std::string> fWatcherIP;     //!
        std::vector<int> fWatcherPort;           //!
        std::vector<std::string> fOnlineBuffer;  //!

        int fSleepBeforeConnect = 1;
        int fBreakAfterFailNumber = 10;

    public:
        LKCoboFrameMergerTask();
        virtual ~LKCoboFrameMergerTask() {};

        bool Init();
        void Exec(Option_t*) {}
        void Run(Long64_t numEvents = -1);
        void RunOnline(Long64_t numEvents = -1);
        bool EndOfRun();
        void SignalNextEvent();
        bool IsEventTrigger() { return true; }

        virtual void AddTriggerInputFile(TString fileName, TString opt) {
            if (opt == "cobo" || opt == "mfm" || opt == "get")
                fTriggerInputFileNameArray.push_back(fileName);
        }

    private:
        /// Decode and emit every event the merger can already complete.
        void DrainReadyEvents();

        /// Extract every complete frame available in an online byte buffer,
        /// routing them like AdvanceFileSource does.
        void ConsumeOnlineBuffer(Int_t sourceId);

    ClassDef(LKCoboFrameMergerTask, 1)
};

#endif
