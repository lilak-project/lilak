#ifndef LKBINNING1_HH
#define LKBINNING1_HH

#include "TNamed.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TArrayD.h"
#include "TArrayI.h"
#include "TRandom.h"

class LKBinning;

class LKBinning1 : public TNamed
{
    public:
        int fNX = 1;
        double fX1 = 0;
        double fX2 = 0;
        double fWX = 0;

    private:
        int fIterationIndex = -1; //! < index for iteration
        double fValue = 0; //! < fValue will be set after iteration using next(), back(), nextb()

    public:
        LKBinning1(LKBinning1 const & binn);
        LKBinning1() : LKBinning1("", "", 1, 0, 0) {}
        LKBinning1(const char* name, const char* title, int nx, double x1, double x2);
        LKBinning1(int nx, double x1, double x2) : LKBinning1("", "", nx, x1, x2) {}
        LKBinning1(double x1, double x2) : LKBinning1("", "", 100, x1, x2) {}
        LKBinning1(TH1 *hist);
        LKBinning1(TGraph *graph);
        void operator=(const LKBinning1 binn); ///< copy LKBinning1
        LKBinning operator*(const LKBinning1 binn); ///< make xy binning x with this binning and y with input binning

        TH1D* NewH1(TString name="", TString title="");

        int    GetNX() const { return fNX; }
        double GetX1() const { return fX1; }
        double GetX2() const { return fX2; }
        double GetWX() const { return fWX; }

        int    n() const { return fNX; }
        double w() const { return fWX; }
        int    nx() const { return fNX; }
        double wx() const { return fWX; }
        double x1() const { return fX1; }
        double x2() const { return fX2; }

        bool IsInside(double x) { if (x>=fX1&&x<fX2) return true; return false; }
        double GetRandomUniform()  { return gRandom -> Uniform(fX1,fX2); }

        void SetBinning(TH1 *hist, int i=0);
        void SetBinning(TGraph *graph);

        void SetN(double nx) { SetNX(nx); }
        void SetW(double wx) { SetWX(wx); }
        void SetNX(double nx);
        void SetWX(double wx);
        void SetX1(double x1);
        void SetX2(double x2);
        void SetXMM(double x1, double x2);
        void SetXNMM(int nx, double x1, double x2);
        void SetXMMW(double x1, double x2, double w);

        void Reset();
        void End();
        bool Next();
        bool Back();
        double GetItValue() const { return fValue; }
        double GetItCenter() const { return fValue+0.5*fWX; }
        int GetItIndex() const { return fIterationIndex; }

        int FindIndex(double value) const;
        int FindBin(double value) const; ///< find bin corresponding to value
        double GetIdxLowEdge(int idx) const; ///< return low edge of the i-idx (i=0~nx-1)
        double GetIdxUpEdge(int idx) const; ///< return high edge of the i-idx (i:0~nx-1)
        double GetIdxCenter(int idx) const; ///< find bin center value of the i-idx (i:0~nx-1)
        double GetBinLowEdge(int bin) const; ///< return low edge of the i-bin (i=1~nx)
        double GetBinUpEdge(int bin) const; ///< return high edge of the i-bin (i:1~nx)
        double GetBinCenter(int bin) const; ///< find bin center value of the i-idx (i:1~nx)

        double GetCenter() const;
        double GetFullWidth() const;
        TString Print(bool show=true) const;
        bool IsEmpty() const;

        TArrayD* MakeArrayD() { return (new TArrayD(fNX)); }
        TGraphErrors* MakeGraph(TArrayD* array);

    ClassDef(LKBinning1,1);
};

#endif
