#ifndef LKBINNING_HH
#define LKBINNING_HH

#include "TNamed.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TArrayD.h"
#include "TArrayI.h"
#include "TRandom.h"

class LKBinning : public TNamed
{
    public:
        int fNX = 1;
        double fX1 = 0;
        double fX2 = 0;
        double fWX = 0;

        int fNY = 1;
        double fY1 = 0;
        double fY2 = 0;
        double fWY = 0;

    private:
        int fIterationIndex = 0; //! < index for iteration
        double fValue = 0; //! < fValue will be set after iteration using next(), back(), nextb()

    public:
        LKBinning(LKBinning const & binn);
        LKBinning() : LKBinning("", "", 1, 0, 0, 1, 0, 0) {}
        LKBinning(const char* name, const char* title, int nx, double x1, double x2, int ny=1, double y1=0, double y2=0);
        LKBinning(int nx, double x1, double x2, int ny=1, double y1=0, double y2=0) : LKBinning("", "", nx, x1, x2, ny, y1, y2) {}
        LKBinning(double x1, double x2) : LKBinning("", "", 100, x1, x2, 1, 0, 0) {}
        LKBinning(TH1 *hist);
        LKBinning(TGraph *graph);
        void operator=(const LKBinning binn); ///< copy LKBinning
        LKBinning operator*(const LKBinning binn); ///< make xy binning x with this binning and y with input binning

        TH1D* NewH1(TString name="", TString title="");
        TH2D* NewH2(TString name="", TString title="");

        int    GetNX() const { return fNX; }
        double GetX1() const { return fX1; }
        double GetX2() const { return fX2; }
        double GetWX() const { return fWX; }
        int    GetNY() const { return fNY; }
        double GetY1() const { return fY1; }
        double GetY2() const { return fY2; }
        double GetWY() const { return fWY; }

        int    n() const { return fNX; }
        double w() const { return fWX; }
        int    nx() const { return fNX; }
        double wx() const { return fWX; }
        double x1() const { return fX1; }
        double x2() const { return fX2; }
        int    ny() const { return fNY; }
        double wy() const { return fWY; }
        double y1() const { return fY1; }
        double y2() const { return fY2; }

        bool IsInside(double x) { if (x>=fX1&&x<fX2) return true; return false; }
        bool IsInside(double x, double y) { if ((x>=fX1&&x<fX2)&&(y>=fY1&&y<fY2)) return true; return false; }
        double GetRandomUniform()  { return gRandom -> Uniform(fX1,fX2); }
        double GetRandomUniformY() { return gRandom -> Uniform(fY1,fY2); }

        void SetBinning(TH1 *hist);
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

        void SetNY(double ny);
        void SetWY(double wy);
        void SetY1(double y1);
        void SetY2(double y2);
        void SetYMM(double y1, double y2);
        void SetYNMM(int ny, double y1, double y2);
        void SetYMMW(double y1, double y2, double w);

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

        TArrayD* MakeArrayD() { return (new TArrayD(fNX)); }
        TGraphErrors* MakeGraph(TArrayD* array);

    ClassDef(LKBinning,1);
};

#endif
