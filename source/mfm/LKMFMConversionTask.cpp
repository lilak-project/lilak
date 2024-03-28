#include <iostream>
using namespace std;

#include "LKMFMConversionTask.h"
#include "GETChannel.h"

//#include "GSpectra.h"
//#include "GNetServerRoot.h"

//#define DEBUG_MFM_CONVERSION_TASK
#define DEBUG_EVENT_POINT_BUFFER

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
    if(!fFileStream.is_open()) {
        lk_error << "Could not open input file!" << std::endl;
        return false;
    }

    return true;
}

void LKMFMConversionTask::Exec(Option_t*)
{
#ifdef DEBUG_MFM_CONVERSION_TASK
    //lk_debug << "file stream : " << fFileStream << " " << matrixSize << endl;
    lk_debug << fFileStream.eof() << endl;
#endif
    if (fFileStream.eof()) {
        lk_warning << "end of MFM file!" << endl;
        fRun -> SignalEndOfRun();
        return;
    }

    fContinueEvent = true;
    int countAddDataChunk = 0;

    char *buffer = (char *) malloc (matrixSize);
#ifdef DEBUG_MFM_CONVERSION_TASK
    lk_debug << "read buffer with " << matrixSize << endl;
#endif

    fFilebuffer = 0;
    while (!fFileStream.eof() && fContinueEvent)
    {
#ifdef DEBUG_MFM_CONVERSION_TASK
        lk_debug << "read " << matrixSize << endl;
#endif
        fFileStream.read(buffer,matrixSize);
        fFilebuffer += matrixSize;
#ifdef DEBUG_MFM_CONVERSION_TASK
        lk_debug << "file buffer " << fFilebuffer << endl;
#endif

#ifdef DEBUG_MFM_CONVERSION_TASK
        lk_debug << fFileStream.eof() << ", " << fFileStream.gcount() << endl;
#endif
        if(!fFileStream.eof()) {
#ifdef DEBUG_MFM_CONVERSION_TASK
            lk_debug << fFilebuffer/matrixSize << endl;
#endif
            // addDataChunk ////////////////////////////////////////////////////////////////////////////////
            try {
#ifdef DEBUG_MFM_CONVERSION_TASK
            lk_debug << fFilebuffer/matrixSize << endl;
#endif
                ++countAddDataChunk;
                fFrameBuilder -> addDataChunk(buffer,buffer+matrixSize);
#ifdef DEBUG_MFM_CONVERSION_TASK
                lk_debug << endl;
#endif
            }catch (const std::exception& e){
                lk_debug << "Error occured from " << countAddDataChunk << "-th addDataChunk()" << endl;
                e_cout << e.what() << endl;
                return;
            }
            ////////////////////////////////////////////////////////////////////////////////////////////////
        }
        else if(fFileStream.gcount()>0) {
#ifdef DEBUG_MFM_CONVERSION_TASK
            lk_debug << endl;
#endif
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
#ifdef DEBUG_EVENT_POINT_BUFFER
    lk_info << "New event! at file buffer: " << fFilebuffer << endl;
#endif
    fContinueEvent = fRun -> ExecuteNextEvent();
}
