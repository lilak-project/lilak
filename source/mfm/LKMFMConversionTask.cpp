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

    auto inputFileName  = fPar -> GetParString("LKMFMConversionTask/InputFileName");
    if (inputFileName=="online") {
        auto check1 = fPar -> CheckPar("LKMFMConversionTask/watcherIP");
        auto check2 = fPar -> CheckPar("LKMFMConversionTask/watcherPort");
        if (!check1||!check2) {
            lk_error << "Must set watcherIP and watcherPort for oline run" << endl;
            return false;
        }
        fWatcherIP = fPar -> GetParString("LKMFMConversionTask/watcherIP");
        fWatcherPort = fPar -> GetParInt("LKMFMConversionTask/watcherPort");
        lk_info << "watcherIP is " << fWatcherIP << endl;
        lk_info << "watcherPort is " << fWatcherPort << endl;
        lk_info << "Block size is " << matrixSize << endl;
    } else {
      lk_info << "Opening file " << inputFileName << endl;
      lk_info << "Block size is " << matrixSize << endl;
      fFileStream.open(inputFileName.Data(), std::ios::binary | std::ios::in);
      if(!fFileStream.is_open()) {
          lk_error << "Could not open input file!" << std::endl;
          return false;
      }
    }

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

    fContinueEvent = true;
    int countAddDataChunk = 0;

    char *buffer = (char *) malloc (matrixSize);

    fFileBuffer = 0;
    fFileBufferLast = 0;

    while (!fFileStream.eof() && fContinueEvent)
    {

        fFileStream.read(buffer,matrixSize);
        fFileBuffer += matrixSize;

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

void LKMFMConversionTask::RunOnline()
{
    fRunOnline = true;

    int sock;                    /* Socket descriptor */
    struct sockaddr_in servAddr; /* server address */
    int size_buffer;
    int size_to_recv;
    char *buffer;
    int received_data;
    int total_received_data;
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == 0) {
        lk_error << "socket call failed" << endl;
        return;
    }

    fContinueEvent = true;
    int countAddDataChunk = 0;

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
        buffer = (char *) malloc (size_buffer);
        total_received_data = 0;
        size_to_recv = size_buffer;

        lk_info << "start receiving data" << endl;
        while (total_received_data < size_buffer)
        {
            received_data = recv(sock, buffer, size_to_recv, 0);
            total_received_data += received_data;
            if (received_data == -1) {
                lk_error << "error receiving data (" << errno << ")" << endl;
                return;
            }

            buffer += received_data;
            size_to_recv -= received_data;

        }
        close (sock);
        lk_info << "end receiving data" << endl;

        if(size_buffer>4) {
            fFrameBuilder -> addDataChunk(buffer-total_received_data,buffer);
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
    fFileBufferLast = fFileBuffer - matrixSize;

    fContinueEvent = fRun -> ExecuteNextEvent();

    if (fNumEvents>0 && fCountEvents>=fNumEvents)
        fContinueEvent = false;
}
