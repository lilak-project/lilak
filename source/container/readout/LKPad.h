#ifndef LKPAD_HH
#define LKPAD_HH

#include "LKPhysicalPad.h"
#include "LKHit.h"

#include "TH1D.h"
#include "TVector2.h"

#include <vector>
using namespace std;

class LKPad : public LKPhysicalPad
{
    public:
        LKPad() { Clear(); }
        virtual ~LKPad() {}

        virtual void Clear(Option_t *option="");
        virtual void Print(Option_t *option="") const;
        virtual int  Compare(const TObject *obj) const;
        virtual void Draw(Option_t *option="out:hit"); ///< in(raw), out(shaped), hit
        void DrawHits();

        void SetPad(LKPad* padRef); // copy configuration (position, ids) from padRef
        void CopyPadData(LKPad* padRef); // copy data from padRef

        void FillRawSigBuffer(int t, double val, int id=-1) { fActive = true; fRawSigBuffer[t] += val; }
        void SetRawSigBuffer(double* buffer) { memcpy(fRawSigBuffer, buffer, sizeof(double)*512); }
        void SetShapedBuffer(double* buffer) { memcpy(fShapedBuffer, buffer, sizeof(double)*512); }
        double* GetRawSigBuffer() { return fRawSigBuffer; }
        double* GetShapedBuffer() { return fShapedBuffer; }

        void SetBuffer(int* buffer) { for (auto tb=0; tb<512; ++tb) fShapedBuffer[tb] = buffer[tb]; }
        void SetBuffer(double* buffer) { memcpy(fShapedBuffer, buffer, sizeof(double)*512); }
        double* GetBuffer() { return fShapedBuffer; }

        void SetActive(bool active=true) { fActive = active; }
        bool IsActive() const { return fActive; }

        void SetHist(TH1D *hist, Option_t *option="out:hit");
        TH1D *GetHist(Option_t *option="");

        void AddNeighborPad(LKPad *pad) { fNeighborPadArray.push_back(pad); }
        vector<LKPad *> *GetNeighborPadArray() { return &fNeighborPadArray; }

        bool IsGrabed() const { return fGrabed; }
        void Grab() { fGrabed = true; }
        void LetGo() { fGrabed = false; }

    private:
        bool    fActive = false; //!
        bool    fGrabed = false; //!

        double  fRawSigBuffer[512];
        double  fShapedBuffer[512];

        vector<LKPad *> fNeighborPadArray; //!
        TH1D*   fHist = nullptr; //!

        //int fPlaneID = 0;
        //int fCoboID = -1;
        //int fAsadID = -1;
        //int fAgetID = -1;
        //int fChanID = -1;
        //int fSection = -1;
        //int fLayer = -1;
        //int fRow = -1;
        //int fDataIndex = 1;
        //double fTime = -1;
        //double fEnergy = -1;
        //double fPedestal = -1;
        //LKVector3 fPosition = LKVector3(LKVector3::kZ);
        //vector<TVector2> fPadCorners;
        //vector<LKPhysicalPad *> fNeighborPadArray; //!
        //vector<LKHit *> fHitArray; //!
        //double fSortValue = -1; //!

    ClassDef(LKPad, 1)
};

#endif
