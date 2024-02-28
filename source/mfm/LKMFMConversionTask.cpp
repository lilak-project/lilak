#include <iostream>
using namespace std;

#include "LKMFMConversionTask.h"
#include "GETChannel.h"

#include "GSpectra.h"
#include "GNetServerRoot.h"

LKMFMConversionTask::LKMFMConversionTask()
:LKTask("LKMFMConversionTask","")
{
}

bool LKMFMConversionTask::Init() 
{
    fEventHeaderArray = fRun -> RegisterBranchA("FrameHeader", "LKEventHeader", 1);
    fChannelArray     = fRun -> RegisterBranchA("RawData", "GETChannel", 1000);

    fFrameBuilder = new LKFrameBuilder();
    fFrameBuilder -> SetPar(fPar);
    fFrameBuilder -> SetMotherTask(this);
    fFrameBuilder -> SetChannelArray(fChannelArray);
    fFrameBuilder -> SetEventHeaderArray(fEventHeaderArray);
    auto initFB = fFrameBuilder -> Init();
    if (initFB==false) {
        lk_error << "LKFrameBuilder cannot be initialized!" << endl;
        return false;
    }

    auto inputFileName  = fPar -> GetParString("LKMFMConversionTask/InputFileName");
    lk_info << "Opening file " << inputFileName << endl;
    lk_info << "Block size is " << matrixSize << endl;
    fFileStream.open(inputFileName.Data(), std::ios::binary | std::ios::in);
    if(!fFileStream) {
        lk_error << "Could not open input file!" << std::endl;
        return false;
    }

    return true;
}

void LKMFMConversionTask::Exec(Option_t*)
{
    if (fFileStream.eof()) {
        lk_warning << "end of MFM file!" << endl;
        fRun -> SignalEndOfRun();
        return;
    }

    fContinueEvent = true;
    int countAddDataChunk = 0;

    char *buffer = (char *) malloc (matrixSize);

    int filebuffer = 0;
    while (!fFileStream.eof() && fContinueEvent)
    {
        fFileStream.read(buffer,matrixSize);
        filebuffer += matrixSize;

        if(!fFileStream.eof()) {
            // addDataChunk ////////////////////////////////////////////////////////////////////////////////
            try {
                ++countAddDataChunk;
                fFrameBuilder -> addDataChunk(buffer,buffer+matrixSize);
            }catch (const std::exception& e){
                lk_debug << "Error occured from " << countAddDataChunk << "-th addDataChunk()" << endl;
                e_cout << e.what() << endl;
                return;
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////
        }
        else if(fFileStream.gcount()>0) {
            // addDataChunk ////////////////////////////////////////////////////////////////////////////////
            try {
                ++countAddDataChunk;
                fFrameBuilder -> addDataChunk(buffer,buffer+fFileStream.gcount());
                break;
            }catch (const std::exception& e){
                lk_debug << "Error occured from LAST " << countAddDataChunk << "-th addDataChunk()" << endl;
                e_cout << e.what() << endl;
                return;
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////
        }
    }
}

bool LKMFMConversionTask::EndOfRun()
{
    return true;
}

void LKMFMConversionTask::SignalNextEvent()
{
    fContinueEvent = fRun -> ExecuteNextEvent();
}
