#include "GETParameters.h"
#include "LKLogger.h"

GETParameters::GETParameters()
{
}

GETParameters::~GETParameters()
{
}

void GETParameters::ClearParameters()
{
    fFrameNo = -1;
    fDecayNo = -1;
    fDetType = -1;
    fCobo = -1;
    fAsad = -1;
    fAget = -1;
    fChan = -1;
}

void GETParameters::PrintParameters(TString option) const
{
    if (option.Index("!title")<0)
        e_info << "[GETParameters]" << std::endl;
    e_info << "- DCAAC: " << fDetType << " " << fCobo << " " << fAsad << " " << fAget << " " << fChan << std::endl;
}
