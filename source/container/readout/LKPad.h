#ifndef LKPAD_HH
#define LKPAD_HH

#include "GETChannel.h"
#include "LKBufferI.h"
#include "LKBufferD.h"

#include "TH1D.h"
#include "TVector2.h"

#include <vector>
using namespace std;

class LKPad : public GETChannel
{
    public:
        LKPad() { Clear(); }
        virtual ~LKPad() {}

        virtual void Clear(Option_t *option="");
        virtual void Print(Option_t *option="") const;
        virtual bool IsSortable() const { return true; }
        virtual int  Compare(const TObject *obj) const;
        virtual void Draw(Option_t *option="out:hit"); ///< in(raw), out(shaped), hit
        void DrawHits();

        void SetPad(LKPad* padRef); // copy configuration (position, ids) from padRef
        void CopyPadData(LKPad* padRef); // copy data from padRef

        /////////////////////////////////////////////////////////////////////////////////////////////////
        void FillBufferRawSig(int t, double val, int id=-1) { fActive = true; fBufferRawSig.Fill(t,val); }
        void SetBufferRawSig(int*    array) { fBufferRawSig.SetArray(array); }
        void SetBufferShaped(double* array) { fBufferShaped.SetArray(array); }
        void SetBufferRawSig(LKBufferI buffer) { fBufferRawSig.SetBuffer(buffer); }
        void SetBufferShaped(LKBufferD buffer) { fBufferShaped.SetBuffer(buffer); }

        int*    GetArrayRawSig() { return fBufferRawSig.GetArray(); }
        double* GetArrayShaped() { return fBufferShaped.GetArray(); }
        LKBufferI GetBufferRawSig() { return fBufferRawSig; }
        LKBufferD GetBufferShaped() { return fBufferShaped; }

        /////////////////////////////////////////////////////////////////////////////////////////////////
        void SetPlaneID(int id) { fPlaneID = id; }
        void SetPosition(LKVector3 pos) { fPosition = pos; }
        void SetPosition(double i, double j) { fPosition.SetI(i); fPosition.SetJ(j); }
        void SetPosError(LKVector3 err) { fPosError = err; }
        void SetPosError(double i, double j) { fPosError.SetI(i); fPosError.SetJ(j); }
        void SetSectionLayerRow(int section, int layer, int row) { fSection = section; fLayer = layer; fRow = row; }
        void SetSection(int section) { fSection = section; }
        void SetLayer(int layer) { fLayer = layer; }
        void SetRow(int row) { fRow = row; }
        void SetDataIndex(int idx) { fDataIndex = idx; }
        void SetSortValue(double value) { fSortValue = value; }

        int GetPlaneID() const { return fPlaneID; }
        LKVector3 GetPosition() const { return fPosition; }
        double GetI() const { return fPosition.I(); }
        double GetJ() const { return fPosition.J(); }
        double GetK() const { return fPosition.K(); }
        double GetX() const { return fPosition.X(); }
        double GetY() const { return fPosition.Y(); }
        double GetZ() const { return fPosition.Z(); }
        LKVector3 GetPosError() const { return fPosError; }
        double GetIError() const { return fPosError.I(); }
        double GetJError() const { return fPosError.J(); }
        double GetKError() const { return fPosError.K(); }
        double GetXError() const { return fPosError.X(); }
        double GetYError() const { return fPosError.Y(); }
        double GetZError() const { return fPosError.Z(); }
        int GetSection() const { return fSection; }
        int GetLayer() const { return fLayer; }
        int GetRow() const { return fRow; }
        int GetDataIndex() const { return fDataIndex; }
        double GetSortValue() { return fSortValue; }

        /////////////////////////////////////////////////////////////////////////////////////////////////
        void SetActive(bool active=true) { fActive = active; }
        bool IsActive() const { return fActive; }

        /////////////////////////////////////////////////////////////////////////////////////////////////
        void SetHist(TH1D *hist, Option_t *option="out:hit");
        TH1D *GetHist(Option_t *option="");

        /////////////////////////////////////////////////////////////////////////////////////////////////
        void AddNeighborPad(LKPad *pad) { fNeighborPadArray.push_back(pad); }
        vector<LKPad *> *GetNeighborPadArray() { return &fNeighborPadArray; }

        /////////////////////////////////////////////////////////////////////////////////////////////////
        void AddPadCorner(double i, double j) { fPadCorners.push_back(TVector2(i,j)); }
        vector<TVector2> *GetPadCorners() { return &fPadCorners; }

        /////////////////////////////////////////////////////////////////////////////////////////////////
        bool IsGrabed() const { return fGrabed; }
        void Grab() { fGrabed = true; }
        void LetGo() { fGrabed = false; }

    private:
        LKBufferD fBufferShaped;

        int      fPlaneID = 0;
        int      fSection = -1;
        int      fLayer = -1;
        int      fRow = -1;
        int      fDataIndex = 1;

        LKVector3 fPosition = LKVector3(LKVector3::kZ);
        LKVector3 fPosError = LKVector3(LKVector3::kZ);
        vector<TVector2> fPadCorners;

        double  fSortValue = -1; //!
        bool    fActive = false; //!
        bool    fGrabed = false; //!

        TH1D*   fHist = nullptr; //!
        vector<LKPad *> fNeighborPadArray; //!

    ClassDef(LKPad, 2)
};

#endif
