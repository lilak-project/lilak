#ifndef LKDRAWINGCLUSTER_HH
#define LKDRAWINGCLUSTER_HH

#include "TObjArray.h"

class LKDrawingCluster : public TObjArray
{
    public:
        LKDrawingCluster();
        ~LKDrawingCluster() {}

        virtual void Draw(Option_t *option="");

    ClassDef(LKDrawingCluster, 1)
};

#endif
