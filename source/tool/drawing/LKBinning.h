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
#include "LKBinning1.h"
#include "LKDrawing.h"

class LKBinning : public TNamed
{
    public:
        LKBinning1 fBinningX;
        LKBinning1 fBinningY;
        LKBinning1 fBinningProjection;

    public:
        LKBinning() {}
        LKBinning(LKBinning const & binn);
        LKBinning(LKBinning1 const & binnx);
        LKBinning(LKBinning1 const & binnx, LKBinning1 const & binny);
        LKBinning(const char* name, const char* title, int nx, double x1, double x2, int ny=1, double y1=0, double y2=0);
        LKBinning(int nx, double x1, double x2, int ny=1, double y1=0, double y2=0) : LKBinning("", "", nx, x1, x2, ny, y1, y2) {}
        LKBinning(double x1, double x2) : LKBinning("", "", 100, x1, x2, 1, 0, 0) {}
        LKBinning(TH1 *hist);
        LKBinning(TGraph *graph);
        void operator=(const LKBinning binn); ///< copy LKBinning
        LKBinning operator*(const LKBinning binn); ///< make xy binning x with this binning and y with input binning

        TString Print(bool show=true) const;

        TH1D* NewH1(TString name="", TString title="");
        TH2D* NewH2(TString name="", TString title="");

        int    GetNX() const { return fBinningX.fNX; }
        double GetX1() const { return fBinningX.fX1; }
        double GetX2() const { return fBinningX.fX2; }
        double GetWX() const { return fBinningX.fWX; }
        double GetDX() const { return fBinningX.GetDX(); }
        int    GetNY() const { return fBinningY.fNX; }
        double GetY1() const { return fBinningY.fX1; }
        double GetY2() const { return fBinningY.fX2; }
        double GetWY() const { return fBinningY.fWX; }
        double GetDY() const { return fBinningY.GetDX(); }

        int    n()  const { return fBinningX.fNX; }
        double w()  const { return fBinningX.fWX; }
        double d()  const { return fBinningX.GetDX(); }
        int    nx() const { return fBinningX.fNX; }
        double wx() const { return fBinningX.fWX; }
        double dx() const { return fBinningX.GetDX(); }
        double x1() const { return fBinningX.fX1; }
        double x2() const { return fBinningX.fX2; }
        int    ny() const { return fBinningY.fNX; }
        double wy() const { return fBinningY.fWX; }
        double dy() const { return fBinningY.GetDX(); }
        double y1() const { return fBinningY.fX1; }
        double y2() const { return fBinningY.fX2; }

        double xmid() const { return 0.5*(fBinningX.fX1+fBinningX.fX2); }
        double ymid() const { return 0.5*(fBinningY.fX1+fBinningY.fX2); }

        bool IsInside(double x) { if (fBinningX.IsInside(x)) return true; return false; }
        bool IsInside(double x, double y) { if (fBinningX.IsInside(x)&&fBinningY.IsInside(y)) return true; return false; }
        double GetRandomUniform()  { return fBinningX.GetRandomUniform(); }
        double GetRandomUniformX() { return fBinningX.GetRandomUniform(); }
        double GetRandomUniformY() { return fBinningY.GetRandomUniform(); }

        void SetBinning(TH1 *hist);
        void SetBinning(TGraph *graph);

        void SetN (double nx) { fBinningX.SetNX(nx); }
        void SetW (double wx) { fBinningX.SetWX(wx); }
        void SetNX(double nx) { fBinningX.SetNX(nx); }
        void SetWX(double wx) { fBinningX.SetWX(wx); }
        void SetX1(double x1) { fBinningX.SetX1(x1); }
        void SetX2(double x2) { fBinningX.SetX2(x2); }
        void SetXMM(double x1, double x2) { fBinningX.SetXMM (x1,x2); }
        void SetXNMM(int nx, double x1, double x2) { fBinningX.SetXNMM(nx,x1,x2); }
        void SetXMMW(double x1, double x2, double w) { fBinningX.SetXMMW(x1,x2,w); }

        void SetNY(double ny) { fBinningY.SetNX(ny); }
        void SetWY(double wy) { fBinningY.SetWX(wy); }
        void SetY1(double y1) { fBinningY.SetX1(y1); }
        void SetY2(double y2) { fBinningY.SetX2(y2); }
        void SetYMM(double y1, double y2) { fBinningY.SetXMM (y1,y2); }
        void SetYNMM(int ny, double y1, double y2) { fBinningY.SetXNMM(ny,y1,y2); }
        void SetYMMW(double y1, double y2, double w) { fBinningY.SetXMMW(y1,y2,w); }

