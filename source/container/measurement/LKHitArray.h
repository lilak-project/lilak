#ifndef LKHITCLONESARRAY_HH
#define LKHITCLONESARRAY_HH

#include "TObjArray.h"
#include "TObject.h"
#include "TCanvas.h"
#include "TGraphErrors.h"

#include <vector>
#include <iomanip>
using namespace std;

#include "LKODRFitter.h"
#include "LKGeoLine.h"
#include "LKGeoPlane.h"
#include "LKGeoCircle.h"
#include "LKGeoHelix.h"
#include "LKVector3.h"

typedef LKVector3::Axis axis_t;

class LKHit;

class LKHitArray : public TObjArray
{
    private:
        Int_t fN = 0;  ///< Number of hits
        Double_t fW = 0;  ///< Sum of charge
        Double_t fEX  = 0;  ///< \<x>   Mean value of x
        Double_t fEY  = 0;  ///< \<y>   Mean value of y
        Double_t fEZ  = 0;  ///< \<z>   Mean value of z
        Double_t fEXX = 0;  ///< \<x*x> Mean value of x*x
        Double_t fEYY = 0;  ///< \<y*y> Mean value of y*y
        Double_t fEZZ = 0;  ///< \<z*z> Mean value of z*z
        Double_t fEXY = 0;  ///< \<x*y> Mean value of x*y
        Double_t fEYZ = 0;  ///< \<y*z> Mean value of y*z
        Double_t fEZX = 0;  ///< \<z*x> Mean value of z*y

        LKODRFitter *fODRFitter = nullptr; //!

    public:
        LKHitArray(Int_t size = 1000);
        virtual ~LKHitArray();

        LKHit *GetHit(Int_t idx) const;
        LKHit *GetLastHit() const;

        void SortByX(bool sortEarlierIfSmaller=true);
        void SortByY(bool sortEarlierIfSmaller=true);
        void SortByZ(bool sortEarlierIfSmaller=true);
        void SortByCharge(bool sortEarlierIfSmaller=true);
        void SortByDistanceTo(TVector3 point, bool sortEarlierIfCloser=true);
        void SortByLayer(bool sortEarlierIfSmaller=true);

        virtual void Clear(Option_t *option = "C");
        virtual void Print(Option_t *option = "at") const;
        virtual void Copy (TObject &object) const;

        void MoveHitsTo(LKHitArray *hitArray);

        void PrintHitIDs(Int_t rank) const;
        void PrintHits(Int_t rank) const;

        LKGeoLine FitLine();
        LKGeoPlane FitPlane();
        LKGeoCircle FitCircle(axis_t ref = LKVector3::kZ);
        LKGeoHelix FitHelix(axis_t ref = LKVector3::kZ);

        TCanvas *DrawFitCircle(axis_t ref = LKVector3::kZ);

        void AddHit(LKHit *hit);
        /// Fit is not possible if point is added through this method
        void AddXYZW(TVector3 pos, Double_t w=0);
        /// Fit is not possible if point is added through this method
        void AddXYZW(Double_t x, Double_t y, Double_t z, Double_t w=0);

        bool RemoveHit(LKHit *hit);
        bool RemoveHit(Int_t iHit);
        virtual void RemoveLastHit();

        /// Fit is not possible if point is removed through this method
        bool Subtract(TVector3 pos, Double_t w=0);
        /// Fit is not possible if point is removed through this method
        bool Subtract(Double_t x, Double_t y, Double_t z, Double_t w=0);

        vector<Int_t> *GetHitIDArray();

        Int_t GetNumHits() const;
        Int_t GetHitID(Int_t idx) const;

        Double_t GetW() const;
        Double_t GetChargeSum() const;

        Double_t GetMeanX() const;
        Double_t GetMeanY() const;
        Double_t GetMeanZ() const;

        Double_t GetMeanXX() const;
        Double_t GetMeanYY() const;
        Double_t GetMeanZZ() const;
        Double_t GetMeanXY() const;
        Double_t GetMeanYZ() const;
        Double_t GetMeanZX() const;

        Double_t GetVarianceX() const; ///< SUM_i (\<x>-x_i)^2
        Double_t GetVarianceY() const; ///< SUM_i (\<y>-y_i)^2
        Double_t GetVarianceZ() const; ///< SUM_i (\<z>-z_i)^2

        Double_t GetAXX() const; ///< W * SUM_i (\<x>-x_i)^2 : diagonal compoent of matrix A
        Double_t GetAYY() const; ///< W * SUM_i (\<y>-y_i)^2 : diagonal compoent of matrix A
        Double_t GetAZZ() const; ///< W * SUM_i (\<z>-z_i)^2 : diagonal compoent of matrix A
        Double_t GetAXY() const; ///< W * SUM_i (\<x>-x_i)*(\<y>-y_i) : off-diagonal compoent of matrix A
        Double_t GetAYZ() const; ///< W * SUM_i (\<y>-y_i)*(\<z>-z_i) : off-diagonal compoent of matrix A
        Double_t GetAZX() const; ///< W * SUM_i (\<z>-z_i)*(\<x>-x_i) : off-diagonal compoent of matrix A

        TVector3 GetMean()          const;
        TVector3 GetVariance()      const;
        TVector3 GetCovariance()    const;
        TVector3 GetStdDev()        const;
        TVector3 GetSquaredMean()   const;
        TVector3 GetCoSquaredMean() const;

        LKVector3 GetMean(axis_t)          const;
        LKVector3 GetVariance(axis_t)      const;
        LKVector3 GetCovariance(axis_t)    const;
        LKVector3 GetStdDev(axis_t)        const;
        LKVector3 GetSquaredMean(axis_t)   const;
        LKVector3 GetCoSquaredMean(axis_t) const;

        TGraphErrors *DrawGraph(axis_t,axis_t);


        ClassDef(LKHitArray, 1);
};

#endif
