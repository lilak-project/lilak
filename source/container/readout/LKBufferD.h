#ifndef LKBUFFERDOUBLE_HH
#define LKBUFFERDOUBLE_HH

#include "TObject.h"
#include "TH1D.h"
#include "TGraph.h"

/**
 * LKBufferD
 */

class LKBufferD : public TObject
{
    public:
        LKBufferD();
        virtual ~LKBufferD() {}
        void Clear(Option_t *option="");
        void Copy(TObject &object) const;
        void Print(Option_t *option="") const;

        void Draw(Option_t *option="");
        virtual void FillGraph(TGraph* graph);
        virtual void FillHist(TH1D* hist);
        virtual TGraph* GetGraph();
        virtual TH1D *GetHist(TString name="");

        bool IsEmpty() const { return fEmpty; }

        void SetBuffer(LKBufferD buffer);

        void SetArray(const int*          buffer) { fEmpty = false; for (int i=0; i<512; ++i) fArray[i] = double(buffer[i]); }
        void SetArray(const double*       buffer) { fEmpty = false; memcpy(fArray, buffer, sizeof(double)*512); }
        void SetArray(const unsigned int* buffer) { fEmpty = false; for (int i=0; i<512; ++i) fArray[i] = double(buffer[i]); }
        void SetArray(std::vector<unsigned int> buffer) { fEmpty = false; for (int i=0; i<512; ++i) fArray[i] = double(buffer[i]); }

        void Fill(int tb, double value) { fArray[tb] += value; }

        double* GetArray() { return fArray; }

        void SubtractArray(int*    buffer)               { for (int i=0; i<512; ++i) fArray[i] = fArray[i] - buffer[i]; }
        void SubtractArray(double* buffer)               { for (int i=0; i<512; ++i) fArray[i] = fArray[i] - buffer[i]; }
        void SubtractArray(int*    buffer, double scale) { for (int i=0; i<512; ++i) fArray[i] = fArray[i] - buffer[i]*scale; }
        void SubtractArray(double* buffer, double scale) { for (int i=0; i<512; ++i) fArray[i] = fArray[i] - buffer[i]*scale; }

        double GetScale(int* buffer); ///< return [scale] where [scale] is fit parameter of fArray = [scale] x buffer.
        double GetScale(double* buffer); ///< return [scale] where [scale] is fit parameter of fArray = [scale] x buffer.
        double CalculateGroupFluctuation(int numGroups=8, int tb2=512); ///< Divide 0-tb2 into numGroups. Calculate std-dev of group means.
        void   GetGroupMeanStdDev(int numGroups, int tb2, double* pedestalGroup, double* stddevGroup); ///< Divide 0-tb2 into numGroups. Evaluate pedestal(mean) and std-dev of each group.

    protected:
        bool fEmpty = true;
        double fArray[512];

        TH1D *fHist; //!
        TGraph *fGraph; //!

    ClassDef(LKBufferD,1);
};

#endif