        void Reset() { fBinningX.Reset(); }
        void End ()  { fBinningX.End();   }
        bool Next()  { return fBinningX.Next();  }
        bool Back()  { return fBinningX.Back();  }
        double GetItValue () const { return fBinningX.GetItValue();  }
        double GetItCenter() const { return fBinningX.GetItCenter(); }
        int    GetItIndex () const { return fBinningX.GetItIndex();  }

        int    FindIndex (double val)  const { return fBinningX.FindIndex    (val); } 
        int    FindBin   (double val)  const { return fBinningX.FindBin      (val); }  ///< find bin corresponding to value
        double GetIdxLowEdge(int idx)  const { return fBinningX.GetIdxLowEdge(idx); }  ///< return low edge of the i-idx (i=0~nx-1)
        double GetIdxUpEdge (int idx)  const { return fBinningX.GetIdxUpEdge (idx); }  ///< return high edge of the i-idx (i:0~nx-1)
        double GetIdxCenter (int idx)  const { return fBinningX.GetIdxCenter (idx); }  ///< find bin center value of the i-idx (i:0~nx-1)
        double GetBinLowEdge(int bin)  const { return fBinningX.GetBinLowEdge(bin); }  ///< return low edge of the i-bin (i=1~nx)
        double GetBinUpEdge (int bin)  const { return fBinningX.GetBinUpEdge (bin); }  ///< return high edge of the i-bin (i:1~nx)
        double GetBinCenter (int bin)  const { return fBinningX.GetBinCenter (bin); }  ///< find bin center value of the i-idx (i:1~nx)
        double GetCenter()             const { return fBinningX.GetCenter();    }
        double GetFullWidth()          const { return fBinningX.GetFullWidth(); }
        double Lerp(double r)          const { return fBinningX.Lerp(r); }
        bool   IsEmpty()               const { return (fBinningX.IsEmpty() && fBinningY.IsEmpty()); }

        LKBinning1 GetBinningX() { return fBinningX; }
        LKBinning1 GetBinningY() { return fBinningY; }

        TArrayD* MakeArrayD() { return fBinningX.MakeArrayD(); }
        TGraphErrors* MakeGraph(TArrayD* array) { return fBinningX.MakeGraph(array); }

        void SetHistRange(TH1* hist);

        void SetProjectionBinningValues(int n_proj, double x1_proj=0, double x2_proj=0);
        void SetProjectionBinningValues(LKBinning  bnn) { SetProjectionBinningValues(bnn.nx(),bnn.x1(),bnn.x2()); }
        void SetProjectionBinningValues(LKBinning1 bnn) { SetProjectionBinningValues(bnn.nx(),bnn.x1(),bnn.x2()); }
        void SetProjectionBinningBins(int n_proj, int b1_proj=0, int b2_proj=0);
        void FindProjectionRange(int i_proj, int &bin1, int &bin2) const;
        LKDrawing* CreateProjectionXGrid();
        LKDrawing* CreateProjectionYGrid();
        TH1D* ProjectionX(TH2D* hist2, int i_proj);
        TH1D* ProjectionY(TH2D* hist2, int i_proj);
        LKBinning GetProjectionBinning() { return fBinningProjection; }
        LKBinning GetProjectionValueBinning() { return LKBinning(fBinningProjection.n(),GetIdxLowEdge(fBinningProjection.x1()),GetIdxUpEdge(fBinningProjection.x2())); }

        void ResetNextProjection();
        bool NextProjection() { return fBinningProjection.Next(); }
        TH1D* NextProjectionX(TH2D* hist2);
        TH1D* NextProjectionY(TH2D* hist2);
        int GetCurrentProjectionIt() const;
        double GetProjectionCenter  (int i_proj) const;
        double GetProjectionLowEdge (int i_proj) const;
        double GetProjectionUpEdge  (int i_proj) const;
        double GetProjectionBinWidth(int i_proj) const;
        double GetCurrentProjectionCenter  () const { return GetProjectionCenter  (fBinningProjection.GetItIndex()); }
        double GetCurrentProjectionLowEdge () const { return GetProjectionLowEdge (fBinningProjection.GetItIndex()); }
        double GetCurrentProjectionUpEdge  () const { return GetProjectionUpEdge  (fBinningProjection.GetItIndex()); }
        double GetCurrentProjectionBinWidth() const { return GetProjectionBinWidth(fBinningProjection.GetItIndex()); }

    ClassDef(LKBinning,1);
};

#endif
