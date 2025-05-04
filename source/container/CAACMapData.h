#ifndef CAACMAPDATA_HH
#define CAACMAPDATA_HH

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>

#include "TVector3.h"
#include "TString.h"
#include "TObject.h"

#include "LKContainer.h"

class CAACMapData : public LKContainer
{
    public:
        short id = -1;
        short cobo = -1;
        short asad = -1;
        short aget = -1;
        short chan = -1;
        bool isrec = true;
        short n = 0;
        double x[8] = {0};
        double y[8] = {0};
        double z[8] = {0};
        double phi1;
        double phi2;
        double theta1;
        double theta2;

        CAACMapData() {}

        virtual void Print(Option_t *option="") const;

        virtual bool SetValue(TString keyword, TString value);

    ClassDef(CAACMapData,1)
};

#endif
