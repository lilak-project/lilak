#ifndef LKPHYSICALPAD_HH
#define LKPHYSICALPAD_HH

#include "LKChannel.h"
#include "LKHit.h"
#include "LKHitArray.h"
#include "LKVector3.h"

#include "TObject.h"
#include "TH1D.h"
#include "TVector2.h"

#include <vector>
using namespace std;

class LKPhysicalPad : public LKChannel
{
    public:
        LKPhysicalPad() { Clear(); }
        virtual ~LKPhysicalPad() {}

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "") const;
        virtual bool IsSortable() const { return true; }
        virtual int Compare(const TObject *obj) const;

        void SetPadID(int id) { fID = id; }
        void SetPlaneID(int id) { fPlaneID = id; }
        void SetCoboID(int id) { fCoboID = id; }
        void SetAsadID(int id) { fAsadID = id; }
        void SetAgetID(int id) { fAgetID = id; }
        void SetChannelID(int id) { fChanID = id; }
        void SetTime(double time) { fTime = time; }
        void SetEnergy(double energy) { fEnergy = energy; }
        void SetPedestal(double pedestal) { fPedestal = pedestal; }
        void SetNoiseAmp(double pedestal) { fNoiseAmp = pedestal; }
        void SetPosition(LKVector3 pos) { fPosition = pos; }
        void SetPosition(double i, double j) { fPosition.SetI(i); fPosition.SetJ(j); }
        void SetSectionLayerRow(int section, int layer, int row) { fSection = section; fLayer = layer; fRow = row; }
        void SetSection(int section) { fSection = section; }
        void SetLayer(int layer) { fLayer = layer; }
        void SetRow(int row) { fRow = row; }
        void SetDataIndex(int idx) { fDataIndex = idx; }
        void SetSortValue(double value) { fSortValue = value; }

        int GetPadID() const { return fID; }
        int GetPlaneID() const { return fPlaneID; }
        int GetCoboID() const { return fCoboID; }
        int GetAsadID() const { return fAsadID; }
        int GetAgetID() const { return fAgetID; }
        int GetChanID() const { return fChanID; }
        int GetChannelID() const { return fChanID; }
        double GetTime() const { return fTime; }
        double GetEnergy() const { return fEnergy; }
        double GetPedestal() const { return fPedestal; }
        double GetNoiseAmp() const { return fNoiseAmp; }
        LKVector3 GetPosition() const { return fPosition; }
        double GetI() const { return fPosition.I(); }
        double GetJ() const { return fPosition.J(); }
        double GetK() const { return fPosition.K(); }
        double GetX() const { return fPosition.X(); }
        double GetY() const { return fPosition.Y(); }
        double GetZ() const { return fPosition.Z(); }
        int GetSection() const { return fSection; }
        int GetLayer() const { return fLayer; } 
        int GetRow() const { return fRow; }
        int GetDataIndex() const { return fDataIndex; }
        double GetSortValue() { return fSortValue; }
        int GetCAAC() const  { return fCoboID*10000+fAsadID*1000+fAgetID*100+fChanID; }

        void AddPadCorner(double i, double j) { fPadCorners.push_back(TVector2(i,j)); }
        vector<TVector2> *GetPadCorners() { return &fPadCorners; }

        void AddHit(LKHit *hit) { fHitArray.push_back(hit); }
        int GetNumHits() const { return fHitArray.size(); }
        LKHit *GetHit(int idx) { return fHitArray.at(idx); }
        void ClearHits() { fHitArray.clear(); }
        LKHit* PullOutNextFreeHit();
        void PullOutHits(LKHitArray *hits);
        void PullOutHits(vector<LKHit *> *hits);

    protected:
        int fPlaneID = 0;
        int fCoboID = -1;
        int fAsadID = -1;
        int fAgetID = -1;
        int fChanID = -1;
        int fSection = -1;
        int fLayer = -1;
        int fRow = -1;
        int fDataIndex = 1;
        double fTime = -1;
        double fEnergy = -1;
        double fPedestal = -1;
        double fNoiseAmp = -1;

        LKVector3 fPosition = LKVector3(LKVector3::kZ);
        vector<TVector2> fPadCorners;

        vector<LKHit *> fHitArray; //!
        double fSortValue = -1; //!

        ClassDef(LKPhysicalPad, 1)
};

#endif
