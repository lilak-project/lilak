#ifndef LKMTEMERGERTASK_HH
#define LKMTEMERGERTASK_HH

#include "LKTask.h"
#include "TTree.h"
#include "LKRun.h"
#include <vector>

/// Master trigger electronics
class LKMTEMergerTask : public LKTask
{
    public:
        LKMTEMergerTask();
        virtual ~LKMTEMergerTask() {};

        bool Init();
        void Exec(Option_t *option="");
        //bool EndOfRun();

    private:
        TString data_filename;
        FILE *data_fp;
        FILE *text_fp;
        unsigned int file_size;
        int nevt;
        int evt;
        char data[16];

        int trig_num;
        double trig_time;
        int trig_type;
        int ext_pattern[10];
        int ext_pattern_channel;

        int itmp;
        double ftmp;
        int i;

        TTree* fTree[10];
        TTree* fCoboTree;

        int fKeyChannelNumber;
        int fInputChannelNumber[10];
        TString fInputChannelName[10];
        std::vector<int> fInputChannelArray;

        double trigger_time_window = 3;

        bool fEventHeaderIsNew = false;
        TClonesArray *fEventHeaderArray = nullptr;

    ClassDef(LKMTEMergerTask, 1)
};

#endif
