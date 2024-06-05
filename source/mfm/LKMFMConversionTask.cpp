#include <iostream>
using namespace std;

#include "LKMFMConversionTask.h"
#include "GETChannel.h"
#include "LKEventHeader.h"

#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */

//#define DEBUG_MFM_CONVERSION_TASK

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

    if (fPar->CheckPar("LKMFMConversionTask/InputFileName"))
    {
        auto inputFileName  = fPar -> GetParString("LKMFMConversionTask/InputFileName");
        if (inputFileName=="online")
        {
            auto check1 = fPar -> CheckPar("LKMFMConversionTask/watcherIP");
            auto check2 = fPar -> CheckPar("LKMFMConversionTask/watcherPort");
            if (!check1||!check2) {
                lk_error << "Must set watcherIP and watcherPort for oline run" << endl;
                return false;
            }
            fWatcherIP = fPar -> GetParString("LKMFMConversionTask/watcherIP    127.0.0.1");
            fWatcherPort = fPar -> GetParInt("LKMFMConversionTask/watcherPort  10205");
            lk_info << "watcherIP is " << fWatcherIP << endl;
            lk_info << "watcherPort is " << fWatcherPort << endl;
            lk_info << "Block size is " << fMatrixSize << endl;
        }
        else
        {
            if (fTriggerInputFileNameArray.size()>0)
            {
                lk_error << "Cannot use two different inputs from LKMFMConversionTask/InputFileName and LKRun/SearchRun" << endl;
                lk_error << "Using input from LKMFMConversionTask/InputFileName ..." << endl;
            }
            fTriggerInputFileNameArray.push_back(inputFileName);
        }
    }
    else if (fTriggerInputFileNameArray.size()>0)
    {
        lk_info << "Using inputs from LKRun" << fWatcherIP << endl;
    }
    else
    {
        lk_error << "No input! "<< endl;
        return false;
    }

    fBuffer = (char *) malloc (fMatrixSize);

    return true;
}

void LKMFMConversionTask::Run(Long64_t numEvents)
{
    fNumEvents = numEvents;

    if (fFileStream.eof()) {
        lk_warning << "end of MFM file!" << endl;
        fRun -> SignalEndOfRun();
        return;
    }

    for (auto fileName : fTriggerInputFileNameArray)
    {
        fFileBuffer = 0;
        fFileBufferLast = 0;
        fContinueEvent = true;
        fCountAddDataChunk = 0;

        e_cout << "=============================================================" << endl;
        lk_info << "Opening file " << fileName << endl;
        fFileStream.open(fileName.Data(), std::ios::binary | std::ios::in);
        if (!fFileStream.is_open()) {
            lk_error << "Could not open " << fileName << std::endl;
            fRun -> SignalEndOfRun();
            return;
        }
        e_cout << "=============================================================" << endl;

        while (!fFileStream.eof() && fContinueEvent)
            Exec();

        fFileStream.close();
    }
}

void LKMFMConversionTask::Exec(Option_t*)
{
    fFileStream.read(fBuffer,fMatrixSize);
    fFileBuffer += fMatrixSize;

    if (!fFileStream.eof()) {
        // addDataChunk ////////////////////////////////////////////////////////////////////////////////
        try {
            ++fCountAddDataChunk;
            fFrameBuilder -> addDataChunk(fBuffer,fBuffer+fMatrixSize);
        } catch (const std::exception& e){
            lk_debug << "Error occured from " << fCountAddDataChunk << "-th addDataChunk()" << endl;
            e_cout << e.what() << endl;
            return;
        }
        ////////////////////////////////////////////////////////////////////////////////////////////////
    }
    else if (fFileStream.gcount()>0) {
        // addDataChunk ////////////////////////////////////////////////////////////////////////////////
        try {
            ++fCountAddDataChunk;
            fFrameBuilder -> addDataChunk(fBuffer,fBuffer+fFileStream.gcount());
            break;
        } catch (const std::exception& e){
            lk_debug << "Error occured from LAST " << fCountAddDataChunk << "-th addDataChunk()" << endl;
            e_cout << e.what() << endl;
            return;
        }
        ////////////////////////////////////////////////////////////////////////////////////////////////
    }
}

void LKMFMConversionTask::RunOnline()
{
    fRunOnline = true;

    int sock;                    /* Socket descriptor */
    struct sockaddr_in servAddr; /* server address */
    int size_buffer;
    int size_to_recv;
    int received_data;
    int total_received_data;
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == 0) {
        lk_error << "socket call failed" << endl;
        return;
    }

    fContinueEvent = true;

    while (1)
    {
        memset(&servAddr, 0, sizeof(servAddr));
        servAddr.sin_family = AF_INET;             /* Internet address family */
        servAddr.sin_addr.s_addr = inet_addr(fWatcherIP.c_str());   /* Server IP address */
        servAddr.sin_port = htons(fWatcherPort); /* Server port */
        if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
            lk_error << "connect call failed" << endl;
            return;
        }
        lk_info << "connect call success!" << endl;

        recv(sock, &size_buffer, sizeof (int), 0);
        lk_info << "size of total buffer: " << size_buffer << endl;
        fBuffer = (char *) malloc (size_buffer);
        total_received_data = 0;
        size_to_recv = size_buffer;

        lk_info << "start receiving data" << endl;
        while (total_received_data < size_buffer)
        {
            received_data = recv(sock, fBuffer, size_to_recv, 0);
            total_received_data += received_data;
            if (received_data == -1) {
                lk_error << "error receiving data (" << errno << ")" << endl;
                return;
            }

            fBuffer += received_data;
            size_to_recv -= received_data;

        }
        close (sock);
        lk_info << "end receiving data" << endl;

        if(size_buffer>4) {
            fFrameBuilder -> addDataChunk(fBuffer-total_received_data,fBuffer);
            lk_error << "online data recorded." << endl;

        }
    }
}

bool LKMFMConversionTask::EndOfRun()
{
    return true;
}

void LKMFMConversionTask::SignalNextEvent()
{
    fCountEvents++;
    int bufferSize = fFileBuffer - fFileBufferLast;

    if (fRunOnline)
        lk_info << "New event!" << endl;
    else
        lk_info << "New event! at file buffer: " << fFileBuffer << " (" << bufferSize << ")" << endl;
    auto eventHeader = (LKEventHeader *) fEventHeaderArray -> At(0);
    eventHeader -> SetBufferStart(fFileBufferLast);
    eventHeader -> SetBufferSize(bufferSize);
    fFileBufferLast = fFileBuffer - fMatrixSize;

    fContinueEvent = fRun -> ExecuteNextEvent();

    if (fNumEvents>0 && fCountEvents>=fNumEvents)
        fContinueEvent = false;
}
