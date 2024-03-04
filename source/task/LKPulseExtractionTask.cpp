#include "LKPulseExtractionTask.h"

ClassImp(LKPulseExtractionTask);

LKPulseExtractionTask::LKPulseExtractionTask()
{
    fName = "LKPulseExtractionTask";
}

bool LKPulseExtractionTask::Init()
{
    fChannelArray = fRun -> GetBranchA("RawData");

    fPar -> UpdatePar(fAnalysisName      ,"LKPulseExtractionTask/analysisName");
    fPar -> UpdatePar(fThreshold         ,"LKPulseExtractionTask/threshold");
    fPar -> UpdatePar(fFixPedestal       ,"LKPulseExtractionTask/fixPedestal");
    fPar -> UpdatePar(fChannelIsInverted ,"LKPulseExtractionTask/channelIsInverted");
    if (fPar -> CheckPar("LKPulseExtractionTask/tbRange 0 512")) {
        fTbRange1 = fPar -> GetParInt("LKPulseExtractionTask/tbRange",0);
        fTbRange2 = fPar -> GetParInt("LKPulseExtractionTask/tbRange",1);
    }
    if (fPar -> CheckPar("LKPulseExtractionTask/tbRangeCut 1, 511")) {
        fTbRangeCut1 = fPar -> GetParInt("LKPulseExtractionTask/tbRangeCut",0);
        fTbRangeCut2 = fPar -> GetParInt("LKPulseExtractionTask/tbRangeCut",1);
    }
    if (fPar -> CheckPar("LKPulseExtractionTask/tbHeightCut 100,4000")) {
        fPulseHeightCut1 = fPar -> GetParInt("LKPulseExtractionTask/tbHeightCut",0);
        fPulseHeightCut2 = fPar -> GetParInt("LKPulseExtractionTask/tbHeightCut",1);
    }
    if (fPar -> CheckPar("LKPulseExtractionTask/tbWidthCut 20 40")) {
        fPulseWidthCut1 = fPar -> GetParInt("LKPulseExtractionTask/tbWidthCut",0);
        fPulseWidthCut2 = fPar -> GetParInt("LKPulseExtractionTask/tbWidthCut",1);
    }

    fPulseAnalyzer = new LKPulseAnalyzer(fAnalysisName,fRun->GetDataPath());
    fPulseAnalyzer -> SetThreshold(fThreshold);
    fPulseAnalyzer -> SetTbRange(fTbRange1,fTbRange2);
    fPulseAnalyzer -> SetPulseTbCuts(fTbRangeCut1,fTbRangeCut2);
    fPulseAnalyzer -> SetPulseWidthCuts(fPulseWidthCut1,fPulseWidthCut2);
    fPulseAnalyzer -> SetPulseHeightCuts(fPulseHeightCut1,fPulseHeightCut2);
    fPulseAnalyzer -> SetInvertChannel(fChannelIsInverted);
    fPulseAnalyzer -> SetFixPedestal(fFixPedestal);

    return true;
}

void LKPulseExtractionTask::Exec(Option_t *option)
{
    auto eventID = fRun -> GetCurrentEventID();

    int numChannels = fChannelArray -> GetEntries();
    for (int iChannel=0; iChannel<numChannels; iChannel++)
    {
        auto channel = (GETChannel *) fChannelArray -> At(iChannel);
        auto cobo = channel -> GetCobo();
        auto asad = channel -> GetAsad();
        auto aget = channel -> GetAget();
        auto chan = channel -> GetChan();
        auto data = channel -> GetWaveformY();

        fPulseAnalyzer -> AddChannel(data, eventID, cobo, asad, aget, chan);
    }

    lk_info << "Channels: +" << numChannels << " >> " << fPulseAnalyzer->GetNumGoodChannels() << endl;
}

bool LKPulseExtractionTask::EndOfRun()
{
    auto runHeader = fRun -> GetRunHeader();

    auto file1 = fPulseAnalyzer -> WriteReferencePulse(fPulseWidthCut1,fPulseWidthCut2);
    file1 -> cd();
    runHeader -> Write(runHeader->GetName(),TObject::kSingleKey);

    bool writeSummaryTree = true;
    if (fPar -> CheckPar("LKPulseExtractionTask/writeSummaryTree"))
        writeSummaryTree = fPar -> GetParBool("LKPulseExtractionTask/writeSummaryTree");
    auto file2 = fPulseAnalyzer -> WriteTree();
    file2 -> cd();
    runHeader -> Write(runHeader->GetName(),TObject::kSingleKey);

    return true;
}
