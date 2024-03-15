#ifndef LKCHANNELBUFFER_HH
#define LKCHANNELBUFFER_HH

#include "LKChannel.h"
#include "TH1D.h"

class LKChannelBuffer : public LKChannel
{
    public:
        LKChannelBuffer();
        virtual ~LKChannelBuffer() {}
        void Clear(Option_t *option="");

        void Draw(Option_t *option="");
        TH1D* GetHist(TString name="");
        void FillHist(TH1D* hist);

        void SetChannelID(int value) { fChannelID = value; }
        void SetNoiseScale(double value) { fNoiseScale = value; }
        void SetTime(double value) { fTime = value; }
        void SetEnergy(double value) { fEnergy = value; }
        void SetPedestal(double value) { fPedestal = value; }
        void SetBuffer(double* buffer);
        void SetBuffer(int* buffer);

        void SubtractBuffer(double* buffer);
        void SubtractBuffer(double* buffer, double scale);
        double GetScale(double* buffer); // return [scale] where [scale] is fit parameter of fBuffer = [scale] x buffer.

        int GetChannelID() const { return fChannelID; }
        double GetNoiseScale() const { return fNoiseScale; }
        double GetTime() const { return fTime; }
        double GetEnergy() const { return fEnergy; }
        double GetPedestal() const { return fPedestal; }
        double* GetBuffer() { return fBuffer; }

        void GetGroupMeanStdDev(int numGroups, int tb2, double* pedestalGroup, double* stddevGroup);
        double CalculateGroupFluctuation(int numGroups=8, int tb2=512);
        //double CalculatePedestal(int numGroups=8, int tb2=512, double stdDevCut=50);

    private:
        int fChannelID = -1;
        double fNoiseScale = -1;
        double fTime = -1;
        double fEnergy = -1;
        double fPedestal = -1;
        double fBuffer[512];

        TH1D* fHist = nullptr; //!

    ClassDef(LKChannelBuffer,1);
};

#endif
