#ifndef GETCHANNELMAPDATA_HH
#define GETCHANNELMAPDATA_HH

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

class GETChannelMapData : public LKContainer
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

        GETChannelMapData() {}

        virtual void Print(Option_t *option="") const;

        virtual bool SetValue(TString keyword, TString value);

    ClassDef(GETChannelMapData,1)
};

#endif

#ifndef GETCHANNELKEY_HH
#define GETCHANNELKEY_HH

#include <unordered_map>
#include <tuple>

struct GETChannelKey {
    short cobo;
    short asad;
    short aget;
    short chan;

    bool operator==(const GETChannelKey &other) const {
        return std::tie(cobo, asad, aget, chan) == std::tie(other.cobo, other.asad, other.aget, other.chan);
    }
};

namespace std {
    template <>
    struct hash<GETChannelKey> {
        std::size_t operator()(const GETChannelKey& key) const {
            return ((std::hash<short>()(key.cobo) << 1) ^
                    (std::hash<short>()(key.asad) << 2) ^
                    (std::hash<short>()(key.aget) << 3) ^
                    (std::hash<short>()(key.chan) << 4));
        }
    };
}

#endif
