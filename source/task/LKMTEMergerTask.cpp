#include <iostream>
using namespace std;

#include "LKMTEMergerTask.h"
#include "LKEventHeader.h"
#include <stdio.h>

LKMTEMergerTask::LKMTEMergerTask()
:LKTask("LKMTEMergerTask","LKMTEMergerTask")
{
}

bool LKMTEMergerTask::Init()
{
    fPar -> UpdatePar(data_filename,"LKMTEMergerTask/data_filename");
    fPar -> UpdatePar(trigger_time_window,"LKMTEMergerTask/TimeWindowCut");

    if (data_filename.IsNull()) {
        lk_error << "cannot find " << data_filename << endl;
        return false;
    }

    lk_info << "MTE file: " << data_filename << endl;

    fEventHeaderArray = fRun -> KeepBranchA("FrameHeader");
    if (fEventHeaderArray==nullptr) {
        fEventHeaderIsNew = true;
        fEventHeaderArray = fRun -> RegisterBranchA("FrameHeader","LKEventHeader",1);
    }

    for (auto i=0; i<10; ++i)
    {
        fInputChannelName[i] = "";
        fInputChannelNumber[i] = -1;
    }

    auto inputChannelPar = fPar -> CreateMultiParContainer("LKMTEMergerTask/AddInputChannel");
    auto n = inputChannelPar -> GetEntries();
    for (auto i=0; i<n; ++i)
    {
        auto par = inputChannelPar -> GetParameter(i);
        auto inputChannelNumber = par -> GetInt(0);
        TString inputChannelName = par -> GetString(1);
        fInputChannelArray.push_back(inputChannelNumber);
        fInputChannelName[inputChannelNumber] = inputChannelName;
        fInputChannelNumber[inputChannelNumber] = inputChannelNumber;
    }

    if (fPar -> CheckPar("LKMTEMergerTask/KeyInputChannel")==false)
    {
        lk_error << "LKMTEMergerTask/KeyInputChannel do not exist!" << endl;
        return false;
    }
    fKeyChannelNumber = fInputChannelNumber[fPar -> GetParInt("LKMTEMergerTask/KeyInputChannel")];

    fRun -> GetOutputFile() -> cd();
    for (auto i : fInputChannelArray)
    {
        fTree[i] = new TTree(Form("trigger_%s",fInputChannelName[i].Data()),"");
        fTree[i] -> Branch("trig_num",&trig_num);
        fTree[i] -> Branch("trig_time",&trig_time);
        fTree[i] -> Branch("trig_type",&trig_type);
    }

    //fCoboTree = new TTree("mte","only cobo triggered event");
    //fCoboTree -> Branch("tn_cobo",&tn_cobo);
    //fCoboTree -> Branch("tt_cobo",&tt_cobo);
    //fCoboTree -> Branch("tn_kobra",&tn_kobra);
    //fCoboTree -> Branch("tt_kobra",&tt_kobra);

    // get file size to know # of events, 1 event = 32 byte
    data_fp = fopen(data_filename, "rb");
    fseek(data_fp, 0L, SEEK_END);
    file_size = ftell(data_fp);
    fclose(data_fp);
    nevt = file_size / 16;
    data_fp = fopen(data_filename, "rb");

    for (evt = 0; evt < nevt; evt++) {
        fread(data, 1, 16, data_fp);

        // trigger logic #
        memcpy(&trig_num, data, 4);

        // trigger time
        itmp = data[4] & 0xFF;
        ftmp = itmp;
        trig_time = ftmp * 0.008;        // trig_ftime = 8 ns unit
        itmp = data[5] & 0xFF;
        ftmp = itmp;
        trig_time = trig_time + ftmp;
        itmp = data[6] & 0xFF;
        ftmp = itmp;
        ftmp = ftmp * 256.0;
        trig_time = trig_time + ftmp;
        itmp = data[7] & 0xFF;
        ftmp = itmp;
        ftmp = ftmp * 256.0 * 256.0;
        trig_time = trig_time + ftmp;
        itmp = data[8] & 0xFF;
        ftmp = itmp;
        ftmp = ftmp * 256.0 * 256.0 * 256.0;
        trig_time = trig_time + ftmp;
        itmp = data[9] & 0xFF;
        ftmp = itmp;
        ftmp = ftmp * 256.0 * 256.0 * 256.0 * 256.0;
        trig_time = trig_time + ftmp;
        itmp = data[10] & 0xFF;
        ftmp = itmp;
        ftmp = ftmp * 256.0 * 256.0 * 256.0 * 256.0 * 256.0;
        trig_time = trig_time + ftmp;

        // trigger type
        trig_type = data[11] & 0xFF;

        // external trigger pattern
        ext_pattern[0] = data[12] & 0x1;
        ext_pattern[1] = (data[12] >> 1) & 0x1;
        ext_pattern[2] = (data[12] >> 2) & 0x1;
        ext_pattern[3] = (data[12] >> 3) & 0x1;
        ext_pattern[4] = (data[12] >> 4) & 0x1;
        ext_pattern[5] = (data[12] >> 5) & 0x1;
        ext_pattern[6] = (data[12] >> 6) & 0x1;
        ext_pattern[7] = (data[12] >> 7) & 0x1;
        ext_pattern[8] = data[13] & 0x1;
        ext_pattern[9] = (data[13] >> 1) & 0x1;

        for (auto i : fInputChannelArray)
        {
            ext_pattern_channel = ext_pattern[i];
            if (ext_pattern_channel>0)
                fTree[i] -> Fill();
        }
    }

    //auto nCobo = fTree[fCoboInputChannel] -> GetEntries();
    //int iKobra = 0;
    //double trig_time_cobo, trig_time_kobra;
    //double trig_num_cobo, trig_num_kobra;
    //for (auto iCobo=0; iCobo<nCobo; ++iCobo) {
    //    fTree[fCoboInputChannel] -> GetEntry(iCobo);
    //    trig_time_cobo = trig_time;
    //    trig_num_cobo = trig_num;
    //    int countTrial = 0;
    //    while (countTrial<4) {
    //        fTree[fKobraInputChannel] -> GetEntry(iKobra++);
    //        trig_time_kobra = trig_time;
    //        trig_num_kobra = trig_num;
    //        double trig_time_diff = trig_time_cobo - trig_time_kora;
    //        if (abs(trig_time_diff)<trigger_time_window)
    //        {
    //            break;
    //        }
    //        else {
    //            countTrial++;
    //            continue;
    //        }
    //    }
    //}

    fclose(data_fp);

    fRun -> GetOutputFile() -> cd();

    for (auto i : fInputChannelArray)
    {
        fTree[i] -> Write();
    }

    return true;
}

void LKMTEMergerTask::Exec(Option_t *option)
{
    LKEventHeader* eventHeader;
    if (fEventHeaderIsNew)
        eventHeader = (LKEventHeader *) fEventHeaderArray -> ConstructedAt(0);
    else
        eventHeader = (LKEventHeader *) fEventHeaderArray -> At(0);

    auto eventID = fRun -> GetCurrentEventID();
    fTree[fKeyChannelNumber] -> GetEntry(eventID);

    eventHeader -> fMTETimeStamp = trig_time;

    //for (auto i : fInputChannelArray) {
        //fTree[i] -> GetEntry(eventID); // cobo
    //}

    //int trig_num;
    //double trig_time;
    //int trig_type;
    //int ext_pattern_channel;
}
