#include "LKPulseExtractionTask.h"

ClassImp(LKPulseExtractionTask);

LKPulseExtractionTask::LKPulseExtractionTask()
{
    fName = "LKPulseExtractionTask";
}

bool LKPulseExtractionTask::Init()
{
    fChannelArray = fRun -> GetBranchA("RawData","GETChannel");

    fPar -> UpdatePar(fAnalysisName      ,"LKPulseExtractionTask/analysisName");
    fPar -> UpdatePar(fThreshold         ,"LKPulseExtractionTask/threshold");
    fPar -> UpdatePar(fFixPedestal       ,"LKPulseExtractionTask/fixPedestal");
    fPar -> UpdatePar(fChannelIsInverted ,"LKPulseExtractionTask/channelIsInverted");
    fPar -> UpdatePar(fWritePulseFunctionParametersOnly ,"LKPulseExtractionTask/writePulseFunctionParametersOnly false # If true, fit extracted reference pulse with LKPulse function and save only function parameters.");
    fPar -> UpdatePar(fFixPulseFunctionAlpha ,"LKPulseExtractionTask/fixPulseFunctionAlpha false # Fix LKPulse function alpha when writePulseFunctionParametersOnly is true.");
    fPar -> UpdatePar(fFixPulseFunctionTau ,"LKPulseExtractionTask/fixPulseFunctionTau false # Fix LKPulse function tau when writePulseFunctionParametersOnly is true.");
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

    fPulseExtractor = new LKPulseExtractor(fAnalysisName,fRun->GetDataPath());
    fPulseExtractor -> SetThreshold(fThreshold);
    fPulseExtractor -> SetTbRange(fTbRange1,fTbRange2);
    fPulseExtractor -> SetPulseTbCuts(fTbRangeCut1,fTbRangeCut2);
    fPulseExtractor -> SetPulseWidthCuts(fPulseWidthCut1,fPulseWidthCut2);
    fPulseExtractor -> SetPulseHeightCuts(fPulseHeightCut1,fPulseHeightCut2);
    fPulseExtractor -> SetInvertChannel(fChannelIsInverted);
    fPulseExtractor -> SetFixPedestal(fFixPedestal);
    fPulseExtractor -> SetWritePulseFunctionParametersOnly(fWritePulseFunctionParametersOnly);
    fPulseExtractor -> FixPulseFunctionAlpha(fFixPulseFunctionAlpha);
    fPulseExtractor -> FixPulseFunctionTau(fFixPulseFunctionTau);

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

        fPulseExtractor -> AddChannel(data, eventID, cobo, asad, aget, chan);
    }

    lk_info << "Channels: +" << numChannels << " >> " << fPulseExtractor->GetNumGoodChannels() << endl;
}

bool LKPulseExtractionTask::EndOfRun()
{
    auto runHeader = fRun -> GetRunHeader();

    if (fWritePulseFunctionParametersOnly)
    {
        fPulseExtractor -> WritePulseParameterFile(fPulseWidthCut1,fPulseWidthCut2);
    }
    else
    {
        auto file1 = fPulseExtractor -> WriteReferencePulse(fPulseWidthCut1,fPulseWidthCut2);
        if (file1!=nullptr) {
            file1 -> cd();
            runHeader -> Write(runHeader->GetName(),TObject::kSingleKey);
        }
    }

    bool writeSummaryTree = true;
    if (fPar -> CheckPar("LKPulseExtractionTask/writeSummaryTree"))
        writeSummaryTree = fPar -> GetParBool("LKPulseExtractionTask/writeSummaryTree");
    if (writeSummaryTree)
    {
        auto file2 = fPulseExtractor -> WriteTree();
        if (file2!=nullptr) {
            file2 -> cd();
            runHeader -> Write(runHeader->GetName(),TObject::kSingleKey);
        }
    }

    return true;
}
