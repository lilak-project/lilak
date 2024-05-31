#ifndef LKRECTANGULARATTPC_HH
#define LKRECTANGULARATTPC_HH

#include "LKLogger.h"
#include "LKDetector.h"

class LKATTPC : public LKDetector
{
    public:
        LKATTPC();
        LKATTPC(const char *name, const char *title);
        virtual ~LKATTPC() { ; }

        virtual bool Init();
        virtual bool GetEffectiveDimension(Double_t &x1, Double_t &y1, Double_t &z1, Double_t &x2, Double_t &y2, Double_t &z2);

    private:
        bool fUsePixelSpace;

    ClassDef(LKATTPC,1);
};

#endif
