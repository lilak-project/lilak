#ifndef LKSILICONARRAY_HH
#define LKSILICONARRAY_HH

#include "LKEvePlane.h"
#include "LKPad.h"
#include "TPad.h"
#include "TH2Poly.h"
#include "LKParameterContainer.h"
#include "LKSiDetector.h"
#include "LKSiChannel.h"
#include "LKSiliconMapping.h"
#include <climits>

class LKSiliconArray : public LKEvePlane
{
    public:
        LKSiliconArray();
        LKSiliconArray(const char *name, const char *title);
        virtual ~LKSiliconArray() {}

        virtual bool Init() override;
        virtual bool EndOfRun() override;

        int GetNumSiDetectors() const { return fDetectorArray -> GetEntries(); }
        LKSiDetector *GetSiDetector(int idx);
        LKSiDetector *GetSiDetector(int cobo, int asad, int aget, int chan);
        int FindSiDetectorID(int cobo, int asad, int aget, int chan);
        int FindEPairDetectorID(int det);

        LKSiChannel *GetSiChannel(int idx);
        LKSiChannel *GetSiChannel(int cobo, int asad, int aget, int chan);
        int FindPadID(int cobo, int asad, int aget, int chan) override;
        int FindSiChannelID(int cobo, int asad, int aget, int chan) { return FindPadID(cobo, asad, aget, chan); }
        bool SetSiChannelData(LKSiChannel* siChannel, GETChannel* channel);

        bool AddUserDrawingArray(TString label, int detID, int joID, TObjArray* userDrawingArray, int leastNDraw=1);
        bool AddUserDrawingArray(TString label, int detID, TObjArray* userDrawingArray, int leastNDraw=1) { return AddUserDrawingArray(label, detID, -1, userDrawingArray, leastNDraw); }
        bool AddUserDrawingArray(TString label, TObjArray* userDrawingArray, int leastNDraw=1) { return AddUserDrawingArray(label, -1, -1, userDrawingArray, leastNDraw); }
        bool AddDrawing(TObject* drawing, TString label, int detID);

        void FireStrip(int det, int side, int strip, double energy);
        void ClearFiredFlags();
        int GetNumFiredDetectors();
        LKSiDetector* GetFiredDetector(int iFired);

    protected:
        void ResetEPairMapping();
        void SetEPairMapping(int pair, int i, int detID);

        LKSiliconMapping fSiliconMapping;
        TString fMappingFileName;
        TString fDetectorParName;
        int fNumCobo = 6;
        int fNumAsad = 4;
        int fNumAget = 4;
        int fNumChan = 68;

        int ****fMapCAACToChannelIndex = nullptr;
        int ****fMapCAACToDetectorIndex = nullptr;

        int fMaxLayerIndex = -INT_MAX;
        double fMinPhi = DBL_MAX;
        double fMaxPhi = -DBL_MAX;
        double fLayerYScale = 100;
        double fLayerYOffset = 0.05;

        TObjArray *fDetectorArray = nullptr;
        TObjArray *fChannelArray = nullptr;
        LKParameterContainer fDetectorTypeArray;

        int fMaxDetectors = 40;
        TObjArray* fUserDrawingArray = nullptr;
        int ****fUserDrawingArrayIndex = nullptr;
        int ****fUserDrawingLeastNDraw = nullptr;
        TString fUserDrawingName[4][10];
        int fUserDrawingType[4][10];
        TGraph* fGSelJOSideDisplay = nullptr;

        int fdEEPairMapping[40][2];

    ClassDefOverride(LKSiliconArray, 1)
};

#endif
