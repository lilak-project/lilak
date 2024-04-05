#ifndef LKNOISEANALYZER_HH
#define LKNOISEANALYZER_HH

#include "TObject.h"
#include "LKLogger.h"
#include "GETChannel.h"
#include "TObjArray.h"

class LKNoiseAnalyzer : public TObject
{
    public:
        LKNoiseAnalyzer() {}
        virtual ~LKNoiseAnalyzer() {}

        bool Init();
        void Clear(Option_t *option="");

        double* GetFPNData() { return fFPNData; }

        void Add(GETChannel* channel);
        void AddFPNData(GETChannel* channel);

        int  FindRefChannelID();
        bool TestValidity() { return (FindRefChannelID() > 0); }
        bool Analyze();

    private:
        TObjArray *fChannelArray = nullptr;

        bool fUseFPNData = true;
        double fFPNData[512];
        int fNumFPNData = 0;
        double fFluctuationThreshold = 100;
        
        GETChannel *fChannelRef = nullptr;

    ClassDef(LKNoiseAnalyzer,1);
};

#endif
