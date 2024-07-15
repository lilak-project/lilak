#ifndef GETCHANNEL_HH
#define GETCHANNEL_HH

#include <vector>

#include "TH1D.h"
#include "TH2D.h"
#include "TGraph.h"

#include "LKHit.h"
#include "LKLogger.h"
#include "LKBufferI.h"
#include "LKChannel.h"
#include "LKHitArray.h"
#include "LKContainer.h"
#include "TClonesArray.h"
#include "GETParameters.h"

/**
 * Raw event data from GET
 */
class GETChannel : public LKChannel, public GETParameters
{
    public:
        GETChannel();
        virtual ~GETChannel() { ; }

        virtual void Clear(Option_t *option="");
        virtual void Copy(TObject &object) const;
        virtual void Print(Option_t *option="") const;

        virtual const char* GetName() const;
        virtual const char* GetTitle() const;

        virtual TObject *Clone(const char *newname="") const;

        virtual void Draw(Option_t *option="");

        virtual TString MakeTitle() { return Form("%d %d %d %d", fCobo, fAsad, fAget, fChan); }

        virtual TH1D *GetHist(TString name="");
        void FillHist(TH1* hist);
        void FillGraph(TGraph* graph);

        TGraph *GetHitGraph();

        /////////////////////////////////////////////////////////////////////////////////////////////////
        int* GetWaveformY() { return fBufferRawSig.GetArray(); }
        int* GetBufferArray() { return fBufferRawSig.GetArray(); }
        LKBufferI GetBuffer() { return fBufferRawSig; }
        virtual double GetIntegral(double pedestal=-1., bool inverted=false);

        void SetWaveformY(const int* array) { fBufferRawSig.SetArray(array); }
        void SetWaveformY(const unsigned int* array) { fBufferRawSig.SetArray(array); }
        void SetWaveformY(std::vector<unsigned int> array) { fBufferRawSig.SetArray(array); }
        void SetBuffer(LKBufferI buffer) { fBufferRawSig.SetBuffer(buffer); }

        /////////////////////////////////////////////////////////////////////////////////////////////////
        LKHitArray *GetHitArray() { return &fHitArray; }
        void AddHit(LKHit *hit) { fHitArray.AddHit(hit); }
        int GetNumHits() const { return fHitArray.GetNumHits(); }
        LKHit *GetHit(int idx) { return fHitArray.GetHit(idx); }
        void ClearHits() { fHitArray.Clear(); }
        LKHit* PullOutNextFreeHit();
        void PullOutHits(LKHitArray *hits);
        void PullOutHits(vector<LKHit *> *hits);

        /////////////////////////////////////////////////////////////////////////////////////////////////
        void SubtractArray(int*    buffer)               { fBufferRawSig.SubtractArray(buffer); }
        void SubtractArray(double* buffer)               { fBufferRawSig.SubtractArray(buffer); }
        void SubtractArray(int*    buffer, double scale) { fBufferRawSig.SubtractArray(buffer, scale); }
        void SubtractArray(double* buffer, double scale) { fBufferRawSig.SubtractArray(buffer, scale); }

        double GetScale(int* buffer) { return fBufferRawSig.GetScale(buffer); }
        double GetScale(double* buffer) { return fBufferRawSig.GetScale(buffer); }
        double CalculateGroupFluctuation(int numGroups=8, int tb2=512) { return fBufferRawSig.CalculateGroupFluctuation(numGroups,tb2); }
        void   GetGroupMeanStdDev(int numGroups, int tb2, double* pedestalGroup, double* stddevGroup) { fBufferRawSig.GetGroupMeanStdDev(numGroups, tb2, pedestalGroup, stddevGroup); }

    protected:
        LKBufferI  fBufferRawSig;

        LKHitArray fHitArray; //!
        TGraph*    fGraphHit = nullptr; //!

    ClassDef(GETChannel,4);
};

#endif
