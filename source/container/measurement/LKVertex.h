#ifndef LKVERTEX_HH
#define LKVERTEX_HH

#include "TVector3.h"

#include "LKHit.h"
#include "LKTracklet.h"

#include <vector>
#include <iostream>
using namespace std;

class LKVertex : public LKHit
{
    public:
        LKVertex();
        virtual ~LKVertex() { Clear(); }

        virtual void Clear(Option_t *option = "");
        virtual void Print(Option_t *option = "at") const;
        virtual void Copy (TObject &object) const;

        void AddTrack(LKTracklet* track);

        vector<Int_t>       *GetTrackIDArray() { return &fTrackIDArray; }
        vector<LKTracklet*> *GetTrackArray()   { return &fTrackArray; }

        Int_t GetNumTracks() const { return fTrackIDArray.size(); }

#ifdef ACTIVATE_EVE
        virtual bool DrawByDefault();
        virtual bool IsEveSet();
        virtual TEveElement *CreateEveElement();
        virtual void AddToEveSet(TEveElement *eveSet, Double_t scale=1);
#endif

    private:
        vector<Int_t>       fTrackIDArray;
        vector<LKTracklet*> fTrackArray; //!

        ClassDef(LKVertex, 1)
};

#endif
