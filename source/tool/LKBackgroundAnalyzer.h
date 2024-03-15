#ifndef LKBACKGROUNDANALYZER_HH
#define LKBACKGROUNDANALYZER_HH

#include "TObject.h"
#include "LKLogger.h"
#include "LKChannelBuffer.h"
#include "TObjArray.h"

class LKBackgroundAnalyzer : public TObject
{
    public:
        LKBackgroundAnalyzer() {}
        virtual ~LKBackgroundAnalyzer() {}

        bool Init();
        void Clear(Option_t *option="");

        double* GetFPNData() { return fFPNData; }

        void Add(LKChannelBuffer* channel);
        void AddFPNData(LKChannelBuffer* channel);
        bool Analyze();

    private:
        TObjArray *fChannelArray = nullptr;

        bool fUseFPNData = true;
        double fFPNData[512];
        int fNumFPNData = 0;
        double fFluctuationThreshold = 100;
        
        LKChannelBuffer *fChannelRef = nullptr;

    ClassDef(LKBackgroundAnalyzer,1);
};

#endif
