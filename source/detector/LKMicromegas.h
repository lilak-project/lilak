#ifndef LKMICROMEGAS_HH
#define LKMICROMEGAS_HH

#include "LKEvePlane.h"
#include "LKPad.h"
#include "TPad.h"

class LKMicromegas : public LKEvePlane
{
    public:
        LKMicromegas();
        LKMicromegas(const char *name, const char *title);
        virtual ~LKMicromegas() {};

        //virtual void Clear(Option_t *option = "");
        //virtual void Print(Option_t *option = "") const;
        virtual bool Init();

        virtual TVector3 GetPositionError(int padID);

    public:
        virtual void FillDataToHistEventDisplay2(Option_t *option="");

        virtual TPad* Get3DEventPad();

        virtual TH2* GetHistEventDisplay1(Option_t *option="-1");
        virtual TH2* GetHistEventDisplay2(Option_t *option="-1");
        virtual TH1D* GetHistChannelBuffer();

        virtual int FindPadID(int cobo, int asad, int aget, int chan);
        virtual LKPad* FindPad(int cobo, int asad, int aget, int chan);
        virtual int FindPadIDFromHistEventDisplay1Bin(int hbin);
        virtual int FindZFromHistEventDisplay2Bin(int hbin);

        virtual void ClickedEventDisplay2(double xOnClick, double yOnClick);
        virtual void UpdateEventDisplay1();
        virtual void UpdateEventDisplay2();
        virtual void UpdateChannelBuffer();

    protected:
        bool fUsePixelSpace = false;

        int fNX = 64;
        double fX1;
        double fX2;
        int fNY = 512;
        double fY2;
        double fY1;
        int fNZ = 72;
        double fZ1;
        double fZ2;
        double fDZPad = 4;
        double fDXPad = 4;

        TString fMappingFileName;
        int fNumCobo = 6;
        int fNumAsad = 4;
        int fNumAget = 4;
        int fNumChan = 68;

        int ****fMapCAACToPadID; ///< [cobo][asad][aget][chan] to pad-id mapping
        int *fMapBin1ToPadID; ///< histogram bin to pad-id mapping
        int *fMapBin2ToIZ; ///< histogram bin to pad-id mapping

        int fCurrentView = 1;
        double fThreshold = 300;
        int fSelIZ = -1;

    ClassDef(LKMicromegas, 1)
};

#endif
