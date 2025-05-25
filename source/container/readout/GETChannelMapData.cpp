#include "GETChannelMapData.h"
#include "LKLogger.h"

ClassImp(GETChannelMapData)

void GETChannelMapData::Print(Option_t *option) const
{
    double xx = 0;
    double yy = 0;
    double zz = 0;
    for (auto i=0; i<n; ++i) {
        xx += x[i];
        yy += y[i];
        zz += z[i];
    }
    xx = xx/n;
    yy = yy/n;
    zz = zz/n;
    e_cout << id << " " << cobo << " " << asad << " " << aget << " " << chan << " " << xx << " " << yy << " " << zz << std::endl;
}

bool GETChannelMapData::SetValue(TString keyword, TString value)
{
    if      (keyword=="id")         id = value.Atoi();
    else if (keyword=="cobo")       cobo = value.Atoi();
    else if (keyword=="asad")       asad = value.Atoi();
    else if (keyword=="aget")       aget = value.Atoi();
    else if (keyword=="chan")       chan = value.Atoi();
    else if (keyword.Index("x")==0) { keyword.Remove(0,1); int i = keyword.Atoi(); x[i] = value.Atof(); if (n<i+1) n = i+1; }
    else if (keyword.Index("y")==0) { keyword.Remove(0,1); int i = keyword.Atoi(); y[i] = value.Atof(); if (n<i+1) n = i+1; }
    else if (keyword.Index("z")==0) { keyword.Remove(0,1); int i = keyword.Atoi(); z[i] = value.Atof(); if (n<i+1) n = i+1; }
    else if (keyword=="phi1")       phi1 = value.Atof();
    else if (keyword=="phi2")       phi2 = value.Atof();
    else if (keyword=="theta1")     theta1 = value.Atof();
    else if (keyword=="theta2")     theta2 = value.Atof();
    else
        return false;
    return true;
}
