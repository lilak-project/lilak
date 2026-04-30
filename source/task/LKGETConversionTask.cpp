#include <iostream>
using namespace std;

#include "LKGETConversionTask.h"

#include "GETChannel.h"
#include "LKEventHeader.h"

ClassImp(LKGETConversionTask);

LKGETConversionTask::LKGETConversionTask()
: LKTask("LKGETConversionTask","")
{
}

bool LKGETConversionTask::Init()
{
    fEventHeaderArray = fRun->RegisterBranchA("FrameHeader", "LKEventHeader", 1);
    fChannelArray = fRun->RegisterBranchA("RawData", "GETChannel", 1000);

    if (fPar->CheckPar("LKGETConversionTask/InputFileName")) {
        auto inputFileName = fPar->GetParString("LKGETConversionTask/InputFileName");
        if (fTriggerInputFileNameArray.size() > 0) {
            lk_error << "Cannot use two different inputs from LKGETConversionTask/InputFileName and LKRun/SearchRun" << endl;
            lk_error << "Using input from LKGETConversionTask/InputFileName ..." << endl;
        }
        fTriggerInputFileNameArray.clear();
        fTriggerInputFileNameArray.push_back(inputFileName);
    }
    else if (fTriggerInputFileNameArray.size() > 0) {
        lk_info << "Using inputs from LKRun" << endl;
    }
    else {
        lk_error << "No input!" << endl;
        return false;
    }

    fConverter = new LKGETRawConverter();
    fConverter->SetPar(fPar);
    fConverter->SetChannelArray(fChannelArray);
    fConverter->SetEventHeaderArray(fEventHeaderArray);
    if (!fConverter->Init()) {
        lk_error << "LKGETRawConverter cannot be initialized!" << endl;
        return false;
    }

    return true;
}

void LKGETConversionTask::Run(Long64_t numEvents)
{
    fNumEvents = numEvents;
    fCountEvents = 0;
    fContinueEvent = true;

    for (auto fileName : fTriggerInputFileNameArray) {
        fRoundedBufferLast = 0;

        e_cout << "===============================================================================================================" << endl;
        lk_info << "Opening file " << fileName << endl;

        ifstream input(fileName.Data(), ios::binary | ios::in);
        if (!input.is_open()) {
            lk_error << "Could not open " << fileName << endl;
            fRun->SignalEndOfRun();
            return;
        }

        while (fContinueEvent) {
            auto startPos = input.tellg();
            LKGETRawFrame frame;
            if (!fParser.ReadNextFrame(input, frame))
                break;

            auto endPos = input.tellg();
            fCurrentFrameStart = (Long64_t) startPos;
            fCurrentFrameEnd = (Long64_t) endPos;

            fConverter->ProcessFrame(frame);
            if (!frame.isBlob)
                SignalNextEvent();
        }

        input.close();
        if (!fContinueEvent)
            break;
    }
}

bool LKGETConversionTask::EndOfRun()
{
    return true;
}

void LKGETConversionTask::SignalNextEvent()
{
    fCountEvents++;
    bool printLog = (fRun->CheckMute(fCountEvents) == false);

    constexpr Long64_t blockSize = 512;
    auto roundedEnd = ((fCurrentFrameEnd + blockSize - 1) / blockSize) * blockSize;
    auto bufferStart = fRoundedBufferLast;
    auto bufferSize = int(roundedEnd - bufferStart);

    if (printLog)
        lk_info << "New event! at file buffer: " << roundedEnd << " (" << bufferSize << ")" << endl;

    auto eventHeader = (LKEventHeader*) fEventHeaderArray->ConstructedAt(0);
    eventHeader->SetBufferStart(bufferStart);
    eventHeader->SetBufferSize(bufferSize);
    fRoundedBufferLast = roundedEnd - blockSize;

    fContinueEvent = fRun->ExecuteNextEvent();
    if (fNumEvents > 0 && fCountEvents >= fNumEvents)
        fContinueEvent = false;
}
