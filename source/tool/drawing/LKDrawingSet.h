#ifndef LKDRAWINGSET_HH
#define LKDRAWINGSET_HH

#include "TObjArray.h"
#include "LKDrawing.h"
#include "LKDrawingGroup.h"

class LKDrawingSet : public TObjArray
{
    public:
        LKDrawingSet();
        ~LKDrawingSet() {}

        virtual void Draw(Option_t *option="");
        void AddDrawing(LKDrawing* drawing);
        void AddGroup(LKDrawingGroup* group);
        void AddSet(LKDrawingSet* set);

    ClassDef(LKDrawingSet, 1)
};

#endif
