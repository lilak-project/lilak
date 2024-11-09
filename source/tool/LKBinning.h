#include "TNamed.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TGraph.h"
#include "TGraphErrors.h"

class LKBinning : public TNamed
{
    private:
        int fN = 0; ///< number of bins
        double fMin = 0; ///< lower bound
        double fMax = 0; ///< upper bound
        double fBinWidth = 0; ///< LKBinning space width
        int fIterationIndex = 0; ///< index for iteration
        double fValue = 0; ///< fValue will be set after iteration using next(), back(), nextb()

    private:
        LKBinning(LKBinning const & binn);
        LKBinning(const char* name, const char* title, int n=-1, double min=0, double max=0, double w=-1);
        LKBinning(int n=-1, double min=0, double max=0, double w=-1) : LKBinning("", "", n, min, max, w) {}
        LKBinning(TH1 *hist, int i=1);
        LKBinning(TGraph *graph, int i=1);
        void operator=(const LKBinning binn); ///< copy LKBinning

        void Init();
        void SetN(double n); ///< set number of the bins
        void SetW(double w); ///< set width of the bin
        void SetMin(double min); ///< set min
        void SetMax(double max); ///< set max
        void SetMM(double min, double max); ///< set min, max
        void SetNMM(int n, double min, double max); ///< set n, min, max
        void SetMMW(double min, double max, double w); ///< set min, max, w

        void Reset();
        void End();
        bool Next();
        bool Back();

        int GetBin(double invalue) const; ///< find bin corresponding to invalue
        double GetLowEdge(int i=1) const; ///< return low edge of the i-bin (i=1~n)
        double GetUpEdge(int i=-1) const; ///< return high edge of the i-bin (i:1~n)
        double GetBinCenter(int bin) const; ///< find bin center fValue
        double GetCenter() const;
        double GetFullWidth() const;
        TString Print(bool show=true) const;

    ClassDef(LKBinning,1);
};
