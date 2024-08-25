#ifndef LKDRAWINGCLUSTER_HH
#define LKDRAWINGCLUSTER_HH

#include "TObjArray.h"
#include "LKDrawing.h"
#include "LKDrawingGroup.h"

class LKDrawingCluster : public TObjArray
{
    public:
        LKDrawingCluster();
        ~LKDrawingCluster() {}

        virtual void Draw(Option_t *option="");
        void AddDrawing(LKDrawing* drawing);
        void AddGroup(LKDrawingGroup* group);
        void AddCluster(LKDrawingCluster* cluster);

    ClassDef(LKDrawingCluster, 1)
};

#endif
